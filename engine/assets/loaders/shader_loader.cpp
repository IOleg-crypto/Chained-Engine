#include "engine/assets/loaders/shader_loader.h"

#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"

#include <regex>

#include "engine/assets/types/shader_asset.h"

namespace Chained
{
	std::shared_ptr<Asset> ShaderLoader::Create()
	{
		return std::make_shared<ShaderAsset>();
	}

	bool ShaderLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{
		auto shaderAsset = std::static_pointer_cast<ShaderAsset>(asset);
		auto shader = LoadShaderFromPath(resolvedPath);
		if (shader)
		{
			shaderAsset->SetShader(shader);
			return true;
		}
		if (outError)
		{
			*outError = "ShaderLoader: failed to load shader from '" + resolvedPath + "'";
		}
		return false;
	}

	std::shared_ptr<Shader> ShaderLoader::LoadShaderFromPath(const std::string& path)
	{
		std::filesystem::path absolutePath(path);

		auto* assetManager = ServiceLocator::TryGet<AssetManager>();

		std::string ext = absolutePath.extension().string();
		std::ranges::transform(ext, ext.begin(), ::tolower);

		if (ext == ".chshader")
		{
			try
			{
				std::string content = assetManager->ReadText(path);
				if (content.empty())
				{
					return nullptr;
				}

				YAML::Node config = YAML::Load(content);

				std::string vsRel = config["VertexShader"].as<std::string>();
				std::string fsRel = config["FragmentShader"].as<std::string>();

				std::filesystem::path basePath = absolutePath.parent_path();
				std::string vsPath = (basePath / vsRel).string();
				std::string fsPath = (basePath / fsRel).string();

				return LoadShaderFromPaths(vsPath, fsPath);
			} catch (...)
			{
				return nullptr;
			}
		}
		else if (ext == ".vs" || ext == ".vert" || ext == ".glsl")
		{
			std::filesystem::path fsPath = absolutePath;

			// Prefer the modern .frag extension; fall back to legacy .fs
			if (ext == ".vs")
			{
				fsPath.replace_extension(".fs");
				if (!assetManager->FileExists(fsPath.string()))
				{
					fsPath.replace_extension(".frag");
				}
			}
			else // .vert / .glsl  — try .frag first, then .fs for backwards compat
			{
				fsPath.replace_extension(".frag");
				if (!assetManager->FileExists(fsPath.string()))
				{
					fsPath.replace_extension(".fs");
				}
			}

			if (assetManager->FileExists(fsPath.string()))
			{
				return LoadShaderFromPaths(absolutePath.string(), fsPath.string());
			}
		}
		return nullptr;
	}

	std::shared_ptr<Shader> ShaderLoader::LoadShaderFromPaths(const std::string& vsPath, const std::string& fsPath)
	{
		std::vector<std::string> vsIncl, fsIncl;
		std::string vsSource = ProcessShaderSource(vsPath, vsIncl);
		std::string fsSource = ProcessShaderSource(fsPath, fsIncl);

		return Shader::Create(vsSource.c_str(), fsSource.c_str());
	}

	std::string ShaderLoader::ProcessShaderSource(const std::string& path, std::vector<std::string>& includedFiles)
	{
		std::filesystem::path fullPath = std::filesystem::weakly_canonical(path);

		for (const auto& included : includedFiles)
		{
			if (included == fullPath.string())
			{
				CH_CORE_WARN("ShaderPreprocessor: Circular include detected: {}", path);
				return "";
			}
		}
		includedFiles.push_back(fullPath.string());

		struct StackScope
		{
			std::vector<std::string>& stack;
			~StackScope() { if (!stack.empty()) stack.pop_back(); }
		} scope{includedFiles};

		auto* assetManager = ServiceLocator::TryGet<AssetManager>();

		// Read file content (pack or disk)
		std::string fileContent = assetManager->ReadText(fullPath.string());
		if (fileContent.empty())
		{
			CH_CORE_ERROR("ShaderPreprocessor: File not found: {}", path);
			return "";
		}

		// Process includes
		std::stringstream ss;
		std::regex includeRegex(R"(^\s*#include\s+["<](.*)[">])");
		std::smatch match;
		std::istringstream stream(fileContent);
		std::string line;

		while (std::getline(stream, line))
		{
			if (std::regex_search(line, match, includeRegex))
			{
				std::string includeFile = match[1].str();
				std::filesystem::path includePath = fullPath.parent_path() / includeFile;

				std::string includedSource = ProcessShaderSource(includePath.string(), includedFiles);
				if (includedSource.empty() && std::filesystem::exists(includePath))
				{
					CH_CORE_WARN("ShaderPreprocessor: Failed to process include: {}", includeFile);
				}
				ss << includedSource << "\n";
			}
			else
			{
				ss << line << "\n";
			}
		}

		return ss.str();
	}
} // namespace Chained