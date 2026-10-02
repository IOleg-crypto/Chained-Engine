#include "asset_dependency_collector.h"
#include "engine/core/log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Chained
{
	namespace
	{
		std::string StringToLower(std::string str)
		{
			std::transform(str.begin(), str.end(), str.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return str;
		}

		std::string NormalizeKey(const fs::path& p)
		{
			std::string s = p.generic_string();
			return StringToLower(s);
		}

		fs::path FindChproject(const fs::path& projectDir)
		{
			std::error_code ec;
			for (const auto& entry : fs::directory_iterator(projectDir, ec))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".chproject")
				{
					return entry.path();
				}
			}
			return {};
		}
	} // namespace

	bool AssetDependencyCollector::IsIgnoredFile(const fs::path& relPath)
	{
		const std::string lower = NormalizeKey(relPath);
		const std::string filename = relPath.filename().string();
		const std::string ext = StringToLower(relPath.extension().string());

		// Exclude 0-byte or dot files
		if (filename.empty() || filename.front() == '.')
		{
			return true;
		}

		// Exclude source code, IDE, VCS, build output directories
		static const std::vector<std::string> kIgnoredPathTokens = {"scripts/", ".idea/", ".vs/",		  ".vscode/",
																	".git/",	"obj/",	  "bin/",		  "debug/",
																	"release/", "x64/",	  "__pycache__/", "tests/"};
		for (const auto& token : kIgnoredPathTokens)
		{
			if (lower.find(token) != std::string::npos)
			{
				return true;
			}
		}

		// Exclude intermediate, metadata, or source code extensions
		static const std::unordered_set<std::string> kIgnoredExts = {
			".cs",	".csproj", ".sln",	".user", ".pdb", ".ilk",  ".obj",	   ".meta",	  ".chmeta", ".tmp",
			".bak", ".log",	   ".tlog", ".txt",	 ".md",	 ".orig", ".autosave", ".blend1", ".blend2"};
		if (kIgnoredExts.count(ext) > 0)
		{
			return true;
		}

		// Exclude test files and test scenes from pack
		std::string stem = StringToLower(relPath.stem().string());
		if (stem.find("test") != std::string::npos)
		{
			return true;
		}

		return false;
	}

	void AssetDependencyCollector::ScanDirectoryFiles(const fs::path& dir, std::vector<fs::path>& outRelativeFiles)
	{
		std::error_code ec;
		if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
		{
			return;
		}

		fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
		for (const auto& entry : it)
		{
			if (!entry.is_regular_file(ec))
			{
				continue;
			}

			// Skip empty files
			if (entry.file_size(ec) == 0)
			{
				continue;
			}

			auto rel = fs::relative(entry.path(), dir, ec);
			if (ec)
			{
				continue;
			}

			if (IsIgnoredFile(rel))
			{
				continue;
			}

			outRelativeFiles.push_back(std::move(rel));
		}
	}

	void AssetDependencyCollector::ScanTextForReferences(
		const fs::path& fullPath, const std::unordered_map<std::string, fs::path>& allAssetsLower,
		std::unordered_set<std::string>& referencedLower, std::vector<fs::path>& newReferences)
	{
		std::ifstream f(fullPath, std::ios::binary);
		if (!f.is_open())
		{
			return;
		}

		std::string line;
		while (std::getline(f, line))
		{
			// Extract potential candidate path after ':' or '-'
			std::string candidate;
			auto colonPos = line.find(':');
			if (colonPos != std::string::npos)
			{
				candidate = line.substr(colonPos + 1);
			}
			else
			{
				auto dashPos = line.find("- ");
				if (dashPos != std::string::npos)
				{
					candidate = line.substr(dashPos + 2);
				}
			}

			if (candidate.empty())
			{
				continue;
			}

			// Trim quotes, spaces, brackets
			size_t start = candidate.find_first_not_of(" \t\"'{}\r\n[]");
			if (start == std::string::npos)
			{
				continue;
			}
			size_t end = candidate.find_last_not_of(" \t\"'{}\r\n[]");
			candidate = candidate.substr(start, end - start + 1);

			if (candidate.empty() || candidate == "\"\"" || candidate == "{}")
			{
				continue;
			}

			std::string key = NormalizeKey(fs::path(candidate));

			// Strip leading assets/ prefix if present in candidate path
			if (key.starts_with("assets/"))
			{
				key = key.substr(7);
			}

			auto it = allAssetsLower.find(key);
			if (it != allAssetsLower.end())
			{
				if (referencedLower.insert(key).second)
				{
					newReferences.push_back(it->second);
				}
			}
		}
	}

	bool AssetDependencyCollector::Collect(const fs::path& projectDir, const fs::path& assetDir,
										   const fs::path& resourcesDir, std::vector<PackItem>& outItems)
	{
		outItems.clear();

		// 1. Locate .chproject file
		fs::path chprojectFile = FindChproject(projectDir);
		if (chprojectFile.empty())
		{
			CH_CORE_ERROR("AssetDependencyCollector: No .chproject found in '{}'", projectDir.string());
			return false;
		}
		outItems.push_back({chprojectFile, "project.chproject"});

		// 2. Index all available files in asset directory
		std::vector<fs::path> allAssetFiles;
		ScanDirectoryFiles(assetDir, allAssetFiles);

		std::unordered_map<std::string, fs::path> allAssetsLower;
		allAssetsLower.reserve(allAssetFiles.size());
		for (const auto& rel : allAssetFiles)
		{
			allAssetsLower.emplace(NormalizeKey(rel), rel);
		}

		// 3. Seed roots: all game scenes in scenes/ + shaders + icons
		std::unordered_set<std::string> referencedLower;
		std::vector<fs::path> workQueue;

		for (const auto& rel : allAssetFiles)
		{
			const std::string lower = NormalizeKey(rel);
			const std::string ext = StringToLower(rel.extension().string());

			// All scenes under scenes/ are valid maps/menus
			if (lower.starts_with("scenes/") && ext == ".chscene")
			{
				if (referencedLower.insert(lower).second)
				{
					workQueue.push_back(rel);
				}
			}
			// All prefabs under prefab/ or prefabs/ or *.chprefab
			else if (lower.starts_with("prefab/") || lower.starts_with("prefabs/") || ext == ".chprefab")
			{
				if (referencedLower.insert(lower).second)
				{
					workQueue.push_back(rel);
				}
			}
			// Shaders in asset directory
			else if (lower.starts_with("shaders/") || ext == ".chshader" || ext == ".vert" || ext == ".frag" ||
					 ext == ".glsl")
			{
				referencedLower.insert(lower);
			}
			// Core icons
			else if (lower.starts_with("icons/"))
			{
				referencedLower.insert(lower);
			}
		}

		// 4. Crawl dependency graph iteratively
		static const std::unordered_set<std::string> kModelExts = {
			".gltf", ".glb", ".fbx", ".obj", ".blend", ".dae", ".3ds", ".ply", ".stl", ".chasset", ".chmesh"};

		while (!workQueue.empty())
		{
			fs::path currentRel = std::move(workQueue.back());
			workQueue.pop_back();

			fs::path fullPath = assetDir / currentRel;
			const std::string ext = StringToLower(currentRel.extension().string());

			// Text-based files containing references
			if (ext == ".chscene" || ext == ".chmat" || ext == ".chenv" || ext == ".chag" || ext == ".json" ||
				ext == ".yaml" || ext == ".yml" || ext == ".chprefab")
			{
				std::vector<fs::path> newRefs;
				ScanTextForReferences(fullPath, allAssetsLower, referencedLower, newRefs);
				for (auto& ref : newRefs)
				{
					workQueue.push_back(std::move(ref));
				}
			}

			// Companion resolution for models
			if (kModelExts.count(ext) > 0)
			{
				// Check for binary .chasset companion
				fs::path chassetRel = currentRel;
				chassetRel.replace_extension(".chasset");
				std::string chassetKey = NormalizeKey(chassetRel);

				auto chassetIt = allAssetsLower.find(chassetKey);
				if (chassetIt != allAssetsLower.end())
				{
					if (referencedLower.insert(chassetKey).second)
					{
						workQueue.push_back(chassetIt->second);
					}
				}

				// Find companion materials (.chmat) in the same directory
				fs::path parentRel = currentRel.parent_path();
				for (const auto& [key, path] : allAssetsLower)
				{
					if (path.parent_path() == parentRel && StringToLower(path.extension().string()) == ".chmat")
					{
						if (referencedLower.insert(key).second)
						{
							workQueue.push_back(path);
						}
					}
				}
			}
		}

		// 5. Deduplication: When binary .chasset companion exists, strip raw .glb/.gltf/.bin
		std::unordered_set<std::string> strippedSources;
		for (const auto& key : referencedLower)
		{
			fs::path p(key);
			std::string ext = StringToLower(p.extension().string());

			if ((kModelExts.count(ext) > 0 && ext != ".chasset" && ext != ".chmesh") || ext == ".bin")
			{
				fs::path chassetPath = p;
				chassetPath.replace_extension(".chasset");
				if (allAssetsLower.count(NormalizeKey(chassetPath)) > 0)
				{
					strippedSources.insert(key);

					// Also strip potential companion scene.bin / stem.bin
					fs::path parent = p.parent_path();
					strippedSources.insert(NormalizeKey(parent / (p.stem().string() + ".bin")));
					strippedSources.insert(NormalizeKey(parent / "scene.bin"));
				}
			}
		}

		// 6. Add referenced asset items (excluding stripped raw model duplicates)
		size_t strippedBytes = 0;
		for (const auto& rel : allAssetFiles)
		{
			std::string key = NormalizeKey(rel);
			if (referencedLower.count(key) && !strippedSources.count(key))
			{
				outItems.push_back({assetDir / rel, fs::path("assets") / rel});
			}
			else if (strippedSources.count(key))
			{
				std::error_code szEc;
				strippedBytes += fs::file_size(assetDir / rel, szEc);
			}
		}

		// 7. Add core engine resources from resourcesDir
		std::vector<fs::path> resourceFiles;
		ScanDirectoryFiles(resourcesDir, resourceFiles);

		// Include essential shaders, icons, and primary UI fonts
		for (const auto& rel : resourceFiles)
		{
			std::string lower = NormalizeKey(rel);
			std::string ext = StringToLower(rel.extension().string());

			bool includeResource = false;
			if (lower.starts_with("shaders/") || lower.starts_with("config/") || lower.starts_with("icons/"))
			{
				includeResource = true;
			}
			// Include primary fonts only (skip 40+ redundant weights and license text)
			else if (lower.starts_with("font/") && (ext == ".ttf" || ext == ".otf"))
			{
				if (lower.find("variable") != std::string::npos || lower.find("fa-solid") != std::string::npos ||
					lower.find("regular") != std::string::npos)
				{
					includeResource = true;
				}
			}

			if (includeResource)
			{
				outItems.push_back({resourcesDir / rel, fs::path("resources") / rel});
			}
		}

		CH_CORE_INFO("AssetDependencyCollector: Collected {} pack items (active: {}/{} assets, stripped {:.1f} MB raw "
					 "duplicate models)",
					 outItems.size(), referencedLower.size() - strippedSources.size(), allAssetFiles.size(),
					 static_cast<double>(strippedBytes) / (1024.0 * 1024.0));

		return outItems.size() > 1;
	}
} // namespace Chained
