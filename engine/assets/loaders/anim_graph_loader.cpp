#include "anim_graph_loader.h"
#include "engine/scene/components/animation/animation_component.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

namespace Chained
{

	std::shared_ptr<Asset> AnimGraphLoader::Create()
	{
		return std::make_shared<AnimationGraphAsset>();
	}

	bool AnimGraphLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{
		auto graph = std::dynamic_pointer_cast<AnimationGraphAsset>(asset);
		if (!graph)
		{
			if (outError)
			{
				*outError = "Asset is not an AnimationGraphAsset";
			}
			return false;
		}

		try
		{
			std::string content;

			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				content = am->ReadText(resolvedPath);
			}

			if (content.empty())
			{
				std::ifstream stream(resolvedPath);
				if (!stream.is_open())
				{
					if (outError)
					{
						*outError = "AnimGraphLoader: File not found: " + resolvedPath;
					}
					return false;
				}
				std::stringstream ss;
				ss << stream.rdbuf();
				content = ss.str();
			}

			YAML::Node data = YAML::Load(content);
			if (!data["AnimationGraph"])
			{
				if (outError)
				{
					*outError = "Invalid AnimGraph format";
				}
				return false;
			}

			graph->Nodes.clear();
			graph->Transitions.clear();

			auto root = data["AnimationGraph"];
			if (root["EntryNodeID"])
			{
				graph->EntryNodeID = root["EntryNodeID"].as<int>();
			}
			if (root["NextNodeID"])
			{
				graph->NextNodeID = root["NextNodeID"].as<int>();
			}
			if (root["NextLinkID"])
			{
				graph->NextLinkID = root["NextLinkID"].as<int>();
			}

			if (auto nodes = root["Nodes"])
			{
				for (const auto& n : nodes)
				{
					AnimNode node;
					if (n["ID"])
					{
						node.ID = n["ID"].as<int>();
					}
					if (n["Name"])
					{
						node.Name = n["Name"].as<std::string>();
					}
					if (n["AnimationIndex"])
					{
						node.AnimationIndex = n["AnimationIndex"].as<int>();
					}
					if (n["IsLooping"])
					{
						node.IsLooping = n["IsLooping"].as<bool>();
					}
					if (n["StartFrame"])
					{
						node.StartFrame = n["StartFrame"].as<int>();
					}
					if (n["EndFrame"])
					{
						node.EndFrame = n["EndFrame"].as<int>();
					}
					if (n["Speed"])
					{
						node.Speed = n["Speed"].as<float>();
					}
					if (n["EditorX"])
					{
						node.EditorX = n["EditorX"].as<float>();
					}
					if (n["EditorY"])
					{
						node.EditorY = n["EditorY"].as<float>();
					}
					graph->Nodes.push_back(node);
				}
			}

			if (auto links = root["Transitions"])
			{
				for (const auto& l : links)
				{
					AnimTransition tr;
					if (l["ID"])
					{
						tr.ID = l["ID"].as<int>();
					}
					if (l["SourceNodeID"])
					{
						tr.SourceNodeID = l["SourceNodeID"].as<int>();
					}
					if (l["TargetNodeID"])
					{
						tr.TargetNodeID = l["TargetNodeID"].as<int>();
					}
					if (l["BlendDuration"])
					{
						tr.BlendDuration = l["BlendDuration"].as<float>();
					}
					if (l["HasExitTime"])
					{
						tr.HasExitTime = l["HasExitTime"].as<bool>();
					}
					if (l["ExitTime"])
					{
						tr.ExitTime = l["ExitTime"].as<float>();
					}
					if (l["Priority"])
					{
						tr.Priority = l["Priority"].as<int>();
					}
					if (l["ExitTimeMode"])
					{
						tr.ExitTimeMode = (decltype(tr.ExitTimeMode))l["ExitTimeMode"].as<int>();
					}

					if (auto conds = l["Conditions"])
					{
						for (const auto& c : conds)
						{
							AnimCondition cond;
							if (c["VariableName"])
							{
								cond.VariableName = c["VariableName"].as<std::string>();
							}
							if (c["Op"])
							{
								cond.Op = (AnimConditionOp)c["Op"].as<int>();
							}
							if (c["Value"])
							{
								cond.Value = c["Value"].as<float>();
							}
							tr.Conditions.push_back(cond);
						}
					}
					graph->Transitions.push_back(tr);
				}
			}
			// Load DefaultVariables (schema: name -> default value)
			if (auto vars = root["Variables"])
			{
				for (auto it = vars.begin(); it != vars.end(); ++it)
				{
					graph->DefaultVariables[it->first.as<std::string>()] = it->second.as<float>();
				}
			}

			return true;
		} catch (const YAML::Exception& e)
		{
			if (outError)
			{
				*outError = std::string("YAML Exception: ") + e.what();
			}
			return false;
		}
	}

	bool AnimGraphLoader::Save(const AnimationGraphAsset& graph, const std::string& path)
	{
		try
		{
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "AnimationGraph" << YAML::Value << YAML::BeginMap;

			out << YAML::Key << "EntryNodeID" << YAML::Value << graph.EntryNodeID;
			out << YAML::Key << "NextNodeID" << YAML::Value << graph.NextNodeID;
			out << YAML::Key << "NextLinkID" << YAML::Value << graph.NextLinkID;

			out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
			for (const auto& node : graph.Nodes)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "ID" << YAML::Value << node.ID;
				out << YAML::Key << "Name" << YAML::Value << node.Name;
				out << YAML::Key << "AnimationIndex" << YAML::Value << node.AnimationIndex;
				out << YAML::Key << "IsLooping" << YAML::Value << node.IsLooping;
				out << YAML::Key << "StartFrame" << YAML::Value << node.StartFrame;
				out << YAML::Key << "EndFrame" << YAML::Value << node.EndFrame;
				out << YAML::Key << "Speed" << YAML::Value << node.Speed;
				out << YAML::Key << "EditorX" << YAML::Value << node.EditorX;
				out << YAML::Key << "EditorY" << YAML::Value << node.EditorY;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::Key << "Transitions" << YAML::Value << YAML::BeginSeq;
			for (const auto& tr : graph.Transitions)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "ID" << YAML::Value << tr.ID;
				out << YAML::Key << "SourceNodeID" << YAML::Value << tr.SourceNodeID;
				out << YAML::Key << "TargetNodeID" << YAML::Value << tr.TargetNodeID;
				out << YAML::Key << "BlendDuration" << YAML::Value << tr.BlendDuration;
				out << YAML::Key << "HasExitTime" << YAML::Value << tr.HasExitTime;
				out << YAML::Key << "ExitTime" << YAML::Value << tr.ExitTime;
				out << YAML::Key << "ExitTimeMode" << YAML::Value << static_cast<int>(tr.ExitTimeMode);
				out << YAML::Key << "Priority" << YAML::Value << tr.Priority;

				if (!tr.Conditions.empty())
				{
					out << YAML::Key << "Conditions" << YAML::Value << YAML::BeginSeq;
					for (const auto& cond : tr.Conditions)
					{
						out << YAML::BeginMap;
						out << YAML::Key << "VariableName" << YAML::Value << cond.VariableName;
						out << YAML::Key << "Op" << YAML::Value << static_cast<int>(cond.Op);
						out << YAML::Key << "Value" << YAML::Value << cond.Value;
						out << YAML::EndMap;
					}
					out << YAML::EndSeq;
				}
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			// Save DefaultVariables
			if (!graph.DefaultVariables.empty())
			{
				out << YAML::Key << "Variables" << YAML::Value << YAML::BeginMap;
				for (const auto& [name, val] : graph.DefaultVariables)
				{
					out << YAML::Key << name << YAML::Value << val;
				}
				out << YAML::EndMap;
			}

			out << YAML::EndMap;
			out << YAML::EndMap;

			std::filesystem::path filePath(path);
			std::filesystem::create_directories(filePath.parent_path());

			std::ofstream file(path);
			if (!file.is_open())
			{
				CH_CORE_ERROR("AnimGraphLoader: Failed to open file for writing: {}", path);
				return false;
			}
			file << out.c_str();
			return true;
		} catch (const std::exception& e)
		{
			CH_CORE_ERROR("AnimGraphLoader: Save exception: {}", e.what());
			return false;
		}
	}

} // namespace Chained
