#include "engine/assets/asset_path_resolver.h"
#include "engine/common/platform_detection.h"

#include <algorithm>
#include <cctype>

namespace Chained
{

	namespace
	{
		static void NormalizeSlashes(std::string& s)
		{
			std::replace(s.begin(), s.end(), '\\', '/');
		}

		static void StripPrefix(std::string& input, const std::filesystem::path& base)
		{
			if (base.empty())
			{
				return;
			}
			std::string basePath = base.generic_string();
			if (!basePath.empty() && basePath.back() != '/')
			{
				basePath += '/';
			}
			if (input.size() >= basePath.size() && input.compare(0, basePath.size(), basePath) == 0)
			{
				input = input.substr(basePath.size());
			}
		}

#if CH_PLATFORM_WINDOWS
		static bool IsWindowsAbsolutePath(const std::string& path)
		{
			if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
				(path[2] == '/' || path[2] == '\\'))
			{
				return true;
			}
			if (path.size() >= 2 && (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\'))
			{
				return true; // UNC network path
			}
			return false;
		}

		static std::string SanitizeForeignWindowsPath(const std::string& cleanPath)
		{
			static const std::vector<std::string> markers = {"/assets/", "/resources/", "/game/"};
			for (const auto& marker : markers)
			{
				size_t pos = cleanPath.find(marker);
				if (pos != std::string::npos)
				{
					if (marker == "/assets/" || marker == "/resources/")
					{
						return cleanPath.substr(pos + 1);
					}
					else if (marker == "/game/")
					{
						size_t assetsPos = cleanPath.find("/assets/", pos);
						if (assetsPos != std::string::npos)
						{
							return cleanPath.substr(assetsPos + 8);
						}
					}
				}
			}
			// Fallback: strip drive letter "X:"
			if (cleanPath.size() >= 2 && cleanPath[1] == ':')
			{
				std::string stripped = cleanPath.substr(2);
				while (!stripped.empty() && (stripped.front() == '/' || stripped.front() == '\\'))
				{
					stripped.erase(stripped.begin());
				}
				return stripped;
			}
			return cleanPath;
		}
#else
		static bool IsWindowsDrivePath(const std::string& path)
		{
			return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':';
		}

		static std::string SanitizeWindowsPathOnPosix(const std::string& cleanPath)
		{
			static const std::vector<std::string> markers = {"/assets/", "/resources/", "/game/"};
			for (const auto& marker : markers)
			{
				size_t pos = cleanPath.find(marker);
				if (pos != std::string::npos)
				{
					if (marker == "/assets/" || marker == "/resources/")
					{
						return cleanPath.substr(pos + 1);
					}
					else if (marker == "/game/")
					{
						size_t assetsPos = cleanPath.find("/assets/", pos);
						if (assetsPos != std::string::npos)
						{
							return cleanPath.substr(assetsPos + 8);
						}
					}
				}
			}
			// Fallback: strip drive letter "X:"
			std::string stripped = cleanPath.substr(2);
			while (!stripped.empty() && (stripped.front() == '/' || stripped.front() == '\\'))
			{
				stripped.erase(stripped.begin());
			}
			return stripped;
		}
#endif
	} // namespace

	std::string AssetPathResolver::ResolvePackKey(const std::string& assetPath) const
	{
		std::string packKey = assetPath;
		NormalizeSlashes(packKey);
		StripPrefix(packKey, m_EngineRoot);

		// Strip the "engine/" virtual prefix — pack stores engine resources
		// under "resources/" directly, not "engine/resources/".
		if (packKey.rfind("engine/", 0) == 0)
		{
			packKey = packKey.substr(7);
		}

		StripPrefix(packKey, m_ProjectDirectory);

		packKey = std::filesystem::path(packKey).lexically_normal().generic_string();
		while (!packKey.empty() && (packKey.front() == '/' || packKey.front() == '\\'))
		{
			packKey.erase(packKey.begin());
		}
		return packKey;
	}

	std::string AssetPathResolver::ResolvePath(const std::string& path) const
	{
		if (path.empty())
		{
			return "";
		}

		{
			std::lock_guard<std::mutex> lock(m_PathMutex);
			if (auto it = m_PathCache.find(path); it != m_PathCache.end())
			{
				return it->second;
			}
		}

		std::string cleanPath = path;
		NormalizeSlashes(cleanPath);

#if CH_PLATFORM_WINDOWS
		if (IsWindowsAbsolutePath(cleanPath))
		{
			std::filesystem::path winPath(cleanPath);
			std::error_code ec;
			if (std::filesystem::exists(winPath, ec))
			{
				std::string resolved = std::filesystem::absolute(winPath).lexically_normal().generic_string();
				std::lock_guard<std::mutex> lock(m_PathMutex);
				m_PathCache[path] = resolved;
				return resolved;
			}
			cleanPath = SanitizeForeignWindowsPath(cleanPath);
		}
#else
		if (IsWindowsDrivePath(cleanPath))
		{
			cleanPath = SanitizeWindowsPathOnPosix(cleanPath);
		}
		else
		{
			std::filesystem::path posixPath(cleanPath);
			std::error_code ec;
			if (posixPath.is_absolute())
			{
				if (std::filesystem::exists(posixPath, ec))
				{
					std::string resolved = std::filesystem::absolute(posixPath).lexically_normal().generic_string();
					std::lock_guard<std::mutex> lock(m_PathMutex);
					m_PathCache[path] = resolved;
					return resolved;
				}
				static const std::vector<std::string> markers = {"/assets/", "/resources/", "/game/"};
				for (const auto& marker : markers)
				{
					size_t pos = cleanPath.find(marker);
					if (pos != std::string::npos)
					{
						if (marker == "/assets/" || marker == "/resources/")
						{
							cleanPath = cleanPath.substr(pos + 1);
						}
						else if (marker == "/game/")
						{
							size_t assetsPos = cleanPath.find("/assets/", pos);
							if (assetsPos != std::string::npos)
							{
								cleanPath = cleanPath.substr(assetsPos + 8);
							}
						}
						break;
					}
				}
			}
		}
#endif

		std::filesystem::path inputPath(cleanPath);
		std::filesystem::path resolvedPath;

		if (inputPath.is_absolute())
		{
			resolvedPath = inputPath;
		}
		else
		{
			std::string pathStr = inputPath.generic_string();
			bool isEngineResource = false;
			if (pathStr.rfind("engine/", 0) == 0)
			{
				pathStr = pathStr.substr(7);
				isEngineResource = true;
			}

			if (isEngineResource)
			{
				if (!m_EngineRoot.empty())
				{
					std::filesystem::path candidate = m_EngineRoot / pathStr;
					if (std::filesystem::exists(candidate))
					{
						resolvedPath = candidate;
					}
				}

				// Fallback: source resources directory (dev mode)
				if (resolvedPath.empty() && !m_SourceResourcesDir.empty())
				{
					std::filesystem::path candidate = m_SourceResourcesDir / pathStr;
					if (std::filesystem::exists(candidate))
					{
						resolvedPath = candidate;
					}
				}
			}
			else
			{
				if (pathStr.rfind("assets/", 0) == 0)
				{
					std::string withoutAssets = pathStr.substr(7);
					if (!m_AssetDirectory.empty())
					{
						std::filesystem::path candidate = m_AssetDirectory / withoutAssets;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					if (resolvedPath.empty() && !m_ProjectDirectory.empty())
					{
						std::filesystem::path candidate = m_ProjectDirectory / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					if (resolvedPath.empty() && !m_SourceAssetsDir.empty())
					{
						std::filesystem::path candidate = m_SourceAssetsDir / withoutAssets;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}
				}
				else
				{
					if (!m_AssetDirectory.empty())
					{
						std::filesystem::path candidate = m_AssetDirectory / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					if (resolvedPath.empty() && !m_ProjectDirectory.empty())
					{
						std::filesystem::path candidate = m_ProjectDirectory / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					if (resolvedPath.empty() && !m_ProjectDirectory.empty())
					{
						std::filesystem::path candidate = m_ProjectDirectory / "assets" / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					// Fallback: source assets directory (dev mode)
					if (resolvedPath.empty() && !m_SourceAssetsDir.empty())
					{
						std::filesystem::path candidate = m_SourceAssetsDir / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					// Fallback: source resources directory for "resources/..." paths (dev mode)
					if (resolvedPath.empty() && !m_SourceResourcesDir.empty())
					{
						std::filesystem::path candidate = m_SourceResourcesDir / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}

					if (resolvedPath.empty() && !m_EngineRoot.empty())
					{
						std::filesystem::path candidate = m_EngineRoot / pathStr;
						if (std::filesystem::exists(candidate))
						{
							resolvedPath = candidate;
						}
					}
				}
			}

			if (resolvedPath.empty())
			{
				if (isEngineResource)
				{
					resolvedPath = m_EngineRoot.empty() ? std::filesystem::path(pathStr) : (m_EngineRoot / pathStr);
				}
				else if (!m_AssetDirectory.empty())
				{
					if (pathStr.rfind("assets/", 0) == 0)
					{
						resolvedPath = m_AssetDirectory / pathStr.substr(7);
					}
					else
					{
						resolvedPath = m_AssetDirectory / pathStr;
					}
				}
				else if (!m_ProjectDirectory.empty())
				{
					resolvedPath = m_ProjectDirectory / pathStr;
				}
				else
				{
					resolvedPath = std::filesystem::path(pathStr);
				}
			}
		}

		std::string resolved = std::filesystem::absolute(resolvedPath).lexically_normal().generic_string();

		std::lock_guard<std::mutex> lock(m_PathMutex);
		m_PathCache[path] = resolved;
		return resolved;
	}

	void AssetPathResolver::RegisterHandle(const std::string& path, AssetHandle handle)
	{
		std::string resolved = ResolvePath(path);
		std::lock_guard<std::mutex> lock(m_PathMutex);
		if (!resolved.empty())
		{
			m_PathToHandle[resolved] = handle;
		}
		if (!path.empty() && path != resolved)
		{
			m_PathToHandle[path] = handle;
		}
	}

	void AssetPathResolver::UnregisterHandle(AssetHandle handle)
	{
		std::lock_guard<std::mutex> lock(m_PathMutex);
		for (auto it = m_PathToHandle.begin(); it != m_PathToHandle.end();)
		{
			if (it->second == handle)
			{
				it = m_PathToHandle.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	AssetHandle AssetPathResolver::ResolveToHandle(const std::string& path) const
	{
		if (path.empty())
		{
			return AssetHandle(0);
		}
		std::string resolved = ResolvePath(path);
		std::lock_guard<std::mutex> lock(m_PathMutex);
		if (auto it = m_PathToHandle.find(resolved); it != m_PathToHandle.end())
		{
			return it->second;
		}
		if (auto it = m_PathToHandle.find(path); it != m_PathToHandle.end())
		{
			return it->second;
		}
		return AssetHandle(0);
	}

	void AssetPathResolver::ResetPath(const std::string& path)
	{
		std::lock_guard<std::mutex> lock(m_PathMutex);
		m_PathCache.erase(path);
	}

	void AssetPathResolver::ClearCache()
	{
		std::lock_guard<std::mutex> lock(m_PathMutex);
		m_PathCache.clear();
		m_PathToHandle.clear();
	}

} // namespace Chained
