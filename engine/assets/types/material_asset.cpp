#include "engine/assets/types/material_asset.h"
#include "engine/assets/loaders/yaml_helpers.h"

namespace Chained
{

	void MaterialAsset::SaveToFile(const std::string& path) const
	{
		try
		{
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;

			out << YAML::Key << "Name" << YAML::Value << m_Material.Name;
			out << YAML::Key << "AlbedoColor" << YAML::Value << Vec4ToYAML(m_Material.AlbedoColor);
			out << YAML::Key << "EmissiveColor" << YAML::Value << Vec4ToYAML(m_Material.EmissiveColor);
			out << YAML::Key << "EmissiveIntensity" << YAML::Value << m_Material.EmissiveIntensity;
			out << YAML::Key << "Metalness" << YAML::Value << m_Material.Metalness;
			out << YAML::Key << "Roughness" << YAML::Value << m_Material.Roughness;
			out << YAML::Key << "Transparent" << YAML::Value << m_Material.Transparent;
			out << YAML::Key << "Alpha" << YAML::Value << m_Material.Alpha;

			if (!m_Material.AlbedoPath.empty())
			{
				out << YAML::Key << "AlbedoMap" << YAML::Value << m_Material.AlbedoPath;
			}
			if (!m_Material.NormalPath.empty())
			{
				out << YAML::Key << "NormalMap" << YAML::Value << m_Material.NormalPath;
			}
			if (!m_Material.MetallicRoughnessPath.empty())
			{
				out << YAML::Key << "MetallicRoughnessMap" << YAML::Value << m_Material.MetallicRoughnessPath;
			}
			if (!m_Material.EmissivePath.empty())
			{
				out << YAML::Key << "EmissiveMap" << YAML::Value << m_Material.EmissivePath;
			}
			if (!m_Material.OcclusionPath.empty())
			{
				out << YAML::Key << "OcclusionMap" << YAML::Value << m_Material.OcclusionPath;
			}

			out << YAML::EndMap;
			out << YAML::EndMap;

			std::filesystem::path filePath(path);
			if (filePath.has_parent_path())
			{
				std::filesystem::create_directories(filePath.parent_path());
			}

			std::ofstream file(path);
			if (!file.is_open())
			{
				CH_CORE_ERROR("MaterialAsset: Failed to open file for writing: {}", path);
				return;
			}
			file << out.c_str();
			CH_CORE_INFO("MaterialAsset: Saved to {}", path);
		} catch (const YAML::Exception& e)
		{
			CH_CORE_ERROR("MaterialAsset: Failed to save {}: {}", path, e.what());
		}
	}

} // namespace Chained
