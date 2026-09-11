#include "network_player_manager.h"

namespace Chained
{
	void NetworkPlayerManager::Reset()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		std::string savedName = m_LocalPlayerName;
		uint8_t savedSkin = m_LocalSkinIndex;

		m_PlayerList.clear();
		m_ClientIndexToNetworkID.clear();
		m_LocalNetworkID = 0;
		m_NextNetworkID = 2;

		m_LocalPlayerName = savedName;
		m_LocalSkinIndex = savedSkin;
	}

	void NetworkPlayerManager::SetLocalPlayerInfo(const char* name, uint8_t skinIndex)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_LocalPlayerName = name ? name : "";
		m_LocalSkinIndex = skinIndex;
	}

	uint64_t NetworkPlayerManager::GetNetworkIDForConnection(int clientIndex) const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto it = m_ClientIndexToNetworkID.find(clientIndex);
		return it != m_ClientIndexToNetworkID.end() ? it->second : 0;
	}

	void NetworkPlayerManager::OnClientConnected(int clientIndex)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_ClientIndexToNetworkID.find(clientIndex) != m_ClientIndexToNetworkID.end())
		{
			return;
		}

		const uint64_t networkID = m_NextNetworkID++;
		m_ClientIndexToNetworkID[clientIndex] = networkID;

		PlayerNetInfo newPlayer;
		newPlayer.NetworkID = networkID;
		newPlayer.Name = "Player...";
		newPlayer.SkinIndex = 0;
		newPlayer.IsHost = 0;
		newPlayer.Ping = 0;
		m_PlayerList.push_back(newPlayer);

		CH_CORE_INFO("NetworkPlayerManager: Client connected (index={}, netID={}).", clientIndex, networkID);
	}

	void NetworkPlayerManager::OnClientDisconnected(int clientIndex)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		auto idIt = m_ClientIndexToNetworkID.find(clientIndex);
		if (idIt != m_ClientIndexToNetworkID.end())
		{
			const uint64_t networkID = idIt->second;
			m_ClientIndexToNetworkID.erase(idIt);

			auto entry = std::find_if(m_PlayerList.begin(), m_PlayerList.end(),
									  [networkID](const PlayerNetInfo& p) { return p.NetworkID == networkID; });

			if (entry != m_PlayerList.end())
			{
				CH_CORE_INFO("NetworkPlayerManager: Player '{}' disconnected.", entry->Name);
				m_PlayerList.erase(entry);
			}

			CH_CORE_INFO("NetworkPlayerManager: Client disconnected (index={}).", clientIndex);
		}
	}

	void NetworkPlayerManager::SetPlayerListFromMessage(const std::vector<PlayerNetInfo>& list)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (list.empty())
		{
			CH_CORE_WARN("NetworkPlayerManager: Received empty player list.");
		}
		if (list.size() > 64)
		{
			CH_CORE_WARN("NetworkPlayerManager: Received player list with {} entries, truncating to 64.", list.size());
		}

		m_PlayerList = list;
	}

	void NetworkPlayerManager::UpdatePlayerInfo(uint64_t networkID, const char* name, uint8_t skinIndex)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		for (auto& p : m_PlayerList)
		{
			if (p.NetworkID == networkID)
			{
				p.Name = name ? name : "";
				p.SkinIndex = skinIndex;
				CH_CORE_INFO("NetworkPlayerManager: Updated player info: '{}' (netID={}, skin={}).", p.Name,
							 p.NetworkID, (int)p.SkinIndex);
				return;
			}
		}
	}

	std::vector<int> NetworkPlayerManager::GetClients() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		std::vector<int> clients;
		for (auto& [idx, netID] : m_ClientIndexToNetworkID)
		{
			clients.push_back(idx);
		}
		return clients;
	}

	void NetworkPlayerManager::AddHostSelf(uint64_t hostNetworkID, const std::string& localPlayerName,
										   uint8_t localSkinIndex)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_LocalNetworkID = hostNetworkID;

		PlayerNetInfo self;
		self.NetworkID = hostNetworkID;
		self.Name = localPlayerName.empty() ? "Host" : localPlayerName;
		self.SkinIndex = localSkinIndex;
		self.IsHost = 1;
		self.Ping = 0;
		m_PlayerList.push_back(self);
	}
} // namespace Chained
