#include "engine/scene/systems/network/network_session_tracker.h"

namespace Chained
{
	NetworkSessionTracker::NetworkSessionTracker()
	{
		Reset();
	}

	void NetworkSessionTracker::Reset()
	{
		m_PeerToNetworkID.clear();
		m_PeerToAvatar.clear();
		m_NetworkIDToEntity.clear();
		m_PendingPlayerInfo.clear();
		m_LocalNetworkID = 0;
		m_PlayerPrefab = "prefab/player.chprefab";
	}

	void NetworkSessionTracker::RegisterPeer(int peer, uint64_t networkID)
	{
		m_PeerToNetworkID[peer] = networkID;
	}

	void NetworkSessionTracker::UnregisterPeer(int peer)
	{
		m_PeerToNetworkID.erase(peer);
		m_PeerToAvatar.erase(peer);
		m_PendingPlayerInfo.erase(peer);
	}

	uint64_t NetworkSessionTracker::GetNetworkIDForPeer(int peer) const
	{
		auto it = m_PeerToNetworkID.find(peer);
		return (it != m_PeerToNetworkID.end()) ? it->second : 0;
	}

	void NetworkSessionTracker::SetPeerAvatar(int peer, UUID avatarUUID)
	{
		m_PeerToAvatar[peer] = avatarUUID;
	}

	UUID NetworkSessionTracker::GetPeerAvatar(int peer) const
	{
		auto it = m_PeerToAvatar.find(peer);
		return (it != m_PeerToAvatar.end()) ? it->second : UUID(0);
	}

	void NetworkSessionTracker::RemovePeerAvatar(int peer)
	{
		m_PeerToAvatar.erase(peer);
	}

	void NetworkSessionTracker::MapNetworkIDToEntity(uint64_t networkID, UUID entityUUID)
	{
		m_NetworkIDToEntity[networkID] = entityUUID;
	}

	UUID NetworkSessionTracker::GetEntityForNetworkID(uint64_t networkID) const
	{
		auto it = m_NetworkIDToEntity.find(networkID);
		return (it != m_NetworkIDToEntity.end()) ? it->second : UUID(0);
	}

	void NetworkSessionTracker::RemoveNetworkID(uint64_t networkID)
	{
		m_NetworkIDToEntity.erase(networkID);
	}

	void NetworkSessionTracker::SetLocalNetworkID(uint64_t networkID)
	{
		m_LocalNetworkID = networkID;
	}

	uint64_t NetworkSessionTracker::GetLocalNetworkID() const
	{
		return m_LocalNetworkID;
	}

	void NetworkSessionTracker::SetPlayerPrefab(const std::string& path)
	{
		m_PlayerPrefab = path;
	}

	const std::string& NetworkSessionTracker::GetPlayerPrefab() const
	{
		return m_PlayerPrefab;
	}

	void NetworkSessionTracker::AddPendingPlayerInfo(int peer, const std::string& name, uint8_t skinIndex)
	{
		m_PendingPlayerInfo[peer] = {name, skinIndex};
	}

	void NetworkSessionTracker::RemovePendingPlayerInfo(int peer)
	{
		m_PendingPlayerInfo.erase(peer);
	}

	const std::unordered_map<int, std::pair<std::string, uint8_t>>& NetworkSessionTracker::GetPendingPlayerInfo() const
	{
		return m_PendingPlayerInfo;
	}

	const std::unordered_map<int, UUID>& NetworkSessionTracker::GetPeerToAvatarMap() const
	{
		return m_PeerToAvatar;
	}

	const std::unordered_map<uint64_t, UUID>& NetworkSessionTracker::GetNetworkIDToEntityMap() const
	{
		return m_NetworkIDToEntity;
	}

} // namespace Chained
