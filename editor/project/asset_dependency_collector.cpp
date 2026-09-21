#include "asset_dependency_collector.h"
#include "project_serializer.h"
#include "engine/core/log.h"
#include "engine/project/project.h"
#include "engine/assets/loaders/model_loader.h"
#include <memory>

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

		// Exclude source code, IDE, VCS, build output directories, and editor-only folders
		static const std::vector<std::string> kIgnoredPathTokens = {
			"scripts/", ".idea/",	".vs/", ".vscode/",		".git/",  "obj/",		  "bin/",
			"debug/",	"release/", "x64/", "__pycache__/", "tests/", "map_previews/"};
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
		std::unordered_set<std::string>& referencedLower, std::vector<fs::path>& newReferences,
		const std::unordered_set<std::string>& excludedKeys)
	{
		std::ifstream f(fullPath, std::ios::binary);
		if (!f.is_open())
		{
			return;
		}

		auto tryAddCandidate = [&](std::string candidate) {
			if (candidate.empty())
			{
				return;
			}

			// Trim quotes, spaces, brackets
			size_t start = candidate.find_first_not_of(" \t\"'{}\r\n[]");
			if (start == std::string::npos)
			{
				return;
			}
			size_t end = candidate.find_last_not_of(" \t\"'{}\r\n[]");
			candidate = candidate.substr(start, end - start + 1);

			if (candidate.empty() || candidate == "\"\"" || candidate == "{}")
			{
				return;
			}

			std::string key = NormalizeKey(fs::path(candidate));

			// Strip leading assets/ prefix if present in candidate path
			if (key.starts_with("assets/"))
			{
				key = key.substr(7);
			}

			if (excludedKeys.count(key))
			{
				return;
			}

			auto it = allAssetsLower.find(key);
			if (it != allAssetsLower.end())
			{
				if (referencedLower.insert(key).second)
				{
					newReferences.push_back(it->second);
				}
			}
		};

		std::string line;
		while (std::getline(f, line))
		{
			// 1. Extract potential candidate path after ':' or '-'
			auto colonPos = line.find(':');
			if (colonPos != std::string::npos)
			{
				tryAddCandidate(line.substr(colonPos + 1));
			}
			else
			{
				auto dashPos = line.find("- ");
				if (dashPos != std::string::npos)
				{
					tryAddCandidate(line.substr(dashPos + 2));
				}
			}

			// 2. Also extract all quoted string literals ("path" or 'path') in the line (handles C# scripts, JSON,
			// YAML)
			for (char quote : {'"', '\''})
			{
				size_t qStart = 0;
				while ((qStart = line.find(quote, qStart)) != std::string::npos)
				{
					size_t qEnd = line.find(quote, qStart + 1);
					if (qEnd == std::string::npos)
					{
						break;
					}
					tryAddCandidate(line.substr(qStart + 1, qEnd - qStart - 1));
					qStart = qEnd + 1;
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

		// 3. Seed roots:
		std::unordered_set<std::string> referencedLower;
		std::vector<fs::path> workQueue;

		std::shared_ptr<Project> dummyProject = std::make_shared<Project>();
		EditorProjectSerializer::Deserialize(dummyProject, chprojectFile);
		const auto& projConfig = dummyProject->GetConfig();

		std::unordered_set<std::string> excludedScenesLower;
		for (const auto& exc : projConfig.Export.ExcludedScenes)
		{
			std::string key = NormalizeKey(fs::path(exc));
			if (key.starts_with("assets/"))
			{
				key = key.substr(7);
			}
			excludedScenesLower.insert(key);
		}

		// A. Seed from StartScene only (not ActiveScene — that is the editor's last open scene,
		//    not necessarily part of the game's reachable graph).
		{
			std::ifstream f(chprojectFile);
			std::string line;
			while (std::getline(f, line))
			{
				const std::string trimmed = [&line]() {
					size_t s = line.find_first_not_of(" \t");
					return s == std::string::npos ? std::string{} : line.substr(s);
				}();
				if (trimmed.rfind("StartScene:", 0) == 0)
				{
					const std::string val = trimmed.substr(11);
					std::vector<fs::path> refs;
					// Re-use the single-candidate path via a tiny helper:
					auto tryOne = [&](std::string candidate) {
						size_t s = candidate.find_first_not_of(" \t\"'");
						if (s == std::string::npos)
						{
							return;
						}
						size_t e = candidate.find_last_not_of(" \t\"'\r\n");
						candidate = candidate.substr(s, e - s + 1);
						if (candidate.empty())
						{
							return;
						}
						std::string key = NormalizeKey(fs::path(candidate));
						if (key.starts_with("assets/"))
						{
							key = key.substr(7);
						}
						if (excludedScenesLower.count(key))
						{
							return;
						}
						auto it2 = allAssetsLower.find(key);
						if (it2 != allAssetsLower.end() && referencedLower.insert(key).second)
						{
							workQueue.push_back(it2->second);
						}
					};
					tryOne(val);
					break;
				}
			}
		}

		// B. Seed from all C# scripts in assets/
		//    Only non-scene assets (textures, models, audio, etc.) are seeded from scripts.
		//    .chscene paths in C# scripts are intentionally skipped here — scenes can only enter
		//    the pack through the .chscene dependency graph (TargetScenePath, etc.) starting
		//    from StartScene, preventing default-value scene fields from pulling in dead scenes.
		for (const auto& rel : allAssetFiles)
		{
			const std::string lower = NormalizeKey(rel);
			const std::string ext = StringToLower(rel.extension().string());

			if (ext == ".cs")
			{
				referencedLower.insert(lower);
				std::vector<fs::path> scriptRefs;
				ScanTextForReferences(assetDir / rel, allAssetsLower, referencedLower, scriptRefs, excludedScenesLower);
				for (auto& ref : scriptRefs)
				{
					// Scenes found in C# scripts are not seeded directly.
					// They are only pulled in when a .chscene in the graph references them.
					if (StringToLower(ref.extension().string()) == ".chscene")
					{
						continue;
					}
					workQueue.push_back(std::move(ref));
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
				ext == ".yaml" || ext == ".yml" || ext == ".chprefab" || ext == ".cs")
			{
				std::vector<fs::path> newRefs;
				ScanTextForReferences(fullPath, allAssetsLower, referencedLower, newRefs, excludedScenesLower);
				for (auto& ref : newRefs)
				{
					workQueue.push_back(std::move(ref));
				}
			}
			else if (ext == ".chasset")
			{
				// Binary model cache: must be deserialized to discover extracted texture references
				auto pendingData = ModelLoader::LoadMeshDataFromDisk(fullPath);
				if (pendingData.isValid)
				{
					auto tryAddTexture = [&](const std::string& texPath) {
						if (texPath.empty() || texPath.front() == '*')
						{
							return;
						}
						std::string key = NormalizeKey(fs::path(texPath));
						if (key.starts_with("assets/"))
						{
							key = key.substr(7);
						}

						// If AssimpImporter just extracted this texture, it might not be in the initial scan
						if (allAssetsLower.find(key) == allAssetsLower.end() && fs::exists(assetDir / key))
						{
							fs::path relPath(key);
							allAssetFiles.push_back(relPath);
							allAssetsLower[key] = relPath;
						}

						auto it = allAssetsLower.find(key);
						if (it != allAssetsLower.end() && referencedLower.insert(key).second)
						{
							workQueue.push_back(it->second);
						}
					};

					for (const auto& mat : pendingData.materials)
					{
						tryAddTexture(mat.albedoPath);
						tryAddTexture(mat.normalPath);
						tryAddTexture(mat.emissivePath);
						tryAddTexture(mat.metallicRoughnessPath);
						tryAddTexture(mat.occlusionPath);
					}
				}
			}

			// Companion resolution for models
			if (kModelExts.count(ext) > 0 && ext != ".chasset" && ext != ".chmesh")
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
				else
				{
					// Force compile if .chasset is missing! This extracts textures and saves the binary cache.
					CH_CORE_INFO("AssetDependencyCollector: Force compiling {} to generate .chasset...",
								 currentRel.string());
					auto data = ModelLoader::LoadMeshDataFromDisk(fullPath);
					if (data.isValid && fs::exists(assetDir / chassetRel))
					{
						allAssetFiles.push_back(chassetRel);
						allAssetsLower[chassetKey] = chassetRel;

						if (referencedLower.insert(chassetKey).second)
						{
							workQueue.push_back(chassetRel);
						}

						// Scan for freshly-created _embtex_ companion files written during
						// the force compile above, so step 5a can find and include them.
						std::string chassetStem = StringToLower(chassetRel.stem().string());
						std::string chassetParent = NormalizeKey(chassetRel.parent_path());
						std::string embtexPrefix = chassetStem + "_embtex_";
						std::error_code ecScan;
						fs::path chassetDir = assetDir / chassetRel.parent_path();
						for (const auto& de : fs::directory_iterator(chassetDir, ecScan))
						{
							if (!de.is_regular_file())
							{
								continue;
							}
							std::string fname = StringToLower(de.path().filename().string());
							if (fname.rfind(embtexPrefix, 0) == 0)
							{
								fs::path relEmbtex = chassetRel.parent_path() / de.path().filename();
								std::string embtexKey = NormalizeKey(relEmbtex);
								if (!allAssetsLower.count(embtexKey))
								{
									allAssetFiles.push_back(relEmbtex);
									allAssetsLower[embtexKey] = relEmbtex;
								}
							}
						}
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

		// 5. Deduplication: strip raw .glb/.gltf/.bin files when any .chasset exists in the
		//    same directory — covers cases where .chasset has a different stem than the source
		//    model (e.g. doric_arena.glb compiled into arena.chasset, toy_train.glb -> train_vagon.chasset).
		std::unordered_set<std::string> strippedSources;

		// Build a set of directories that contain at least one .chasset
		std::unordered_set<std::string> dirsWithChAsset;
		for (const auto& [key, rel] : allAssetsLower)
		{
			if (StringToLower(fs::path(key).extension().string()) == ".chasset")
			{
				dirsWithChAsset.insert(NormalizeKey(fs::path(key).parent_path()));
			}
		}

		for (const auto& key : referencedLower)
		{
			fs::path p(key);
			std::string ext = StringToLower(p.extension().string());

			// Strip raw model source files and loose .bin buffers when the folder
			// already has a compiled .chasset — exact-stem OR any chasset in that dir.
			if ((kModelExts.count(ext) > 0 && ext != ".chasset" && ext != ".chmesh") || ext == ".bin")
			{
				std::string parentKey = NormalizeKey(p.parent_path());

				// Exact-stem match (original logic — fast path)
				fs::path exactChasset = p;
				exactChasset.replace_extension(".chasset");
				bool hasExact = allAssetsLower.count(NormalizeKey(exactChasset)) > 0;

				// Folder-level match: any .chasset in same directory
				bool hasFolderChasset = dirsWithChAsset.count(parentKey) > 0;

				if (hasExact || hasFolderChasset)
				{
					strippedSources.insert(key);

					// Also strip companion scene.bin / stem.bin
					fs::path parent = p.parent_path();
					strippedSources.insert(NormalizeKey(parent / (p.stem().string() + ".bin")));
					strippedSources.insert(NormalizeKey(parent / "scene.bin"));
				}
			}
		}

		// 5a. Include extracted embedded texture companions for referenced .chasset files.
		//     When chasset saves extract JPEG/PNG from GLB embedded textures to _embtex_ files,
		//     these need to be packed alongside the chasset so the runtime can load them.
		{
			std::vector<std::string> companionKeys;
			for (const auto& key : referencedLower)
			{
				fs::path p(key);
				if (StringToLower(p.extension().string()) != ".chasset")
				{
					continue;
				}

				std::string stem = StringToLower(p.stem().string());
				std::string parentDir = NormalizeKey(p.parent_path());
				std::string prefix = parentDir.empty() ? (stem + "_embtex_") : (parentDir + "/" + stem + "_embtex_");

				for (const auto& [assetKey, assetRel] : allAssetsLower)
				{
					if (assetKey.size() > prefix.size() && assetKey.compare(0, prefix.size(), prefix) == 0)
					{
						companionKeys.push_back(assetKey);
					}
				}
			}
			for (const auto& k : companionKeys)
			{
				if (referencedLower.insert(k).second)
				{
					CH_CORE_TRACE("AssetDependencyCollector: Including companion texture '{}'", k);
				}
			}
		}

		// 6. Add referenced asset items (excluding stripped raw model duplicates)
		size_t strippedBytes = 0;
		std::unordered_set<std::string> addedCanonicalSources; // dedup by real path on disk
		for (const auto& rel : allAssetFiles)
		{
			std::string key = NormalizeKey(rel);
			if (referencedLower.count(key) && !strippedSources.count(key))
			{
				// Canonicalise the source path so that symlinks / ".." traversals
				// that resolve to the same physical file are not packed twice.
				std::error_code ecCan;
				fs::path srcPath = assetDir / rel;
				fs::path canonical = fs::weakly_canonical(srcPath, ecCan);
				std::string canonKey = ecCan ? srcPath.string() : canonical.string();

				if (addedCanonicalSources.insert(canonKey).second)
				{
					outItems.push_back({srcPath, fs::path("assets") / rel});
				}
				else
				{
					CH_CORE_TRACE(
						"AssetDependencyCollector: Skipping duplicate source '{}' (already added via different path)",
						srcPath.string());
				}
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
