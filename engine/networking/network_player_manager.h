#ifndef CH_NETWORK_PLAYER_MANAGER_H
#define CH_NETWORK_PLAYER_MANAGER_H

#include "network_types.h"
#include "net_packet.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace Chained
{

	class NetworkPlayerManager
	{
	public:
		NetworkPlayerManager() = default;
		~NetworkPlayerManager() = default;

		NetworkPlayerManager(const NetworkPlayerManager&) = delete;
		NetworkPlayerManager& operator=(const NetworkPlayerManager&) = delete;

		void Reset();

		void SetLocalPlayerInfo(const char* name, uint8_t skinIndex);
		std::string GetLocalPlayerName() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_LocalPlayerName;
		}
		uint8_t GetLocalSkinIndex() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_LocalSkinIndex;
		}

		uint64_t GetLocalNetworkID() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_LocalNetworkID;
		}
		void SetLocalNetworkID(uint64_t id)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_LocalNetworkID = id;
		}

		void SetHostNetworkID(uint64_t id)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_HostNetworkID = id;
		}

		uint64_t GetNetworkIDForConnection(int clientIndex) const;
		void OnClientConnected(int clientIndex);
		void OnClientDisconnected(int clientIndex);

		std::vector<PlayerNetInfo> GetPlayerList() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_PlayerList;
		}

		void SetPlayerListFromMessage(const std::vector<PlayerNetInfo>& list);
		void UpdatePlayerInfo(uint64_t networkID, const char* name, uint8_t skinIndex);

		size_t GetClientCount() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_ClientIndexToNetworkID.size();
		}
		std::vector<int> GetClients() const;

		void AddHostSelf(uint64_t hostNetworkID, const std::string& localPlayerName, uint8_t localSkinIndex);

	private:
		mutable std::mutex m_Mutex;
		std::vector<PlayerNetInfo> m_PlayerList;
		std::unordered_map<int, uint64_t> m_ClientIndexToNetworkID;
		uint64_t m_LocalNetworkID = 0;
		std::string m_LocalPlayerName;
		uint8_t m_LocalSkinIndex = 0;
		uint64_t m_HostNetworkID = 1;
		uint64_t m_NextNetworkID = 2;
	};

} // namespace Chained

#endif // CH_NETWORK_PLAYER_MANAGER_H
