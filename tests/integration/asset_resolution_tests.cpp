// asset_resolution_tests.cpp
// Integration tests for AssetResolutionSystem (SpriteComponent, ShaderComponent, ModelComponent resolution).
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/assets/asset_manager.h"
#include "engine/assets/loaders/iasset_loader.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/assets/types/shader_asset.h"
#include "engine/assets/types/model_asset.h"
#include "engine/scene/scene.h"
#include "engine/scene/components/render/sprite_component.h"
#include "engine/scene/components/render/shader_component.h"
#include "engine/scene/components/render/model_component.h"
#include "engine/scene/systems/asset_resolution_system.h"
#include "gtest/gtest.h"

#include <filesystem>

using namespace Chained;

namespace
{
	template <typename AssetT> class TestLoader final : public IAssetLoader
	{
	public:
		bool IsAsync() const override
		{
			return false;
		}
		std::shared_ptr<Asset> Create() override
		{
			return std::make_shared<AssetT>();
		}
		bool Load(std::shared_ptr<Asset> asset, const std::string& path, std::string*) override
		{
			asset->SetPath(path);
			return true;
		}
	};

	struct ScopedResolutionEnvironment
	{
		std::filesystem::path dir;
		ScopedResolutionEnvironment(const std::string& name)
		{
			dir = std::filesystem::current_path() / name;
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
		}
		~ScopedResolutionEnvironment()
		{
			std::error_code ec;
			std::filesystem::remove_all(dir, ec);
		}
	};
} // namespace

TEST(AssetResolutionTest, ResolveSpriteByPath)
{
	// Arrange
	ScopedResolutionEnvironment env("test_res_sprite");
	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);
	am.RegisterLoader(AssetType::Texture, std::make_unique<TestLoader<TextureAsset>>());

	Scene scene;
	Entity entity = scene.CreateEntity("SpriteEntity");
	auto& sprite = entity.AddComponent<SpriteComponent>();
	sprite.TexturePath = "textures/player.png";

	// Act
	AssetResolutionSystem::Update(scene.GetRegistry(), &am);

	// Assert
	EXPECT_NE((uint64_t)sprite.TextureHandle, 0ull);
	EXPECT_NE(sprite.TextureUUID, 0u);

	auto asset = am.Get<TextureAsset>(sprite.TextureHandle);
	ASSERT_NE(asset, nullptr);
	EXPECT_FALSE(asset->GetPath().empty());
}

TEST(AssetResolutionTest, ResolveShaderByPath)
{
	// Arrange
	ScopedResolutionEnvironment env("test_res_shader");
	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);
	am.RegisterLoader(AssetType::Shader, std::make_unique<TestLoader<ShaderAsset>>());

	Scene scene;
	Entity entity = scene.CreateEntity("ShaderEntity");
	auto& shader = entity.AddComponent<ShaderComponent>();
	shader.ShaderPath = "shaders/standard.glsl";

	// Act
	AssetResolutionSystem::Update(scene.GetRegistry(), &am);

	// Assert
	EXPECT_NE((uint64_t)shader.ShaderHandle, 0ull);
	EXPECT_NE(shader.ShaderUUID, 0u);

	auto asset = am.Get<ShaderAsset>(shader.ShaderHandle);
	ASSERT_NE(asset, nullptr);
	EXPECT_FALSE(asset->GetPath().empty());
}

TEST(AssetResolutionTest, ResolveModelByPath)
{
	// Arrange
	ScopedResolutionEnvironment env("test_res_model");
	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);
	am.RegisterLoader(AssetType::Model, std::make_unique<TestLoader<ModelAsset>>());

	Scene scene;
	Entity entity = scene.CreateEntity("ModelEntity");
	auto& model = entity.AddComponent<ModelComponent>();
	model.ModelPath = "models/tree.glb";

	// Act
	AssetResolutionSystem::Update(scene.GetRegistry(), &am);

	// Assert
	EXPECT_NE((uint64_t)model.ModelHandle, 0ull);
	EXPECT_NE(model.ModelUUID, 0u);

	auto asset = am.Get<ModelAsset>(model.ModelHandle);
	ASSERT_NE(asset, nullptr);
	EXPECT_FALSE(asset->GetPath().empty());
}

TEST(AssetResolutionTest, ResolveByUUIDFallbackWhenPathAndHandleEmpty)
{
	// Arrange
	ScopedResolutionEnvironment env("test_res_fallback_uuid");
	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);
	am.RegisterLoader(AssetType::Shader, std::make_unique<TestLoader<ShaderAsset>>());

	auto loaded = am.Get<ShaderAsset>("shaders/preloaded.glsl");
	ASSERT_NE(loaded, nullptr);
	UUID uuid = loaded->GetID();

	Scene scene;
	Entity entity = scene.CreateEntity("UUIDEntity");
	auto& shader = entity.AddComponent<ShaderComponent>();
	shader.ShaderHandle = AssetHandle(0);
	shader.ShaderUUID = (uint64_t)uuid;
	shader.ShaderPath = "";

	// Act
	AssetResolutionSystem::Update(scene.GetRegistry(), &am);

	// Assert
	EXPECT_EQ(shader.ShaderHandle, (AssetHandle)uuid);
	EXPECT_FALSE(shader.ShaderPath.empty());
}

TEST(AssetResolutionTest, EmptyPathAndHandleRemainsUnresolved)
{
	// Arrange
	Scene scene;
	Entity entity = scene.CreateEntity("EmptyEntity");
	auto& sprite = entity.AddComponent<SpriteComponent>();
	sprite.TextureHandle = AssetHandle(0);
	sprite.TextureUUID = 0;
	sprite.TexturePath = "";

	// Act
	AssetResolutionSystem::Update(scene.GetRegistry(), nullptr);

	// Assert
	EXPECT_EQ((uint64_t)sprite.TextureHandle, 0ull);
	EXPECT_EQ(sprite.TextureUUID, 0u);
	EXPECT_TRUE(sprite.TexturePath.empty());
}
