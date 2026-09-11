#ifndef CH_PROJECT_SERIALIZER_H
#define CH_PROJECT_SERIALIZER_H

#include "engine/project/project.h"
#include <filesystem>
#include <memory>

namespace Chained
{
	namespace EditorProjectSerializer
	{
		bool Serialize(const std::shared_ptr<Project>& project, const std::filesystem::path& filepath);
		bool Deserialize(const std::shared_ptr<Project>& project, const std::filesystem::path& filepath);
	} // namespace EditorProjectSerializer
} // namespace Chained

#endif // CH_PROJECT_SERIALIZER_H
