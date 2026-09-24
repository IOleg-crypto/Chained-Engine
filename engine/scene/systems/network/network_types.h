#ifndef CH_SCENE_NETWORK_TYPES_H
#define CH_SCENE_NETWORK_TYPES_H

#include "engine/networking/net_packet.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

namespace Chained
{
	// ---- Network Constants ----
	namespace NetworkConstants
	{
		// Maximum distance (m) between local and server position before snap-correction kicks in.
		static constexpr float kMaxCorrectionDistance = 10.0f;

		// 64 Hz (Valve / Source tickrate standard)
		static constexpr float kNetworkTickInterval = 1.0f / 64.0f;

		// Half the uint32_t tick range guard for counter wraparounds.
		static constexpr uint32_t kTickWrapGuard = 100'000u;

		// Client input timeout in seconds (stops ghost coasting on dropped UDP packets)
		static constexpr float kClientInputTimeout = 0.25f;
	} // namespace NetworkConstants

	// ---- State structures ----

	struct PendingNetworkState
	{
		uint64_t NetworkID = 0;
		uint32_t LastTick = 0;
		glm::vec3 TargetPosition = {0, 0, 0};	 // Authoritative server position
		glm::quat TargetRotation = {1, 0, 0, 0}; // Authoritative server rotation
		glm::vec3 TargetVelocity = {0, 0, 0};
		glm::vec3 RenderPosition = {0, 0, 0};	 // Smoothly-interpolated render pos (never hard-reset)
		glm::quat RenderRotation = {1, 0, 0, 0}; // Smoothly-interpolated render rot
		bool RenderInitialized = false;			 // First packet: snap render pos to server pos
		bool IsGrounded = false;
		uint8_t ActionFlags = 0;
	};

	struct ProcessedInput
	{
		uint64_t NetworkID = 0;
		float MoveX = 0.0f;
		float MoveZ = 0.0f;
		uint8_t ActionFlags = 0;
		float MouseX = 0.0f;
		float MouseY = 0.0f;
	};

} // namespace Chained

#endif // CH_SCENE_NETWORK_TYPES_H
