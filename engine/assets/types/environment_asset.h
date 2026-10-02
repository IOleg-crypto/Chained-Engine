#ifndef CH_ENVIRONMENT_H
#define CH_ENVIRONMENT_H

#include "engine/assets/asset.h"
#include "engine/common/color.h"
#include <glm/glm.hpp>
#include <string>

namespace Chained
{

	// Face index constants for mode 3 (Six Faces cubemap)
	constexpr int SKYBOX_FACE_PX = 0; // Right  (+X)
	constexpr int SKYBOX_FACE_NX = 1; // Left   (-X)
	constexpr int SKYBOX_FACE_PY = 2; // Up     (+Y)
	constexpr int SKYBOX_FACE_NY = 3; // Down   (-Y)
	constexpr int SKYBOX_FACE_PZ = 4; // Front  (+Z)
	constexpr int SKYBOX_FACE_NZ = 5; // Back   (-Z)

	struct SkyboxSettings
	{
		std::string TexturePath;
		std::string CubeFaces[6]; // [0]=+X, [1]=-X, [2]=+Y, [3]=-Y, [4]=+Z, [5]=-Z
		int Mode = 0;			  // 0: Sphere, 1: Cross, 2: Cubemap (GPU), 3: Six Faces
		float Exposure = 1.0f;
		float Brightness = 0.0f;
		float Contrast = 1.0f;
	};

	struct FogSettings
	{
		bool Enabled = false;
		int Mode = 0; // 0: Linear, 1: Exp, 2: Exp2
		Color FogColor = {158, 148, 148, 255};
		float Density = 0.001f;
		float Start = 10.0f;
		float End = 5000.0f;
		float HeightFalloff = 0.2f;
	};

	struct LightingSettings
	{
		glm::vec3 Direction = {-1.0f, -1.0f, -1.0f};
		Color LightColor = {255, 255, 255, 255}; // WHITE
		float Ambient = 0.3f;
		float Exposure = 1.0f;
		float Gamma = 2.2f;
	};

	struct EnvironmentSettings
	{
		LightingSettings Lighting;
		SkyboxSettings Skybox;
		FogSettings Fog;
	};

	class EnvironmentAsset : public Asset
	{
	public:
		EnvironmentAsset()
			: Asset(GetStaticType())
		{
		}
		virtual ~EnvironmentAsset() = default;

		static AssetType GetStaticType()
		{
			return AssetType::Environment;
		}

		void OnLoaded() override
		{
			CH_CORE_INFO("Environment asset loaded: {}", GetPath());
		}

		const EnvironmentSettings& GetSettings() const
		{
			return m_Settings;
		}
		EnvironmentSettings& GetSettings()
		{
			return m_Settings;
		}
		void SetSettings(const EnvironmentSettings& settings)
		{
			m_Settings = settings;
		}

	private:
		EnvironmentSettings m_Settings;
	};

} // namespace Chained

#endif // CH_ENVIRONMENT_H