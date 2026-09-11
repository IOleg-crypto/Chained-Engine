#include "scene_serializer.h"
#include "component_serializer.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/environment_asset.h"
#include "engine/core/service_locator.h"
#include "engine/project/project.h"
#include "engine/scene/hierarchy_serializer.h"
#include "engine/scene/yaml.h"
#include "engine/scene/components/physics/physics_component.h"
#include "engine/scene/systems/hierarchy_system.h"
#include "scene.h"

#include <set>

namespace Chained
{
	namespace
	{
		template <typename T> T ReadYamlValue(const YAML::Node& node, const char* key, const T& fallback)
		{
			if (!node || !node[key])
			{
				return fallback;
			}

			return node[key].as<T>(fallback);
		}

		namespace
		{
			static std::string ToProjectRelativePath(const std::string& absPath)
			{
				auto project = Project::GetActive();
				return project ? project->GetRelativePath(absPath) : absPath;
			}
		} // namespace

		static void SerializeBackgroundSettings(YAML::Emitter& out, const SceneSettings& settings)
		{
			out << YAML::Key << "Background" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Mode" << YAML::Value << (int)settings.Mode;
			out << YAML::Key << "Color" << YAML::Value << settings.BackgroundColor;
			out << YAML::Key << "TexturePath" << YAML::Value << settings.BackgroundTexturePath;
			out << YAML::EndMap;
		}

		static void SerializeCanvasSettings(YAML::Emitter& out, const SceneSettings& settings)
		{
			out << YAML::Key << "Canvas" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "ReferenceResolution" << YAML::Value << settings.Canvas.ReferenceResolution;
			out << YAML::Key << "ScaleMode" << YAML::Value << (int)settings.Canvas.ScaleMode;
			out << YAML::Key << "MatchWidthOrHeight" << YAML::Value << settings.Canvas.MatchWidthOrHeight;
			out << YAML::EndMap;
		}

		static void SerializeEnvironmentSettings(YAML::Emitter& out, const SceneSettings& settings)
		{
			if (!settings.Environment)
			{
				return;
			}

			std::string envPath = ToProjectRelativePath(settings.Environment->GetPath());
			out << YAML::Key << "EnvironmentPath" << YAML::Value << envPath;

			// Also serialize the current settings for quick preview/fallback.

			const auto& envSettings = settings.Environment->GetSettings();

			out << YAML::Key << "Lighting" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Direction" << YAML::Value << envSettings.Lighting.Direction;
			out << YAML::Key << "LightColor" << YAML::Value << envSettings.Lighting.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << envSettings.Lighting.Ambient;
			out << YAML::EndMap;

			out << YAML::Key << "Skybox" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "TexturePath" << YAML::Value << ToProjectRelativePath(envSettings.Skybox.TexturePath);
			out << YAML::Key << "Mode" << YAML::Value << envSettings.Skybox.Mode;
			out << YAML::Key << "Exposure" << YAML::Value << envSettings.Skybox.Exposure;
			out << YAML::Key << "Brightness" << YAML::Value << envSettings.Skybox.Brightness;
			out << YAML::Key << "Contrast" << YAML::Value << envSettings.Skybox.Contrast;
			out << YAML::EndMap;

			out << YAML::Key << "Fog" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Enabled" << YAML::Value << envSettings.Fog.Enabled;
			out << YAML::Key << "Color" << YAML::Value << envSettings.Fog.FogColor;
			out << YAML::Key << "Density" << YAML::Value << envSettings.Fog.Density;
			out << YAML::Key << "Start" << YAML::Value << envSettings.Fog.Start;
			out << YAML::Key << "End" << YAML::Value << envSettings.Fog.End;
			out << YAML::EndMap;
		}

		static void SerializeDebugSettings(YAML::Emitter& out, const SceneSettings& settings)
		{
			out << YAML::Key << "DebugSettings" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "DiagnosticMode" << YAML::Value << settings.DiagnosticMode;
			out << YAML::Key << "DrawColliders" << YAML::Value << settings.DebugFlags.DrawColliders;
			out << YAML::Key << "DrawHierarchy" << YAML::Value << settings.DebugFlags.DrawHierarchy;
			out << YAML::Key << "DrawGrid" << YAML::Value << settings.DebugFlags.DrawGrid;
			out << YAML::Key << "DrawSelection" << YAML::Value << settings.DebugFlags.DrawSelection;
			out << YAML::Key << "DrawLights" << YAML::Value << settings.DebugFlags.DrawLights;
			out << YAML::Key << "DrawSpawnZones" << YAML::Value << settings.DebugFlags.DrawSpawnZones;
			out << YAML::Key << "CollisionWireframeMode" << YAML::Value
				<< settings.DebugFlags.SetCollisionWireframeMode;
			out << YAML::EndMap;
		}

		static void SerializeGridSettings(YAML::Emitter& out, const GridSettings& grid)
		{
			out << YAML::Key << "Grid" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Spacing" << YAML::Value << grid.Spacing;
			out << YAML::Key << "SecondarySpacing" << YAML::Value << grid.SecondarySpacing;
			out << YAML::Key << "Color" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "r" << YAML::Value << grid.Color.x;
			out << YAML::Key << "g" << YAML::Value << grid.Color.y;
			out << YAML::Key << "b" << YAML::Value << grid.Color.z;
			out << YAML::Key << "a" << YAML::Value << grid.Color.w;
			out << YAML::EndMap;
			out << YAML::Key << "FadeStart" << YAML::Value << grid.FadeStart;
			out << YAML::Key << "FadeEnd" << YAML::Value << grid.FadeEnd;
			out << YAML::Key << "PlaneSize" << YAML::Value << grid.PlaneSize;
			out << YAML::EndMap;
		}

		static void DeserializeBackgroundSettings(const YAML::Node& sceneRoot, SceneSettings& settings)
		{
			if (!sceneRoot["Background"])
			{
				return;
			}

			auto background = sceneRoot["Background"];
			settings.Mode =
				static_cast<BackgroundMode>(ReadYamlValue(background, "Mode", static_cast<int>(settings.Mode)));
			settings.BackgroundColor = ReadYamlValue(background, "Color", settings.BackgroundColor);
			if (background["TexturePath"] && background["TexturePath"].IsScalar())
			{
				settings.BackgroundTexturePath =
					ReadYamlValue(background, "TexturePath", settings.BackgroundTexturePath);
			}
			// Legacy AmbientIntensity in Background block is silently ignored.
		}

		static void DeserializeCanvasSettings(const YAML::Node& sceneRoot, SceneSettings& settings)
		{
			if (!sceneRoot["Canvas"])
			{
				return;
			}

			auto canvas = sceneRoot["Canvas"];
			settings.Canvas.ReferenceResolution =
				ReadYamlValue(canvas, "ReferenceResolution", settings.Canvas.ReferenceResolution);
			settings.Canvas.ScaleMode = static_cast<CanvasScaleMode>(
				ReadYamlValue(canvas, "ScaleMode", static_cast<int>(settings.Canvas.ScaleMode)));
			settings.Canvas.MatchWidthOrHeight =
				ReadYamlValue(canvas, "MatchWidthOrHeight", settings.Canvas.MatchWidthOrHeight);
		}

		static void DeserializeDebugSettings(const YAML::Node& sceneRoot, SceneSettings& settings)
		{
			if (!sceneRoot["DebugSettings"])
			{
				return;
			}

			auto debugNode = sceneRoot["DebugSettings"];
			settings.DiagnosticMode = ReadYamlValue(debugNode, "DiagnosticMode", 0.0f);
			settings.DebugFlags.DrawColliders = ReadYamlValue(debugNode, "DrawColliders", false);
			settings.DebugFlags.DrawHierarchy = ReadYamlValue(debugNode, "DrawHierarchy", false);
			settings.DebugFlags.DrawGrid = ReadYamlValue(debugNode, "DrawGrid", false);
			settings.DebugFlags.DrawSelection = ReadYamlValue(debugNode, "DrawSelection", true);
			settings.DebugFlags.DrawLights = ReadYamlValue(debugNode, "DrawLights", true);
			settings.DebugFlags.DrawSpawnZones = ReadYamlValue(debugNode, "DrawSpawnZones", true);
			settings.DebugFlags.SetCollisionWireframeMode = ReadYamlValue(debugNode, "CollisionWireframeMode", 0);
		}

		static void DeserializeGridSettings(const YAML::Node& sceneRoot, GridSettings& grid)
		{
			if (!sceneRoot["Grid"])
			{
				return;
			}

			auto gridNode = sceneRoot["Grid"];
			grid.Spacing = ReadYamlValue(gridNode, "Spacing", grid.Spacing);
			grid.SecondarySpacing = ReadYamlValue(gridNode, "SecondarySpacing", grid.SecondarySpacing);
			grid.FadeStart = ReadYamlValue(gridNode, "FadeStart", grid.FadeStart);
			grid.FadeEnd = ReadYamlValue(gridNode, "FadeEnd", grid.FadeEnd);
			grid.PlaneSize = ReadYamlValue(gridNode, "PlaneSize", grid.PlaneSize);
			if (gridNode["Color"])
			{
				auto colorNode = gridNode["Color"];
				grid.Color.x = ReadYamlValue(colorNode, "r", grid.Color.x);
				grid.Color.y = ReadYamlValue(colorNode, "g", grid.Color.y);
				grid.Color.z = ReadYamlValue(colorNode, "b", grid.Color.z);
				grid.Color.w = ReadYamlValue(colorNode, "a", grid.Color.w);
			}
		}

		static void DeserializeEnvironmentSettings(const YAML::Node& sceneRoot, SceneSettings& settings)
		{
			if (sceneRoot["EnvironmentPath"] && sceneRoot["EnvironmentPath"].IsScalar())
			{
				std::string envPath = ReadYamlValue(sceneRoot, "EnvironmentPath", std::string());
				if (Project::GetActive())
				{
					auto* assets = ServiceLocator::TryGet<AssetManager>();
					if (assets)
					{
						auto sharedEnv = assets->Get<EnvironmentAsset>(envPath);
						if (sharedEnv)
						{
							if (!settings.Environment)
							{
								settings.Environment = std::make_shared<EnvironmentAsset>();
							}

							settings.Environment->SetSettings(sharedEnv->GetSettings());
							settings.Environment->SetPath(sharedEnv->GetPath());
						}
					}
				}
			}

			if (!sceneRoot["Skybox"] && !sceneRoot["Fog"] && !sceneRoot["LightDirection"])
			{
				return;
			}

			if (!settings.Environment)
			{
				settings.Environment = std::make_shared<EnvironmentAsset>();
			}

			auto& env = settings.Environment;
			auto& envSettings = env->GetSettings();

			if (sceneRoot["Lighting"])
			{
				auto lighting = sceneRoot["Lighting"];
				envSettings.Lighting.Direction = ReadYamlValue(lighting, "Direction", envSettings.Lighting.Direction);
				envSettings.Lighting.LightColor =
					ReadYamlValue(lighting, "LightColor", envSettings.Lighting.LightColor);
				envSettings.Lighting.Ambient = ReadYamlValue(lighting, "Ambient", envSettings.Lighting.Ambient);
			}
			else
			{
				// Backward compat: old flat field names.
				envSettings.Lighting.Direction =
					ReadYamlValue(sceneRoot, "LightDirection", envSettings.Lighting.Direction);
				envSettings.Lighting.LightColor =
					ReadYamlValue(sceneRoot, "LightColor", envSettings.Lighting.LightColor);
				envSettings.Lighting.Ambient =
					ReadYamlValue(sceneRoot, "AmbientIntensity", envSettings.Lighting.Ambient);
			}

			if (auto skybox = sceneRoot["Skybox"])
			{
				if (skybox["TexturePath"] && skybox["TexturePath"].IsScalar())
				{
					envSettings.Skybox.TexturePath =
						ReadYamlValue(skybox, "TexturePath", envSettings.Skybox.TexturePath);
				}
				envSettings.Skybox.Mode = ReadYamlValue(skybox, "Mode", envSettings.Skybox.Mode);
				envSettings.Skybox.Exposure = ReadYamlValue(skybox, "Exposure", envSettings.Skybox.Exposure);
				envSettings.Skybox.Brightness = ReadYamlValue(skybox, "Brightness", envSettings.Skybox.Brightness);
				envSettings.Skybox.Contrast = ReadYamlValue(skybox, "Contrast", envSettings.Skybox.Contrast);
			}

			if (auto fog = sceneRoot["Fog"])
			{
				envSettings.Fog.Enabled = ReadYamlValue(fog, "Enabled", envSettings.Fog.Enabled);
				envSettings.Fog.FogColor = ReadYamlValue(fog, "Color", envSettings.Fog.FogColor);
				envSettings.Fog.Density = ReadYamlValue(fog, "Density", envSettings.Fog.Density);
				envSettings.Fog.Start = ReadYamlValue(fog, "Start", envSettings.Fog.Start);
				envSettings.Fog.End = ReadYamlValue(fog, "End", envSettings.Fog.End);
			}
		}

		static void SerializeSceneSettings(YAML::Emitter& out, const SceneSettings& settings)
		{
			out << YAML::Key << "Scene" << YAML::Value << settings.Name;
			out << YAML::Key << "SceneType" << YAML::Value << (int)settings.Type;
			SerializeBackgroundSettings(out, settings);
			SerializeCanvasSettings(out, settings);
			SerializeEnvironmentSettings(out, settings);
			SerializeGridSettings(out, settings.Grid);
			SerializeDebugSettings(out, settings);
		}

		static bool DeserializeSceneSettings(const YAML::Node& sceneRoot, SceneSettings& settings,
											 std::string& lastError)
		{
			if (!sceneRoot["Scene"] || !sceneRoot["Scene"].IsScalar())
			{
				lastError = "SceneSerializer: missing Scene root key";
				return false;
			}

			settings.Name = ReadYamlValue(sceneRoot, "Scene", settings.Name);
			settings.Type =
				static_cast<SceneType>(ReadYamlValue(sceneRoot, "SceneType", static_cast<int>(settings.Type)));
			DeserializeBackgroundSettings(sceneRoot, settings);
			DeserializeCanvasSettings(sceneRoot, settings);
			DeserializeGridSettings(sceneRoot, settings.Grid);
			DeserializeDebugSettings(sceneRoot, settings);
			DeserializeEnvironmentSettings(sceneRoot, settings);
			return true;
		}

		static void DeserializeEntities(Scene* scene, const YAML::Node& entities)
		{
			std::vector<HierarchyTask> hierarchyTasks;
			std::set<uint64_t> seenUUIDs;

			for (auto entity : entities)
			{
				if (!entity["Entity"])
				{
					CH_CORE_WARN("SceneSerializer: Malformed entity entry missing 'Entity' key, skipping.");
					continue;
				}

				uint64_t uuid = ReadYamlValue(entity, "Entity", uint64_t{0});
				if (uuid == 0)
				{
					uuid = UUID();
				}
				else if (seenUUIDs.count(uuid))
				{
					CH_CORE_WARN("SceneSerializer: Duplicate UUID {} found, generating new UUID.", uuid);
					uuid = UUID();
				}
				seenUUIDs.insert(uuid);

				std::string name;
				auto tagComponent = entity["Tag"];
				if (tagComponent && tagComponent["Tag"] && tagComponent["Tag"].IsScalar())
				{
					name = ReadYamlValue(tagComponent, "Tag", std::string());
				}

				Entity deserializedEntity = scene->CreateEntityWithUUID(uuid, name);

				ComponentSerializer::DeserializeAll(deserializedEntity, entity);

				if (deserializedEntity.HasComponent<TransformComponent>())
				{
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.RotationQuat = glm::quat(tc.Rotation);
					tc.PrevRotationQuat = tc.RotationQuat;
					tc.PrevTranslation = tc.Translation;
					tc.PrevScale = tc.Scale;
					tc.TransformChanged = true;
				}

				// Editor-saved physics handles are stale at runtime. Reset so
				// PhysicsBodySystem / BatchInitializeBodies will create fresh bodies.
				if (deserializedEntity.HasComponent<RigidBodyComponent>())
				{
					deserializedEntity.GetComponent<RigidBodyComponent>().Handle = kInvalidPhysicsBody;
				}

				HierarchyTask task;
				HierarchySerializer::DeserializeTask(deserializedEntity, entity, task);
				if (task.entity)
				{
					hierarchyTasks.push_back(task);
				}
			}

			for (auto& task : hierarchyTasks)
			{
				if (!task.entity.HasComponent<HierarchyComponent>())
				{
					task.entity.AddComponent<HierarchyComponent>();
				}

				auto& hc = task.entity.GetComponent<HierarchyComponent>();
				if (task.parent != 0)
				{
					Chained::Entity parent = scene->GetEntityByUUID(task.parent);
					if (parent)
					{
						hc.Parent = parent;
					}
				}

				for (uint64_t childUUID : task.children)
				{
					Chained::Entity child = scene->GetEntityByUUID(childUUID);
					if (child)
					{
						hc.Children.push_back(child);
					}
				}
			}

			// Immediately compute all world transforms on startup
			Hierarchy::UpdateWorldTransforms(scene->GetRegistry(), scene->GetRootEntities());
		}
	} // namespace

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap; // Entity

		ComponentSerializer::SerializeID(out, entity);
		ComponentSerializer::SerializeAll(out, entity);

		out << YAML::EndMap; // Entity
	}

	std::string SceneSerializer::SerializeToString()
	{
		if (!m_Scene)
		{
			return "";
		}

		YAML::Emitter out;
		out << YAML::BeginMap;

		SerializeSceneSettings(out, m_Scene->GetSettings());

		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		m_Scene->GetRegistry().view<IDComponent>().each([&, this](auto entityID, auto& id) {
			Entity entity(entityID, &m_Scene->GetRegistry());
			SerializeEntity(out, entity);
		});

		out << YAML::EndSeq;
		out << YAML::EndMap;

		return std::string(out.c_str());
	}

	bool SceneSerializer::Serialize(const std::string& filepath)
	{
		std::string yaml = SerializeToString();
		std::ofstream fout(filepath);
		if (fout.is_open())
		{
			fout << yaml;
			if (!fout.good())
			{
				CH_CORE_ERROR("SceneSerializer: Failed to write scene file '{}'.", filepath);
				return false;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	SceneSerializer::SceneSerializer(Scene* scene)
		: m_Scene(scene)
	{
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		m_LastError.clear();

		// ── 1. Try reading from pack archive ────────────────────────────────────
		// ReadProjectAsset converts the absolute path to a project-relative pack key internally.
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (am && am->IsPacked())
		{
			auto data = am->ReadProjectAsset(filepath);
			if (!data.empty())
			{
				return DeserializeFromString({data.begin(), data.end()});
			}
		}

		// ── 2. Fallback: direct disk read ────────────────────────────────────────
		std::ifstream stream(filepath);
		if (!stream.is_open())
		{
			m_LastError = "SceneSerializer: failed to open scene file '" + filepath + "'";
			return false;
		}

		std::stringstream strStream;
		strStream << stream.rdbuf();

		return DeserializeFromString(strStream.str());
	}

	bool SceneSerializer::DeserializeFromString(const std::string& yaml)
	{
		m_LastError.clear();

		YAML::Node sceneRootNode;
		try
		{
			sceneRootNode = YAML::Load(yaml);
		} catch (const std::exception& e)
		{
			m_LastError = std::string("SceneSerializer: invalid YAML: ") + e.what();
			return false;
		} catch (...)
		{
			m_LastError = "SceneSerializer: invalid YAML with an unknown exception";
			return false;
		}

		SceneSettings settings = m_Scene->GetSettings();
		if (!DeserializeSceneSettings(sceneRootNode, settings, m_LastError))
		{
			return false;
		}
		m_Scene->SetSettings(settings);

		auto entities = sceneRootNode["Entities"];
		if (entities && entities.IsSequence())
		{
			DeserializeEntities(m_Scene, entities);
		}

		return true;
	}
} // namespace Chained
