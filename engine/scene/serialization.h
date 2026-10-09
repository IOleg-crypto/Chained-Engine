#ifndef CH_SERIALIZATION_UTILS_H
#define CH_SERIALIZATION_UTILS_H

#include "engine/reflection/reflection.h"
#include "engine/common/uuid.h"
#include "engine/project/project.h"
#include "engine/scene/yaml.h"
#include <filesystem>

namespace Chained::Serialization
{
	// --- YAML Serialization Helpers ---

	template <typename T> inline void SerializeProperty(YAML::Emitter& out, const char* name, const T& value)
	{
		if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t> || std::is_same_v<T, unsigned char> ||
					  std::is_same_v<T, char>)
		{
			out << YAML::Key << name << YAML::Value << static_cast<int>(value);
		}
		else
		{
			out << YAML::Key << name << YAML::Value << value;
		}
	}

	inline void SerializePath(YAML::Emitter& out, const char* name, const std::string& path)
	{
		if (path.empty())
		{
			out << YAML::Key << name << YAML::Value << "";
			return;
		}

		// fallback for now to fix build: return normalized path
		std::string relativePath = std::filesystem::path(path).generic_string();
		out << YAML::Key << name << YAML::Value << relativePath;
	}

	inline void SerializeHandle(YAML::Emitter& out, const char* name, uint64_t handle)
	{
		out << YAML::Key << name << YAML::Value << handle;
	}

	// --- YAML Deserialization Helpers ---

	template <typename T> inline void DeserializeProperty(YAML::Node node, const char* name, T& value)
	{
		if (node.IsMap() && node[name])
		{
			if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t> || std::is_same_v<T, unsigned char> ||
						  std::is_same_v<T, char>)
			{
				value = static_cast<T>(node[name].as<int>(static_cast<int>(value)));
			}
			else
			{
				value = node[name].as<T>(value);
			}
		}
	}

	inline void DeserializePath(YAML::Node node, const char* name, std::string& path)
	{
		if (node.IsMap() && node[name])
		{
			std::string pathValue = node[name].as<std::string>(path);
			if (pathValue.empty())
			{
				path = "";
				return;
			}
			path = std::filesystem::path(pathValue).generic_string();
		}
	}

	inline void DeserializeHandle(YAML::Node node, const char* name, uint64_t& handle)
	{
		if (node.IsMap() && node[name])
		{
			handle = node[name].as<uint64_t>(handle);
		}
	}

	inline void DeserializePath(YAML::Node node, const char* name, std::filesystem::path& path)
	{
		std::string pathStr;
		DeserializePath(node, name, pathStr);
		if (!pathStr.empty())
		{
			path = pathStr;
		}
	}

	// ========================================================================
	// PropertyArchive - Declarative Serialization/Deserialization
	// ========================================================================

	class PropertyArchive : public IPropertyArchiveBase
	{
	public:
		PropertyArchive(YAML::Emitter& emitter)
			: m_Mode(ReflectionMode::Serialize),
			  m_Out(&emitter)
		{
		}

		PropertyArchive(YAML::Node node)
			: m_Mode(ReflectionMode::Deserialize),
			  m_Out(nullptr),
			  m_Node(node)
		{
		}

		virtual ReflectionMode GetReflectionMode() const override
		{
			return m_Mode;
		}

		virtual bool Property(const char* name, int& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, float& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, bool& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, std::string& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, glm::vec2& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, glm::vec3& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, glm::vec4& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, Color& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}
		virtual bool Property(const char* name, uint64_t& value, const PropertyMeta& meta = {}) override
		{
			return Handle(name, value, meta);
		}
		virtual bool Enum(const char* name, int& value, const char** names, int count,
						  const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}

		virtual bool StringEnum(const char* name, std::string& value, const std::vector<std::string>& options,
								const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value);
		}

		virtual bool Handle(const char* name, uint64_t& handle, const PropertyMeta& meta = {}) override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				SerializeHandle(*m_Out, name, handle);
			}
			else
			{
				DeserializeHandle(m_Node, name, handle);
			}
			return false;
		}
		virtual bool File(const char* name, std::string& path, const char* extensions = nullptr,
						  const PropertyMeta& meta = {}) override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				SerializePath(*m_Out, name, path);
			}
			else
			{
				DeserializePath(m_Node, name, path);
			}
			return false;
		}

		virtual void BeginSequence(const char* name, size_t& size) override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				if (name)
				{
					*m_Out << YAML::Key << name;
				}
				*m_Out << YAML::Value << YAML::BeginSeq;
				m_SequenceStack.push_back({YAML::Node(), 0});
			}
			else
			{
				if (name && m_Node[name] && m_Node[name].IsSequence())
				{
					size = m_Node[name].size();
					m_SequenceStack.push_back({m_Node[name], 0});
				}
				else
				{
					m_SequenceStack.push_back({YAML::Node(), 0});
				}
			}
		}

		virtual void EndSequence() override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				*m_Out << YAML::EndSeq;
			}
			m_SequenceStack.pop_back();
		}

		virtual bool Nested(const char* name, std::function<void(IPropertyArchiveBase&)> callback) override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				if (name)
				{
					*m_Out << YAML::Key << name << YAML::Value << YAML::BeginMap;
				}
				else
				{
					*m_Out << YAML::BeginMap;
				}
				PropertyArchive nestedArchive(*m_Out);
				callback(static_cast<IPropertyArchiveBase&>(nestedArchive));
				*m_Out << YAML::EndMap;
			}
			else
			{
				YAML::Node nodeToUse;
				if (!m_SequenceStack.empty() && m_SequenceStack.back().Node && m_SequenceStack.back().Node.IsSequence())
				{
					auto& seq = m_SequenceStack.back();
					if (seq.Index < seq.Node.size())
					{
						nodeToUse = seq.Node[seq.Index++];
					}
				}
				else if (name && m_Node[name])
				{
					nodeToUse = m_Node[name];
				}

				if (nodeToUse && nodeToUse.IsMap())
				{
					PropertyArchive nestedArchive(nodeToUse);
					callback(static_cast<IPropertyArchiveBase&>(nestedArchive));
				}
			}
			return false;
		}

		virtual void BeginMap(const char* name, size_t& size) override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				if (name)
				{
					*m_Out << YAML::Key << name << YAML::Value << YAML::BeginMap;
				}
				m_MapStack.push_back({YAML::Node(), 0, false});
			}
			else
			{
				YAML::Node mapNode;
				if (name && m_Node[name] && m_Node[name].IsMap())
				{
					mapNode = m_Node[name];
					size = mapNode.size();
				}
				else
				{
					size = 0;
				}
				m_MapStack.push_back({mapNode, 0, true});
			}
		}

		virtual void EndMap() override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				*m_Out << YAML::EndMap;
			}
			if (!m_MapStack.empty())
			{
				m_MapStack.pop_back();
			}
		}

		virtual bool MapNextKey(std::string& key) override
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				*m_Out << YAML::Key << key;
				return true;
			}
			else
			{
				if (m_MapStack.empty())
				{
					return false;
				}
				auto& state = m_MapStack.back();
				if (!state.Node || !state.IsMap)
				{
					return false;
				}

				auto it = state.Node.begin();
				for (size_t i = 0; i < state.Index && it != state.Node.end(); ++i, ++it)
				{
				}
				if (it == state.Node.end())
				{
					return false;
				}

				key = it->first.as<std::string>();
				state.Index++;
				return true;
			}
		}

	private:
		template <typename T> bool PropertyInternal(const char* name, T& value)
		{
			if (m_Mode == ReflectionMode::Serialize)
			{
				if (name)
				{
					SerializeProperty(*m_Out, name, value);
				}
				else
				{
					*m_Out << value;
				}
			}
			else
			{
				if (!m_SequenceStack.empty() && m_SequenceStack.back().Node && m_SequenceStack.back().Node.IsSequence())
				{
					auto& seq = m_SequenceStack.back();
					if (seq.Index < seq.Node.size())
					{
						value = seq.Node[seq.Index++].as<T>(value);
					}
				}
				else if (name && m_Node.IsMap())
				{
					DeserializeProperty(m_Node, name, value);
				}
			}
			return false;
		}

		struct SequenceState
		{
			YAML::Node Node;
			size_t Index;
		};
		std::vector<SequenceState> m_SequenceStack;

		struct MapState
		{
			YAML::Node Node;
			size_t Index;
			bool IsMap;
		};
		std::vector<MapState> m_MapStack;

		ReflectionMode m_Mode;
		YAML::Emitter* m_Out = nullptr;
		YAML::Node m_Node;
	};
} // namespace Chained::Serialization

#endif // CH_SERIALIZATION_UTILS_H
