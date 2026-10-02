#ifndef CH_NETWORK_INTERPOLATION_COMPONENT_H
#define CH_NETWORK_INTERPOLATION_COMPONENT_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Chained
{
	struct NetworkInterpolationComponent
	{
		glm::vec3 TargetPosition{0.0f};
		glm::quat TargetRotation{1.0f, 0.0f, 0.0f, 0.0f};
		glm::vec3 TargetVelocity{0.0f};
		glm::vec3 RenderPosition{0.0f};
		glm::quat RenderRotation{1.0f, 0.0f, 0.0f, 0.0f};
		bool Initialized = false;
	};
} // namespace Chained

#endif // CH_NETWORK_INTERPOLATION_COMPONENT_H
