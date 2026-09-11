#ifndef CH_SCRIPTING_COMPONENTS_H
#define CH_SCRIPTING_COMPONENTS_H
#include "engine/reflection/reflection_rfl.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Chained
{

	class ScriptEngine;

	enum class ScriptFieldType
	{
		None = 0,
		Float,
		Int,
		Bool,
		String,
		Vec2,
		Vec3,
		Vec4,
		Color,
		Entity
	};

	struct ScriptField
	{
		ScriptFieldType Type = ScriptFieldType::None;
		std::string Name;
		std::variant<float, int, bool, std::string, glm::vec2, glm::vec3, glm::vec4, Chained::Color, uint64_t> Value;
	};

	CH_MARK_RFL(ScriptField);

	// Represents a single C# script instance attached to an entity.
	struct ManagedScriptInstance
	{
		std::string ClassName;
		std::map<std::string, ScriptField> Fields; // Persistent fields
		int Priority = 0;						   // Lower = executes first
		bool IsInstantiated = false;
		bool NeedsStart = true;
		bool InstantiateTried = false; // set once C++ attempted instantiation (success or not)

		ManagedScriptInstance ClonePersistent() const
		{
			ManagedScriptInstance copy;
			copy.ClassName = ClassName;
			copy.Fields = Fields;
			copy.Priority = Priority;
			return copy;
		}

		void ResetRuntimeState()
		{
			IsInstantiated = false;
			NeedsStart = true;
			InstantiateTried = false;
		}

		bool HasInstance() const
		{
			return IsInstantiated;
		}

		template <typename T_Archive> void Reflect(::Chained::Properties<T_Archive>& props);
	};

	// ECS component that enables managed (C#) scripting for an entity.
	struct ManagedScriptComponent
	{
		std::vector<ManagedScriptInstance> Scripts;

		ManagedScriptComponent ClonePersistent() const
		{
			ManagedScriptComponent copy;
			copy.Scripts.reserve(Scripts.size());
			for (const auto& script : Scripts)
			{
				copy.Scripts.push_back(script.ClonePersistent());
			}
			return copy;
		}

		static const char* GetStaticName()
		{
			return "ManagedScriptComponent";
		}
	};
	CH_MARK_RFL(ManagedScriptComponent)
} // namespace Chained

#endif // CH_SCRIPTING_COMPONENTS_H
