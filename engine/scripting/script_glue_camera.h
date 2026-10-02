#ifndef SCRIPT_GLUE_CAMERA_H
#define SCRIPT_GLUE_CAMERA_H
#include "script_glue_internal.h"
#include "engine/scene/systems/transform_system.h"

namespace Chained
{

	// ── Camera ────────────────────────────────────────────────────────────
	CH_SCRIPT_FUNC void Camera_GetForward(uint64_t entityID, glm::vec3* outForward);

	CH_SCRIPT_FUNC void Camera_GetRight(uint64_t entityID, glm::vec3* outRight);

	CH_SCRIPT_FUNC void Camera_GetOrbit(uint64_t entityID, float* yaw, float* pitch, float* distance);

	CH_SCRIPT_FUNC void Camera_SetOrbit(uint64_t entityID, float yaw, float pitch, float distance);

	CH_SCRIPT_FUNC uint8_t Camera_GetPrimary(uint64_t entityID);

	CH_SCRIPT_FUNC void Camera_SetPrimary(uint64_t entityID, uint8_t primary);

	CH_SCRIPT_FUNC uint8_t Camera_GetIsOrbit(uint64_t entityID);

	CH_SCRIPT_FUNC void Camera_SetIsOrbit(uint64_t entityID, uint8_t isOrbit);

	CH_SCRIPT_FUNC const Coral::UCChar* Camera_GetTargetTag(uint64_t entityID);

	CH_SCRIPT_FUNC void Camera_SetTargetTag(uint64_t entityID, const Coral::UCChar* tag);

} // namespace Chained
#endif