// project_tests.cpp
// Consolidated scenario-based unit tests for Chained::Project and ProjectConfig.
// Follows Arrange-Act-Assert (AAA) pattern with Google Test.

#include "engine/project/project.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace Chained;

// ============================================================================
// 1. DEFAULT CONFIGURATION & FIELD SANITY TESTS
// ============================================================================
TEST(ProjectModule, Config_DefaultValuesSanity)
{
	// Arrange & Act
	Project project;
	const auto& config = project.GetConfig();

	// Assert: Core metadata
	EXPECT_EQ(config.Name, "Untitled");
	EXPECT_EQ(config.AssetDirectory, "assets");
	EXPECT_TRUE(config.StartScene.empty());
	EXPECT_TRUE(config.IconPath.empty());
	EXPECT_TRUE(config.ActiveScenePath.empty());
	EXPECT_EQ(config.BuildConfig, Configuration::Debug);

	// Assert: Sub-systems default settings
	EXPECT_FLOAT_EQ(config.Physics.Gravity, 20.0f);
	EXPECT_FLOAT_EQ(config.Physics.FixedTimestep, 1.0f / 60.0f);
	EXPECT_FLOAT_EQ(config.Animation.TargetFPS, 30.0f);
	EXPECT_EQ(config.Render.ShadowResolution, 2048);
	EXPECT_TRUE(config.Render.EnableShadows);
	EXPECT_EQ(config.Render.AntiAliasingSamples, 4);
	EXPECT_TRUE(config.Mesh.ImportMaterials);
	EXPECT_TRUE(config.Mesh.CalculateTangents);
	EXPECT_TRUE(config.Mesh.FlipUVs);
	EXPECT_FALSE(config.Runtime.Fullscreen);
	EXPECT_TRUE(config.Runtime.ShowStats);
	EXPECT_FALSE(config.Runtime.EnableConsole);
	EXPECT_EQ(config.Runtime.TargetFPS, 0); // 0 = uncapped
	EXPECT_FLOAT_EQ(config.Audio.MasterVolume, 1.0f);
	EXPECT_FLOAT_EQ(config.Audio.MusicVolume, 1.0f);
	EXPECT_FLOAT_EQ(config.Audio.SFXVolume, 1.0f);
	EXPECT_TRUE(config.Scripting.AutoLoad);
	EXPECT_EQ(config.Export.Mode, PackMode::Balanced);
	EXPECT_EQ(config.Export.PackName, "resources");
}

// ============================================================================
// 2. SETTERS, CLAMPING & VALIDATION BOUNDARIES
// ============================================================================
TEST(ProjectModule, Setters_ClampingAndBoundaryValidations)
{
	// Arrange
	Project project;

	// Act & Assert: Physics (Gravity >= 0.0, FixedTimestep >= 0.001)
	project.SetPhysicsGravity(9.81f);
	EXPECT_FLOAT_EQ(project.GetConfig().Physics.Gravity, 9.81f);
	project.SetPhysicsGravity(-15.0f);
	EXPECT_FLOAT_EQ(project.GetConfig().Physics.Gravity, 0.0f); // Clamped to 0.0

	project.SetPhysicsFixedTimestep(0.02f);
	EXPECT_FLOAT_EQ(project.GetConfig().Physics.FixedTimestep, 0.02f);
	project.SetPhysicsFixedTimestep(-1.0f);
	EXPECT_FLOAT_EQ(project.GetConfig().Physics.FixedTimestep, 0.001f); // Clamped to min 0.001

	// Act & Assert: Rendering (ShadowResolution [256, 8192], AntiAliasing [0, 16])
	project.SetShadowResolution(4096);
	EXPECT_EQ(project.GetConfig().Render.ShadowResolution, 4096);
	project.SetShadowResolution(64);
	EXPECT_EQ(project.GetConfig().Render.ShadowResolution, 256);
	project.SetShadowResolution(16384);
	EXPECT_EQ(project.GetConfig().Render.ShadowResolution, 8192);

	project.SetAntiAliasingSamples(8);
	EXPECT_EQ(project.GetAntiAliasingSamples(), 8);
	project.SetAntiAliasingSamples(-2);
	EXPECT_EQ(project.GetAntiAliasingSamples(), 0);
	project.SetAntiAliasingSamples(32);
	EXPECT_EQ(project.GetAntiAliasingSamples(), 16);

	// Act & Assert: Target FPS (>= 0)
	project.SetTargetFPS(144);
	EXPECT_EQ(project.GetConfig().Runtime.TargetFPS, 144);
	project.SetTargetFPS(-60);
	EXPECT_EQ(project.GetConfig().Runtime.TargetFPS, 0);

	// Act & Assert: Audio Volumes ([0.0, 1.0])
	project.SetMasterVolume(0.75f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.MasterVolume, 0.75f);
	project.SetMasterVolume(-0.5f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.MasterVolume, 0.0f);
	project.SetMasterVolume(1.8f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.MasterVolume, 1.0f);

	project.SetMusicVolume(-1.0f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.MusicVolume, 0.0f);
	project.SetMusicVolume(2.0f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.MusicVolume, 1.0f);

	project.SetSFXVolume(-0.1f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.SFXVolume, 0.0f);
	project.SetSFXVolume(1.5f);
	EXPECT_FLOAT_EQ(project.GetConfig().Audio.SFXVolume, 1.0f);

	// Act & Assert: General and Scripting settings
	project.SetName("ChainedQuest");
	EXPECT_EQ(project.GetName(), "ChainedQuest");

	project.SetStartScene("scenes/Main.chscene");
	EXPECT_EQ(project.GetStartScene(), "scenes/Main.chscene");

	project.SetActiveScenePath("scenes/Level1.chscene");
	EXPECT_EQ(project.GetActiveScenePath(), "scenes/Level1.chscene");

	project.SetScripting("GameplayModule", "bin/scripts", false);
	EXPECT_EQ(project.GetConfig().Scripting.ModuleName, "GameplayModule");
	EXPECT_EQ(project.GetConfig().Scripting.ModuleDirectory, "bin/scripts");
	EXPECT_FALSE(project.GetConfig().Scripting.AutoLoad);
}

// ============================================================================
// 3. ACTIVE PROJECT SINGLETON & THREAD-SAFETY
// ============================================================================
TEST(ProjectModule, ActiveProject_LifecycleAndConcurrency)
{
	// Arrange
	Project::SetActive(nullptr);
	EXPECT_EQ(Project::GetActive(), nullptr);

	// Act & Assert: Basic setting and retrieval
	auto projA = std::make_shared<Project>();
	projA->SetName("ProjectAlpha");
	Project::SetActive(projA);

	EXPECT_EQ(Project::GetActive(), projA);
	EXPECT_EQ(Project::GetActive()->GetName(), "ProjectAlpha");

	// Act: Reassignment
	auto projB = std::make_shared<Project>();
	projB->SetName("ProjectBeta");
	Project::SetActive(projB);

	EXPECT_EQ(Project::GetActive(), projB);
	EXPECT_EQ(Project::GetActive()->GetName(), "ProjectBeta");

	// Act & Assert: Concurrent access safety
	constexpr int kThreadCount = 8;
	std::vector<std::thread> threads;
	threads.reserve(kThreadCount);

	for (int i = 0; i < kThreadCount; ++i)
	{
		threads.emplace_back([i]() {
			auto tempProj = std::make_shared<Project>();
			tempProj->SetName("ThreadProject_" + std::to_string(i));
			Project::SetActive(tempProj);
			auto active = Project::GetActive();
			EXPECT_NE(active, nullptr);
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_NE(Project::GetActive(), nullptr);

	// Cleanup
	Project::SetActive(nullptr);
	EXPECT_EQ(Project::GetActive(), nullptr);
}

// ============================================================================
// 4. PATH NORMALIZATION & RELATIVE CONVERSIONS
// ============================================================================
TEST(ProjectModule, PathUtilities_NormalizationAndRelativeResolution)
{
	// Act & Assert: NormalizePath (resolves . and .. and standardizes slashes)
	auto norm = Project::NormalizePath("some/path/../path/./subfolder");
	EXPECT_FALSE(norm.empty());

	// Act & Assert: TryMakeRelative (positive containment)
	auto base = std::filesystem::current_path();
	auto child = base / "assets" / "textures" / "diffuse.png";
	auto relOpt = Project::TryMakeRelative(child, base);

	ASSERT_TRUE(relOpt.has_value());
	EXPECT_EQ(*relOpt, "assets/textures/diffuse.png");

	// Act & Assert: TryMakeRelative (negative - outside directory)
	auto outside = base.parent_path() / "other_project" / "file.txt";
	auto outsideRel = Project::TryMakeRelative(outside, base);
	EXPECT_FALSE(outsideRel.has_value());

	// Act & Assert: Empty base path returns nullopt
	EXPECT_FALSE(Project::TryMakeRelative(child, "").has_value());
}

// ============================================================================
// 5. PROJECT INSTANCE PATH HELPERS
// ============================================================================
TEST(ProjectModule, InstancePathHelpers_GetAssetAndRelativePaths)
{
	// Arrange
	Project project;
	auto root = std::filesystem::current_path();
	project.GetConfig().ProjectDirectory = root;
	project.GetConfig().AssetDirectory = "assets";

	// Act & Assert: Directory getters
	EXPECT_EQ(project.GetProjectDirectory(), root);
	EXPECT_EQ(project.GetAssetDirectory(), root / "assets");
	EXPECT_EQ(project.GetAssetPath("models/player.glb"), root / "assets" / "models/player.glb");

	// Act & Assert: GetRelativePath for relative input
	EXPECT_EQ(project.GetRelativePath("textures/icon.png"), "textures/icon.png");

	// Act & Assert: GetRelativePath for absolute path within asset directory
	auto absAssetPath = root / "assets" / "shaders" / "pbr.glsl";
	EXPECT_EQ(project.GetRelativePath(absAssetPath), "shaders/pbr.glsl");

	// Act & Assert: Empty path handling
	EXPECT_TRUE(project.GetRelativePath("").empty());
	EXPECT_TRUE(project.GetAbsolutePath("").empty());
}

// ============================================================================
// 6. YAML SERIALIZATION & DESERIALIZATION (Project::Load)
// ============================================================================
TEST(ProjectModule, Serialization_LoadFromYamlFile)
{
	// Arrange: Create a temporary test .chproject file
	auto tempDir = std::filesystem::temp_directory_path() / "ChainedProjectTest";
	std::filesystem::create_directories(tempDir);
	auto projectFile = tempDir / "TestGame.chproject";

	std::ofstream out(projectFile);
	out << R"(
Project:
  Name: ChainedSandbox
  StartScene: scenes/Entry.chscene
  AssetDirectory: game_assets
  ActiveScene: scenes/Level2.chscene
  Physics:
    Gravity: 9.81
    FixedTimestep: 0.01666
  Window:
    Width: 1920
    Height: 1080
    VSync: true
  Runtime:
    Fullscreen: true
    ShowStats: false
    EnableConsole: true
    TargetFPS: 120
  Audio:
    MasterVolume: 0.8
    MusicVolume: 0.5
    SFXVolume: 0.9
  Render:
    ShadowResolution: 4096
    EnableShadows: true
    AntiAliasingSamples: 8
  Mesh:
    ImportMaterials: true
    CalculateTangents: true
    FlipUVs: false
  Scripting:
    ModuleName: GameLogic
    ModuleDirectory: scripts/bin
    AutoLoad: false
  Export:
    Mode: 1
    ZipThreshold: 0.1
    PackName: game_pack
)";
	out.close();

	// Act
	auto project = Project::Load(projectFile);

	// Assert: Loading succeeded and fields parsed correctly
	ASSERT_NE(project, nullptr);
	const auto& cfg = project->GetConfig();

	EXPECT_EQ(cfg.Name, "ChainedSandbox");
	EXPECT_EQ(cfg.StartScene, "scenes/Entry.chscene");
	EXPECT_EQ(cfg.AssetDirectory, "game_assets");
	EXPECT_EQ(cfg.ActiveScenePath, "scenes/Level2.chscene");
	EXPECT_FLOAT_EQ(cfg.Physics.Gravity, 9.81f);
	EXPECT_FLOAT_EQ(cfg.Physics.FixedTimestep, 0.01666f);
	EXPECT_EQ(cfg.Window.Width, 1920);
	EXPECT_EQ(cfg.Window.Height, 1080);
	EXPECT_TRUE(cfg.Window.VSync);
	EXPECT_TRUE(cfg.Runtime.Fullscreen);
	EXPECT_FALSE(cfg.Runtime.ShowStats);
	EXPECT_TRUE(cfg.Runtime.EnableConsole);
	EXPECT_EQ(cfg.Runtime.TargetFPS, 120);
	EXPECT_FLOAT_EQ(cfg.Audio.MasterVolume, 0.8f);
	EXPECT_FLOAT_EQ(cfg.Audio.MusicVolume, 0.5f);
	EXPECT_FLOAT_EQ(cfg.Audio.SFXVolume, 0.9f);
	EXPECT_EQ(cfg.Render.ShadowResolution, 4096);
	EXPECT_TRUE(cfg.Render.EnableShadows);
	EXPECT_EQ(cfg.Render.AntiAliasingSamples, 8);
	EXPECT_FALSE(cfg.Mesh.FlipUVs);
	EXPECT_EQ(cfg.Scripting.ModuleName, "GameLogic");
	EXPECT_EQ(cfg.Scripting.ModuleDirectory, "scripts/bin");
	EXPECT_FALSE(cfg.Scripting.AutoLoad);
	EXPECT_EQ(cfg.Export.Mode, PackMode::Balanced);
	EXPECT_FLOAT_EQ(cfg.Export.ZipThreshold, 0.1f);
	EXPECT_EQ(cfg.Export.PackName, "game_pack");

	// Act & Assert: Negative load tests (non-existent and non-project YAML)
	EXPECT_EQ(Project::Load(tempDir / "NonExistent.chproject"), nullptr);

	auto nonProjectFile = tempDir / "NonProject.chproject";
	std::ofstream nonProjectOut(nonProjectFile);
	nonProjectOut << "OtherConfig:\n  Key: Value\n";
	nonProjectOut.close();
	EXPECT_EQ(Project::Load(nonProjectFile), nullptr);

	// Cleanup
	std::filesystem::remove_all(tempDir);
}

// ============================================================================
// 7. AVAILABLE SCENES DISCOVERY (GetAvailableScenes)
// ============================================================================
TEST(ProjectModule, SceneDiscovery_FindsChSceneFilesRecursively)
{
	// Arrange: Create mock asset hierarchy
	auto tempDir = std::filesystem::temp_directory_path() / "ChainedSceneDiscoveryTest";
	auto assetDir = tempDir / "assets";
	auto scenesDir = assetDir / "scenes";
	auto subScenesDir = scenesDir / "levels";

	std::filesystem::create_directories(subScenesDir);

	std::ofstream(scenesDir / "Menu.chscene").close();
	std::ofstream(subScenesDir / "Level1.chscene").close();
	std::ofstream(subScenesDir / "IgnoredFile.txt").close();

	Project project;
	project.GetConfig().ProjectDirectory = tempDir;
	project.GetConfig().AssetDirectory = "assets";

	// Act
	auto scenes = project.GetAvailableScenes();

	// Assert: Found 2 .chscene files, ignored .txt
	EXPECT_EQ(scenes.size(), 2u);

	bool hasMenu = false;
	bool hasLevel1 = false;
	for (const auto& scene : scenes)
	{
		if (scene.find("Menu.chscene") != std::string::npos)
		{
			hasMenu = true;
		}
		if (scene.find("Level1.chscene") != std::string::npos)
		{
			hasLevel1 = true;
		}
	}
	EXPECT_TRUE(hasMenu);
	EXPECT_TRUE(hasLevel1);

	// Cleanup
	std::filesystem::remove_all(tempDir);
}
