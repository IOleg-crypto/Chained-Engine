#include "script_glue_network.h"
#include "engine/networking/network_service.h"
#include "engine/scene/systems/network_system.h"
#include "engine/core/service_locator.h"

namespace Chained
{
	namespace
	{
		/// Escape a string for embedding in a JSON string literal. Player nicknames and
		/// chat messages are user-supplied — an unescaped quote or backslash would
		/// corrupt the document the managed side parses.
		std::string EscapeJson(const char* text)
		{
			std::string out;
			if (!text)
			{
				return out;
			}

			for (const char* p = text; *p; ++p)
			{
				const unsigned char c = static_cast<unsigned char>(*p);
				switch (c)
				{
				case '"':
					out += "\\\"";
					break;
				case '\\':
					out += "\\\\";
					break;
				case '\n':
					out += "\\n";
					break;
				case '\r':
					out += "\\r";
					break;
				case '\t':
					out += "\\t";
					break;
				default:
					if (c < 0x20)
					{
						char buf[7];
						std::snprintf(buf, sizeof(buf), "\\u%04x", c);
						out += buf;
					}
					else
					{
						out += static_cast<char>(c);
					}
					break;
				}
			}

			return out;
		}

		std::string EscapeJson(const std::string& text)
		{
			return EscapeJson(text.c_str());
		}
	} // namespace

	CH_SCRIPT_FUNC void Network_HostGame(uint16_t port, int maxClients)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net)
		{
			return;
		}
		CH_CORE_INFO("[Script] Network.HostGame(port={}, maxClients={})", port, maxClients);
		net->HostGame(port, maxClients);
	}

	CH_SCRIPT_FUNC void Network_ConnectTo(const Coral::UCChar* ip, uint16_t port)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !ip)
		{
			return;
		}
		std::string ipStr = ch_u16_to_string(ip);
		CH_CORE_INFO("[Script] Network.ConnectTo(ip='{}', port={})", ipStr, port);
		net->ConnectTo(ipStr, port);
	}

	CH_SCRIPT_FUNC void Network_Disconnect()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net)
		{
			return;
		}
		CH_CORE_INFO("[Script] Network.Disconnect()");
		net->Disconnect();
	}

	CH_SCRIPT_FUNC uint8_t Network_IsHost()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->IsHost();
	}

	CH_SCRIPT_FUNC uint8_t Network_IsClient()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->IsClient();
	}

	CH_SCRIPT_FUNC uint8_t Network_IsConnected()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->IsConnected();
	}

	CH_SCRIPT_FUNC uint8_t Network_IsFullyConnected()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->IsFullyConnected();
	}

	CH_SCRIPT_FUNC int Network_GetClientCount()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net ? static_cast<int>(net->GetClientCount()) : 0;
	}

	CH_SCRIPT_FUNC int Network_GetMaxClients()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net ? net->GetMaxClients() : 0;
	}

	CH_SCRIPT_FUNC int Network_GetRole()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net ? static_cast<int>(net->GetRole()) : 0;
	}

	CH_SCRIPT_FUNC void Network_GetListenAddress(char* outBuffer, int bufferSize)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !outBuffer || bufferSize <= 0)
		{
			if (outBuffer && bufferSize > 0)
			{
				outBuffer[0] = '\0';
			}
			return;
		}
		std::string ip = net->GetListenAddress();
		std::strncpy(outBuffer, ip.c_str(), bufferSize - 1);
		outBuffer[bufferSize - 1] = '\0';
	}

	CH_SCRIPT_FUNC void Network_GetPublicAddress(char* outBuffer, int bufferSize)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !outBuffer || bufferSize <= 0)
		{
			if (outBuffer && bufferSize > 0)
			{
				outBuffer[0] = '\0';
			}
			return;
		}
		std::string addr = net->GetPublicAddress();
		std::strncpy(outBuffer, addr.c_str(), bufferSize - 1);
		outBuffer[bufferSize - 1] = '\0';
	}

	CH_SCRIPT_FUNC void Network_BroadcastSceneChange(const Coral::UCChar* scenePath)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !scenePath)
		{
			return;
		}
		std::string path = ch_u16_to_string(scenePath);
		CH_CORE_INFO("[Script] Network.BroadcastSceneChange(path='{}')", path);
		net->BroadcastSceneChange(path.c_str());
	}

	CH_SCRIPT_FUNC uint8_t Network_HasPendingSceneChange()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->HasPendingSceneChange();
	}

	CH_SCRIPT_FUNC void Network_GetPendingSceneChange(char* outBuffer, int bufferSize)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !outBuffer || bufferSize <= 0)
		{
			if (outBuffer && bufferSize > 0)
			{
				outBuffer[0] = '\0';
			}
			return;
		}
		const std::string& path = net->GetPendingSceneChange();
		std::strncpy(outBuffer, path.c_str(), bufferSize - 1);
		outBuffer[bufferSize - 1] = '\0';
	}

	CH_SCRIPT_FUNC void Network_ClearPendingSceneChange()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (net)
		{
			net->ClearPendingSceneChange();
		}
	}

	// ---- Player list ----

	CH_SCRIPT_FUNC void Network_SetLocalPlayerInfo(const Coral::UCChar* name, uint8_t skinIndex)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !name)
		{
			return;
		}
		std::string nameStr = ch_u16_to_string(name);
		net->SetLocalPlayerInfo(nameStr.c_str(), skinIndex);
	}

	CH_SCRIPT_FUNC void Network_SendPlayerInfo(const Coral::UCChar* name, uint8_t skinIndex)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !name)
		{
			return;
		}
		std::string nameStr = ch_u16_to_string(name);
		net->SendPlayerInfoToHost(nameStr.c_str(), skinIndex);
	}

	CH_SCRIPT_FUNC int Network_GetPlayerCount()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net ? static_cast<int>(net->GetPlayerList().size()) : 0;
	}

	CH_SCRIPT_FUNC void Network_GetPlayerListJSON(char* outBuffer, int bufferSize)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !outBuffer || bufferSize <= 0)
		{
			if (outBuffer && bufferSize > 0)
			{
				outBuffer[0] = '\0';
			}
			return;
		}

		const auto& players = net->GetPlayerList();
		// Build a simple JSON array: [{"id":1,"name":"Player","skin":0,"isHost":1}, ...]
		std::string json = "[";
		for (size_t i = 0; i < players.size(); ++i)
		{
			if (i > 0)
			{
				json += ",";
			}
			json += "{\"id\":" + std::to_string(players[i].NetworkID);
			json += ",\"name\":\"" + EscapeJson(players[i].Name) + "\"";
			json += ",\"skin\":" + std::to_string((int)players[i].SkinIndex);
			json += ",\"isHost\":" + std::to_string((int)players[i].IsHost) + "}";
		}
		json += "]";

		std::strncpy(outBuffer, json.c_str(), bufferSize - 1);
		outBuffer[bufferSize - 1] = '\0';
	}

	CH_SCRIPT_FUNC uint64_t Network_GetLocalNetworkID()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net ? net->GetLocalNetworkID() : 0;
	}

	// ---- Chat ----

	CH_SCRIPT_FUNC void Network_SendChatMessage(const Coral::UCChar* message)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !message)
		{
			return;
		}
		std::string msg = ch_u16_to_string(message);
		net->SendChatMessage(msg.c_str());
	}

	CH_SCRIPT_FUNC uint8_t Network_HasPendingChat()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->HasPendingChatMessages();
	}

	CH_SCRIPT_FUNC void Network_GetPendingChatJSON(char* outBuffer, int bufferSize)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !outBuffer || bufferSize <= 0)
		{
			if (outBuffer && bufferSize > 0)
			{
				outBuffer[0] = '\0';
			}
			return;
		}

		const auto& messages = net->GetPendingChatMessages();
		// JSON array: [{"sender":"Name","message":"text"}, ...]
		std::string json = "[";
		for (size_t i = 0; i < messages.size(); ++i)
		{
			if (i > 0)
			{
				json += ",";
			}
			json += "{\"sender\":\"" + EscapeJson(messages[i].SenderName) + "\"";
			json += ",\"message\":\"" + EscapeJson(messages[i].Message) + "\"}";
		}
		json += "]";

		std::strncpy(outBuffer, json.c_str(), bufferSize - 1);
		outBuffer[bufferSize - 1] = '\0';
	}

	CH_SCRIPT_FUNC void Network_ClearPendingChat()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (net)
		{
			net->ClearPendingChatMessages();
		}
	}

	// ---- Prefab ----

	CH_SCRIPT_FUNC void Network_SetPlayerPrefab(const Coral::UCChar* path)
	{
		if (!path)
		{
			return;
		}
		std::string pathStr = ch_u16_to_string(path);
		ServiceLocator::Get<NetworkSystem>()->SetPlayerPrefab(pathStr.c_str());
		CH_CORE_INFO("[Script] Network.SetPlayerPrefab(path='{}')", pathStr);
	}

	// ---- UPnP ----

	CH_SCRIPT_FUNC uint8_t Network_IsUpnpAvailable()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->IsUpnpAvailable();
	}

	// ── STUN / NAT Traversal ─────────────────────────────────────────────

	CH_SCRIPT_FUNC uint8_t Network_HasStunResult()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		return net && net->HasStunResult();
	}

	CH_SCRIPT_FUNC void Network_GetStunPublicAddress(char* outBuffer, int bufferSize)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !outBuffer || bufferSize <= 0)
		{
			if (outBuffer && bufferSize > 0)
			{
				outBuffer[0] = '\0';
			}
			return;
		}
		std::string addr = net->GetPublicAddress();
		std::strncpy(outBuffer, addr.c_str(), bufferSize - 1);
		outBuffer[bufferSize - 1] = '\0';
	}

	CH_SCRIPT_FUNC void Network_StartHolePunch(const Coral::UCChar* remoteIP, uint16_t remotePort)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !remoteIP)
		{
			return;
		}
		std::string ip = ch_u16_to_string(remoteIP);
		CH_CORE_INFO("[Script] Network.StartHolePunch(ip='{}', port={})", ip, remotePort);
		net->StartHolePunch(ip, remotePort);
	}

	CH_SCRIPT_FUNC void Network_QueryStun(uint16_t localPort)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net)
		{
			return;
		}
		CH_CORE_INFO("[Script] Network.QueryStun(localPort={})", localPort);
		net->QueryStunPublicEndpoint(localPort);
	}

	CH_SCRIPT_FUNC uint32_t Network_GetPing()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !net->IsClient())
		{
			return 0;
		}
		return net->GetPing();
	}

} // namespace Chained
