#ifndef SCRIPT_GLUE_NETWORK_H
#define SCRIPT_GLUE_NETWORK_H
#include "script_glue_internal.h"

namespace Chained
{
	class Network;
}

namespace Chained
{

	CH_SCRIPT_FUNC void Network_HostGame(uint16_t port, int maxClients);

	CH_SCRIPT_FUNC void Network_ConnectTo(const Coral::UCChar* ip, uint16_t port);

	CH_SCRIPT_FUNC void Network_Disconnect();

	CH_SCRIPT_FUNC uint8_t Network_IsHost();

	CH_SCRIPT_FUNC uint8_t Network_IsClient();

	CH_SCRIPT_FUNC uint8_t Network_IsConnected();
	CH_SCRIPT_FUNC uint8_t Network_IsFullyConnected();

	CH_SCRIPT_FUNC int Network_GetClientCount();

	CH_SCRIPT_FUNC int Network_GetMaxClients();

	CH_SCRIPT_FUNC int Network_GetRole();

	CH_SCRIPT_FUNC void Network_GetListenAddress(char* outBuffer, int bufferSize);

	CH_SCRIPT_FUNC void Network_GetPublicAddress(char* outBuffer, int bufferSize);

	CH_SCRIPT_FUNC void Network_BroadcastSceneChange(const Coral::UCChar* scenePath);

	CH_SCRIPT_FUNC uint8_t Network_HasPendingSceneChange();

	CH_SCRIPT_FUNC void Network_GetPendingSceneChange(char* outBuffer, int bufferSize);

	CH_SCRIPT_FUNC void Network_ClearPendingSceneChange();

	// Player list
	CH_SCRIPT_FUNC void Network_SetLocalPlayerInfo(const Coral::UCChar* name, uint8_t skinIndex);
	CH_SCRIPT_FUNC void Network_SendPlayerInfo(const Coral::UCChar* name, uint8_t skinIndex);
	CH_SCRIPT_FUNC int Network_GetPlayerCount();
	CH_SCRIPT_FUNC void Network_GetPlayerListJSON(char* outBuffer, int bufferSize);
	CH_SCRIPT_FUNC uint64_t Network_GetLocalNetworkID();

	// Chat
	CH_SCRIPT_FUNC void Network_SendChatMessage(const Coral::UCChar* message);
	CH_SCRIPT_FUNC uint8_t Network_HasPendingChat();
	CH_SCRIPT_FUNC void Network_GetPendingChatJSON(char* outBuffer, int bufferSize);
	CH_SCRIPT_FUNC void Network_ClearPendingChat();

	// Prefab
	CH_SCRIPT_FUNC void Network_SetPlayerPrefab(const Coral::UCChar* path);

	// UPnP
	CH_SCRIPT_FUNC uint8_t Network_IsUpnpAvailable();

	// STUN / NAT Traversal
	CH_SCRIPT_FUNC uint8_t Network_HasStunResult();
	CH_SCRIPT_FUNC void Network_GetStunPublicAddress(char* outBuffer, int bufferSize);
	CH_SCRIPT_FUNC void Network_StartHolePunch(const Coral::UCChar* remoteIP, uint16_t remotePort);
	CH_SCRIPT_FUNC void Network_QueryStun(uint16_t localPort);

	// Ping / RTT
	CH_SCRIPT_FUNC uint32_t Network_GetPing();

} // namespace Chained
#endif
