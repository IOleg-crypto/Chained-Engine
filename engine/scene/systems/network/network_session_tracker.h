#ifndef CH_NETWORK_SESSION_TRACKER_H
#define CH_NETWORK_SESSION_TRACKER_H

#include "engine/scene/systems/network/network_types.h"
#include "engine/common/base.h"
#include "engine/common/uuid.h"
#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>

namespace Chained
{
	class CH_API NetworkSessionTracker
	{
	public:
		NetworkSessionTracker();
		~NetworkSessionTracker() = default;

		void Reset();

		void RegisterPeer(int peer, uint64_t networkID);
		void UnregisterPeer(int peer);
		uint64_t GetNetworkIDForPeer(int peer) const;

		void SetPeerAvatar(int peer, UUID avatarUUID);
		UUID GetPeerAvatar(int peer) const;
		void RemovePeerAvatar(int peer);

		void MapNetworkIDToEntity(uint64_t networkID, UUID entityUUID);
		UUID GetEntityForNetworkID(uint64_t networkID) const;
		void RemoveNetworkID(uint64_t networkID);

		void SetLocalNetworkID(uint64_t networkID);
		uint64_t GetLocalNetworkID() const;

		void SetPlayerPrefab(const std::string& path);
		const std::string& GetPlayerPrefab() const;

		void AddPendingPlayerInfo(int peer, const std::string& name, uint8_t skinIndex);
		void RemovePendingPlayerInfo(int peer);
		const std::unordered_map<int, std::pair<std::string, uint8_t>>& GetPendingPlayerInfo() const;

		const std::unordered_map<int, UUID>& GetPeerToAvatarMap() const;
		const std::unordered_map<uint64_t, UUID>& GetNetworkIDToEntityMap() const;

	private:
		std::unordered_map<int, uint64_t> m_PeerToNetworkID;
		std::unordered_map<int, UUID> m_PeerToAvatar;
		std::unordered_map<uint64_t, UUID> m_NetworkIDToEntity;
		std::unordered_map<int, std::pair<std::string, uint8_t>> m_PendingPlayerInfo;
		uint64_t m_LocalNetworkID = 0;
		std::string m_PlayerPrefab = "prefab/player.chprefab";
	};
} // namespace Chained

#endif // CH_NETWORK_SESSION_TRACKER_H
