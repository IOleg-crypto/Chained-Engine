#include "stun_client.h"
#include "engine/core/log.h"
#include "engine/common/platform_detection.h"

#if CH_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <cstring>
#include <thread>
#include <random>

namespace Chained
{
	StunClient::~StunClient()
	{
		Shutdown();
	}

	void StunClient::Shutdown()
	{
		if (m_QueryThread.joinable())
		{
			m_QueryThread.join();
		}
	}

	// RFC 5389 STUN constants
	static constexpr uint16_t STUN_METHOD_BINDING = 0x0001;
	static constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442u; // Bug #4 fix: must be uint32_t
	static constexpr uint16_t STUN_ATTR_MAPPED_ADDRESS = 0x0001;
	static constexpr uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020;
	static constexpr size_t STUN_HEADER_SIZE = 20;

	void StunClient::AddServer(const std::string& host, uint16_t port)
	{
		m_Servers.push_back({host, port});
	}

	bool StunClient::BuildBindingRequest(uint8_t* out, size_t* outLen, uint32_t* outTransactionId)
	{
		if (*outLen < STUN_HEADER_SIZE)
		{
			return false;
		}

		const uint32_t transactionId = std::mt19937{std::random_device{}()}();
		*outTransactionId = transactionId;

		// Bytes 0-1: Message Type (Binding Request = 0x0001)
		out[0] = 0x00;
		out[1] = 0x01;
		// Bytes 2-3: Message Length (0 — no attributes)
		out[2] = 0x00;
		out[3] = 0x00;
		// Bytes 4-7: Magic Cookie
		out[4] = 0x21;
		out[5] = 0x12;
		out[6] = 0xA4;
		out[7] = 0x42;
		// Bytes 8-11: Transaction ID (first 4 bytes), rest zeroed
		out[8] = (transactionId >> 24) & 0xFF;
		out[9] = (transactionId >> 16) & 0xFF;
		out[10] = (transactionId >> 8) & 0xFF;
		out[11] = transactionId & 0xFF;
		out[12] = out[13] = out[14] = out[15] = 0;
		out[16] = out[17] = out[18] = out[19] = 0;

		*outLen = STUN_HEADER_SIZE;
		return true;
	}

	bool StunClient::ParseBindingResponse(const uint8_t* data, size_t len, uint32_t expectedTransactionId,
										  std::string& outIP, uint16_t& outPort)
	{
		if (len < STUN_HEADER_SIZE)
		{
			return false;
		}

		// Message Type must be Binding Response (0x0101)
		if (((data[0] << 8) | data[1]) != 0x0101)
		{
			return false;
		}

		// Magic Cookie
		const uint32_t magicCookie = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
		if (magicCookie != STUN_MAGIC_COOKIE)
		{
			return false;
		}

		// Transaction ID (first 4 bytes only — we only store 32 bits)
		const uint32_t transId = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
		if (transId != expectedTransactionId)
		{
			return false;
		}

		const uint16_t msgLength = (data[2] << 8) | data[3];
		const size_t end = STUN_HEADER_SIZE + msgLength;

		// Parse attributes — prefer XOR-MAPPED-ADDRESS (0x0020) over MAPPED-ADDRESS (0x0001)
		for (size_t offset = STUN_HEADER_SIZE; offset + 4 <= end;)
		{
			const uint16_t attrType = (data[offset] << 8) | data[offset + 1];
			const uint16_t attrLen = (data[offset + 2] << 8) | data[offset + 3];
			offset += 4;

			if (offset + attrLen > end)
			{
				break;
			}

			if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS || attrType == STUN_ATTR_MAPPED_ADDRESS)
			{
				// Need at least: 1 reserved + 1 family + 2 port + 4 IP = 8 bytes
				if (attrLen < 8)
				{
					offset += (attrLen + 3) & ~3u;
					continue;
				}

				const uint8_t family = data[offset + 1];
				uint16_t port = (data[offset + 2] << 8) | data[offset + 3];
				uint8_t ip[4] = {data[offset + 4], data[offset + 5], data[offset + 6], data[offset + 7]};

				if (family == 0x01) // IPv4
				{
					if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS)
					{
						// RFC 5389 §15.2: IPv4 XORed only with magic cookie
						port ^= static_cast<uint16_t>(STUN_MAGIC_COOKIE >> 16); // Bug #3 fix
						ip[0] ^= (STUN_MAGIC_COOKIE >> 24) & 0xFF;
						ip[1] ^= (STUN_MAGIC_COOKIE >> 16) & 0xFF;
						ip[2] ^= (STUN_MAGIC_COOKIE >> 8) & 0xFF;
						ip[3] ^= STUN_MAGIC_COOKIE & 0xFF;
						// Bug #5 fix: do NOT XOR with transaction ID for IPv4
					}

					char ipStr[INET_ADDRSTRLEN];
					snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
					outIP = ipStr;
					outPort = port;
					return true;
				}
				else if (family == 0x02 && attrLen >= 20) // IPv6
				{
					uint8_t ip6[16];
					for (int i = 0; i < 16; ++i)
					{
						ip6[i] = data[offset + 4 + i];
					}

					if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS)
					{
						port ^= static_cast<uint16_t>(STUN_MAGIC_COOKIE >> 16);
						ip6[0] ^= (STUN_MAGIC_COOKIE >> 24) & 0xFF;
						ip6[1] ^= (STUN_MAGIC_COOKIE >> 16) & 0xFF;
						ip6[2] ^= (STUN_MAGIC_COOKIE >> 8) & 0xFF;
						ip6[3] ^= STUN_MAGIC_COOKIE & 0xFF;
						// Bytes 4-15: XOR with transaction ID bytes
						for (int i = 0; i < 12; ++i)
						{
							ip6[4 + i] ^= data[12 + i]; // transaction ID in-place from packet
						}
					}

					char ipStr[INET6_ADDRSTRLEN];
					if (inet_ntop(AF_INET6, ip6, ipStr, sizeof(ipStr)))
					{
						outIP = ipStr;
						outPort = port;
						return true;
					}
				}
			}

			offset += (attrLen + 3) & ~3u; // advance with 4-byte padding
		}

		return false;
	}

	StunClient::StunResult StunClient::QuerySingleServer(const StunServer& server, uint16_t localPort, int timeoutMs)
	{
		StunResult result;

		struct addrinfo hints = {}, *res = nullptr;
		hints.ai_family = AF_INET;		// Force IPv4 — avoids IPv6-on-IPv4-only issues
		hints.ai_socktype = SOCK_DGRAM; // UDP for STUN
		hints.ai_protocol = IPPROTO_UDP;

		if (getaddrinfo(server.Host.c_str(), nullptr, &hints, &res) != 0 || !res)
		{
			result.Error = "DNS failed for " + server.Host;
			CH_CORE_WARN("[Network][STUN] DNS resolution failed for '{}'", server.Host);
			return result;
		}

		CH_CORE_TRACE("[Network][STUN] Resolved '{}' → family={}", server.Host, res->ai_family);

		// Create UDP socket
		int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock < 0
#if CH_PLATFORM_WINDOWS
			|| sock == INVALID_SOCKET
#endif
		)
		{
#if CH_PLATFORM_WINDOWS
			int err = WSAGetLastError();
#else
			int err = errno;
#endif
			freeaddrinfo(res);
			result.Error = "Socket creation failed (err=" + std::to_string(err) + ")";
			CH_CORE_WARN("[Network][STUN] Socket creation failed for '{}' — err={}", server.Host, err);
			return result;
		}

		// Set destination port on the resolved address
		sockaddr_storage dest = {};
		socklen_t destLen = 0;
		if (res->ai_family == AF_INET)
		{
			auto* sin = reinterpret_cast<sockaddr_in*>(&dest);
			memcpy(sin, res->ai_addr, sizeof(sockaddr_in));
			sin->sin_port = htons(server.Port);
			destLen = sizeof(sockaddr_in);
		}
		else
		{
			auto* sin6 = reinterpret_cast<sockaddr_in6*>(&dest);
			memcpy(sin6, res->ai_addr, sizeof(sockaddr_in6));
			sin6->sin6_port = htons(server.Port);
			destLen = sizeof(sockaddr_in6);
		}
		freeaddrinfo(res);

		// Allow reusing address/port
		int reuseOpt = 1;
#if CH_PLATFORM_WINDOWS
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseOpt, sizeof(reuseOpt));
#else
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void*)&reuseOpt, sizeof(reuseOpt));
#ifdef SO_REUSEPORT
		setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const void*)&reuseOpt, sizeof(reuseOpt));
#endif
#endif

		bool boundRequested = false;
		if (localPort > 0)
		{
			sockaddr_in localAddr = {};
			localAddr.sin_family = AF_INET;
			localAddr.sin_addr.s_addr = INADDR_ANY;
			localAddr.sin_port = htons(localPort);
			if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) == 0)
			{
				boundRequested = true;
				CH_CORE_TRACE("[Network][STUN] Bound socket to local port {}", localPort);
			}
			else
			{
				CH_CORE_TRACE("[Network][STUN] Querying via ephemeral port (local port {} in use)", localPort);
			}
		}

		// Set socket timeout
#if CH_PLATFORM_WINDOWS
		DWORD tv = static_cast<DWORD>(timeoutMs);
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
		struct timeval tv;
		tv.tv_sec = timeoutMs / 1000;
		tv.tv_usec = (timeoutMs % 1000) * 1000;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

		// Build STUN Binding Request
		uint8_t request[20];
		size_t requestLen = sizeof(request);
		uint32_t transactionId = 0;

		if (!BuildBindingRequest(request, &requestLen, &transactionId))
		{
#if CH_PLATFORM_WINDOWS
			closesocket(sock);
#else
			close(sock);
#endif
			result.Error = "Failed to build STUN request";
			return result;
		}

		// Send request
		int sent = sendto(sock, (const char*)request, (int)requestLen, 0, (sockaddr*)&dest, destLen);
		if (sent < 0)
		{
#if CH_PLATFORM_WINDOWS
			int err = WSAGetLastError();
			closesocket(sock);
#else
			int err = errno;
			close(sock);
#endif
			result.Error = "Send failed (err=" + std::to_string(err) + ")";
			CH_CORE_WARN("[Network][STUN] Sendto {}:{} failed — err={}", server.Host, server.Port, err);
			return result;
		}
		CH_CORE_TRACE("[Network][STUN] Sent {} bytes to {}:{}", sent, server.Host, server.Port);

		// Receive response
		uint8_t response[1024];
		sockaddr_storage from = {};
		socklen_t fromLen = sizeof(from);
		int received = recvfrom(sock, (char*)response, sizeof(response), 0, (sockaddr*)&from, &fromLen);

#if CH_PLATFORM_WINDOWS
		closesocket(sock);
#else
		close(sock);
#endif

		if (received < (int)STUN_HEADER_SIZE)
		{
#if CH_PLATFORM_WINDOWS
			int err = WSAGetLastError();
#else
			int err = errno;
#endif
			result.Error =
				"No response from STUN server (recv=" + std::to_string(received) + ", err=" + std::to_string(err) + ")";
			CH_CORE_TRACE("[Network][STUN] No response from {}:{} — received={}, err={}", server.Host, server.Port,
						  received, err);
			return result;
		}
		CH_CORE_TRACE("[Network][STUN] Received {} bytes from {}:{}", received, server.Host, server.Port);

		// Parse response
		std::string publicIP;
		uint16_t publicPort = 0;

		if (ParseBindingResponse(response, received, transactionId, publicIP, publicPort))
		{
			result.Success = true;
			result.BoundToRequestedPort = boundRequested;
			result.PublicIP = publicIP;
			result.PublicPort = publicPort;
			CH_CORE_INFO("[Network][STUN] Public endpoint resolved: {}:{}", publicIP, publicPort);
		}
		else
		{
			result.Error = "Failed to parse STUN response";
			CH_CORE_WARN("[Network][STUN] Parse failed for response from {}:{}", server.Host, server.Port);
		}

		return result;
	}

	void StunClient::QueryPublicEndpoint(uint16_t localPort, Callback callback)
	{
		// Join previous query thread if still running
		if (m_QueryThread.joinable())
		{
			m_QueryThread.join();
		}

		m_QueryThread = std::thread([this, localPort, cb = std::move(callback)]() {
			StunResult result = QueryPublicEndpointSync(localPort);
			cb(result);
		});
	}

	StunClient::StunResult StunClient::QueryPublicEndpointSync(uint16_t localPort, int timeoutMs)
	{
		if (m_Servers.empty())
		{
			// Default STUN servers
			m_Servers.push_back({"stun.l.google.com", 19302});
			m_Servers.push_back({"stun1.l.google.com", 19302});
			m_Servers.push_back({"stun2.l.google.com", 19302});
		}

		for (size_t i = 0; i < m_Servers.size(); ++i)
		{
			const auto& server = m_Servers[i];
			CH_CORE_INFO("STUN: Trying server {}/{} — {}:{}", i + 1, m_Servers.size(), server.Host, server.Port);
			StunResult result = QuerySingleServer(server, localPort, timeoutMs);
			if (result.Success)
			{
				m_LastPublicIP = result.PublicIP;
				m_LastPublicPort = result.PublicPort;
				m_HasResult = true;
				return result;
			}
			CH_CORE_WARN("STUN: Server {}:{} failed — {}", server.Host, server.Port, result.Error);
		}

		StunResult fail;
		fail.Error = "All STUN servers failed";
		return fail;
	}

} // namespace Chained
