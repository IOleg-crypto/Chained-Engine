#ifndef CH_NETWORK_INPUT_CONTROLLER_H
#define CH_NETWORK_INPUT_CONTROLLER_H

#include "engine/scene/systems/network/network_types.h"
#include "engine/common/base.h"
#include "engine/common/timestep.h"
#include <entt/entt.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace Chained
{
	class Network;
	class Scene;
	struct InputStateMessage;

	class CH_API NetworkInputController
	{
	public:
		NetworkInputController();
		~NetworkInputController() = default;

		void Reset();

		void CollectAndSendInput(Network* net, float dt, Scene* scene);
		void ProcessInputStateMessage(InputStateMessage* msg, uint64_t networkID);
		void ApplyHostInputs(entt::registry& reg, Timestep ts);

		const std::vector<ProcessedInput>& GetPendingInputs() const;
		void ClearPendingInputs();

	private:
		std::vector<ProcessedInput> m_PendingInputs;
		std::unordered_map<uint64_t, ProcessedInput> m_ActiveClientInputs;
		std::unordered_map<uint64_t, float> m_ActiveClientInputTimers;
		std::unordered_map<uint64_t, uint8_t> m_LastActionFlags;
		std::unordered_set<uint64_t> m_WarnedInputNetID;
		uint32_t m_ClientTick = 0;
	};
} // namespace Chained

#endif // CH_NETWORK_INPUT_CONTROLLER_H
