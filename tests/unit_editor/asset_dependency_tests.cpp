#include <gtest/gtest.h>

#include "editor/project/asset_dependency_collector.h"
#include <filesystem>
#include <fstream>

using namespace Chained;
namespace fs = std::filesystem;

TEST(AssetDependencyCollectorTest, IsIgnoredFileRules)
{
	// Dot files & hidden files
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile(".gitignore"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile(".DS_Store"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("textures/.hidden.png"));

	// VCS and IDE paths
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile(".git/config"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile(".vs/Project/v17/.suo"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile(".vscode/settings.json"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("bin/Debug/game.exe"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("obj/Debug/game.pdb"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("scripts/PlayerController.cs"));

	// Ignored file extensions
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("Player.cs"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("Project.csproj"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("Game.sln"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("output.log"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("character.blend1"));
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("README.md"));

	// Test files
	EXPECT_TRUE(AssetDependencyCollector::IsIgnoredFile("test_scene.chscene"));

	// Valid game assets that must NOT be ignored
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("models/character.glb"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("models/character.chasset"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("textures/grass_albedo.png"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("textures/grass_normal.ktx2"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("audio/theme.wav"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("audio/explosion.ogg"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("scenes/level1.chscene"));
	EXPECT_FALSE(AssetDependencyCollector::IsIgnoredFile("materials/brick.chmat"));
}

TEST(AssetDependencyCollectorTest, ScanTextForReferences)
{
	fs::path tempFile = fs::temp_directory_path() / "temp_scene_test.yaml";
	{
		std::ofstream ofs(tempFile);
		ofs << "Scene: Main\n";
		ofs << "  Model: assets/models/tree.glb\n";
		ofs << "  Texture: assets/textures/bark.png\n";
		ofs << "  Sound: assets/audio/wind.ogg\n";
	}

	std::unordered_map<std::string, fs::path> allAssetsLower;
	allAssetsLower["models/tree.glb"] = "models/tree.glb";
	allAssetsLower["textures/bark.png"] = "textures/bark.png";
	allAssetsLower["audio/wind.ogg"] = "audio/wind.ogg";
	allAssetsLower["unused/unused.png"] = "unused/unused.png";

	std::unordered_set<std::string> referencedLower;
	std::vector<fs::path> newReferences;

	AssetDependencyCollector::ScanTextForReferences(tempFile, allAssetsLower, referencedLower, newReferences, {});

	EXPECT_EQ(referencedLower.size(), 3u);
	EXPECT_EQ(newReferences.size(), 3u);
	EXPECT_TRUE(referencedLower.count("models/tree.glb") > 0);
	EXPECT_TRUE(referencedLower.count("textures/bark.png") > 0);
	EXPECT_TRUE(referencedLower.count("audio/wind.ogg") > 0);
	EXPECT_FALSE(referencedLower.count("unused/unused.png") > 0);

	std::error_code ec;
	fs::remove(tempFile, ec);
}
