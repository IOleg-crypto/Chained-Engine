#ifndef CH_SCENE_SERIALIZER_H
#define CH_SCENE_SERIALIZER_H

#include "engine/scene/scene.h"

namespace Chained
{
	class Project;

	// Serializes and deserializes a single scene instance; the scene is not owned.
	class SceneSerializer
	{
	public:
		// Creates a serializer bound to the provided scene instance.
		SceneSerializer(Scene* scene, Project* project = nullptr);

		// Serializes the bound scene to disk. Returns false on failure.
		bool Serialize(const std::string& filepath);
		// Deserializes the scene from disk. Returns false on failure.
		bool Deserialize(const std::string& filepath);

		// Returns the last serialization or deserialization error message.
		const std::string& GetLastError() const
		{
			return m_LastError;
		}

		// Serializes the bound scene to a YAML string.
		std::string SerializeToString();
		// Deserializes the scene from a YAML string. Returns false on failure.
		bool DeserializeFromString(const std::string& yaml);

	private:
		Scene* m_Scene;
		Project* m_Project = nullptr;
		std::string m_LastError;
	};
} // namespace Chained

#endif // CH_SCENE_SERIALIZER_H
