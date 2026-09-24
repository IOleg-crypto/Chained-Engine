#include "engine/scene/systems/network/network_input_controller.h"
#include "engine/core/input.h"
#include "engine/core/key_codes.h"
#include "engine/core/log.h"
#include "engine/networking/network_service.h"
#include "engine/networking/net_packet.h"
#include "engine/scene/scene.h"
#include "engine/scene/components/core/transform_component.h"
#include "engine/scene/components/render/camera_component.h"
#include "engine/scene/components/gameplay/player_component.h"
#include "engine/scene/components/gameplay/network_identity_component.h"
#include "engine/scene/components/physics/physics_component.h"
#include "engine/scene/systems/transform_system.h"
#include <imgui.h>
#include <cmath>

namespace Chained
{
	NetworkInputController::NetworkInputController()
	{
		Reset();
	}

	void NetworkInputController::Reset()
	{
		m_PendingInputs.clear();
		m_ActiveClientInputs.clear();
		m_ActiveClientInputTimers.clear();
		m_LastActionFlags.clear();
		m_WarnedInputNetID.clear();
		m_ClientTick = 0;
	}

	void NetworkInputController::ProcessInputStateMessage(InputStateMessage* msg, uint64_t networkID)
	{
		if (!msg || networkID == 0)
		{
			return;
		}

		ProcessedInput input;
		input.NetworkID = networkID;
		input.MoveX = msg->MoveX;
		input.MoveZ = msg->MoveZ;
		input.ActionFlags = msg->ActionFlags;
		input.MouseX = msg->MouseX;
		input.MouseY = msg->MouseY;

		m_ActiveClientInputs[networkID] = input;
		m_ActiveClientInputTimers[networkID] = 0.0f;
	}

	void NetworkInputController::ApplyHostInputs(entt::registry& reg, Timestep ts)
	{
		m_PendingInputs.clear();
		if (m_ActiveClientInputs.empty())
		{
			return;
		}

		float dt = static_cast<float>(ts);

		for (auto it = m_ActiveClientInputs.begin(); it != m_ActiveClientInputs.end();)
		{
			uint64_t networkID = it->first;
			auto& input = it->second;
			float& timer = m_ActiveClientInputTimers[networkID];
			timer += dt;

			// If no input received for more than 250ms, drop it
			if (timer > NetworkConstants::kClientInputTimeout)
			{
				it = m_ActiveClientInputs.erase(it);
				m_ActiveClientInputTimers.erase(networkID);
				continue;
			}

			entt::entity targetEntity = entt::null;
			auto view = reg.view<NetworkIdentityComponent>();
			for (auto entity : view)
			{
				auto& netID = view.get<NetworkIdentityComponent>(entity);
				if (netID.NetworkID == networkID)
				{
					targetEntity = entity;
					break;
				}
			}

			if (targetEntity == entt::null || !reg.valid(targetEntity))
			{
				++it;
				continue;
			}

			if (reg.all_of<PlayerComponent, RigidBodyComponent>(targetEntity))
			{
				auto& player = reg.get<PlayerComponent>(targetEntity);
				auto& rb = reg.get<RigidBodyComponent>(targetEntity);

				if (rb.Handle != kInvalidPhysicsBody)
				{
					float speed = player.MovementSpeed;
					if (input.ActionFlags & InputAction_Sprint)
					{
						speed *= 2.0f;
					}

					float moveX = input.MoveX * speed;
					float moveZ = input.MoveZ * speed;
					rb.Velocity = glm::vec3(moveX, rb.Velocity.y, moveZ);

					// Rotate remote avatar to face movement direction
					if (auto* tc = reg.try_get<TransformComponent>(targetEntity))
					{
						if (std::abs(moveX) > 0.001f || std::abs(moveZ) > 0.001f)
						{
							float yaw = std::atan2(moveX, moveZ);
							TransformSystem::SetRotation(*tc, glm::vec3(0.0f, yaw, 0.0f));
						}
					}

					if ((input.ActionFlags & InputAction_Jump) && rb.IsGrounded)
					{
						rb.Velocity.y = player.JumpForce;
						rb.VelocityForced = true;
						input.ActionFlags &= ~InputAction_Jump;
					}

					m_LastActionFlags[networkID] = input.ActionFlags;
				}
			}
			else
			{
				if (m_WarnedInputNetID.insert(networkID).second)
				{
					CH_CORE_WARN(
						"Network: ApplyHostInputs — entity netID={} missing PlayerComponent/RigidBodyComponent",
						networkID);
				}
			}

			++it;
		}
	}

	void NetworkInputController::CollectAndSendInput(Network* net, float dt, Scene* scene)
	{
		if (!net || !net->IsClient())
		{
			return;
		}

		InputStateMessage msg;
		msg.Tick = m_ClientTick++;
		msg.DeltaTime = dt;

		float rawX = 0.0f;
		float rawZ = 0.0f;
		uint8_t flags = 0;

		const bool captureKeyboard = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
		if (!captureKeyboard)
		{
			if (Core::Input::IsKeyDown(KeyCode::W))
			{
				rawZ += 1.0f;
			}
			if (Core::Input::IsKeyDown(KeyCode::S))
			{
				rawZ -= 1.0f;
			}
			if (Core::Input::IsKeyDown(KeyCode::A))
			{
				rawX -= 1.0f;
			}
			if (Core::Input::IsKeyDown(KeyCode::D))
			{
				rawX += 1.0f;
			}

			if (Core::Input::IsKeyPressed(KeyCode::Space))
			{
				flags |= InputAction_Jump;
			}
			if (Core::Input::IsKeyDown(KeyCode::LeftShift))
			{
				flags |= InputAction_Sprint;
			}
		}

		float moveX = rawX;
		float moveZ = rawZ;
		if (scene)
		{
			auto camView = scene->GetRegistry().view<CameraComponent, TransformComponent>();
			for (auto entity : camView)
			{
				auto& cam = camView.get<CameraComponent>(entity);
				if (!cam.Primary)
				{
					continue;
				}
				auto& tc = camView.get<TransformComponent>(entity);
				glm::vec3 forward = tc.WorldTransform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
				forward.y = 0.0f;
				float fwdLen = glm::length(forward);
				if (fwdLen > 0.001f)
				{
					forward /= fwdLen;
				}
				else
				{
					forward = glm::vec3(0.0f, 0.0f, -1.0f);
				}
				glm::vec3 right = glm::vec3(-forward.z, 0.0f, forward.x);

				moveX = rawX * right.x + rawZ * forward.x;
				moveZ = rawX * right.z + rawZ * forward.z;
				break;
			}
		}

		float len = std::sqrt(moveX * moveX + moveZ * moveZ);
		if (len > 0.001f)
		{
			moveX /= len;
			moveZ /= len;
		}

		msg.MoveX = moveX;
		msg.MoveZ = moveZ;
		msg.ActionFlags = flags;

		glm::vec2 mouseDelta = Core::Input::GetMouseDelta();
		msg.MouseX = mouseDelta.x;
		msg.MouseY = mouseDelta.y;

		ByteWriter w;
		msg.Encode(w);
		net->SendToServer(MessageType_InputState, w.Data().data(), w.Data().size(), false);
	}

	const std::vector<ProcessedInput>& NetworkInputController::GetPendingInputs() const
	{
		return m_PendingInputs;
	}

	void NetworkInputController::ClearPendingInputs()
	{
		m_PendingInputs.clear();
	}

} // namespace Chained
