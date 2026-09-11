#ifndef CH_STUN_CLIENT_H
#define CH_STUN_CLIENT_H

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <thread>

namespace Chained
{

	class StunClient
	{
	public:
		StunClient() = default;
		~StunClient();

		StunClient(const StunClient&) = delete;
		StunClient& operator=(const StunClient&) = delete;

		struct StunResult
		{
			bool Success = false;
			bool BoundToRequestedPort = false;
			std::string PublicIP;
			uint16_t PublicPort = 0;
			std::string Error;
		};

		using Callback = std::function<void(const StunResult& result)>;

		void AddServer(const std::string& host, uint16_t port = 19302);

		void QueryPublicEndpoint(uint16_t localPort, Callback callback);
		StunResult QueryPublicEndpointSync(uint16_t localPort, int timeoutMs = 3000);

		void SetTimeoutMs(int ms)
		{
			m_TimeoutMs = ms;
		}

		void Shutdown();

		const std::string& GetLastPublicIP() const
		{
			return m_LastPublicIP;
		}
		uint16_t GetLastPublicPort() const
		{
			return m_LastPublicPort;
		}
		bool HasResult() const
		{
			return m_HasResult;
		}

	private:
		struct StunServer
		{
			std::string Host;
			uint16_t Port = 19302;
		};

		StunResult QuerySingleServer(const StunServer& server, uint16_t localPort, int timeoutMs);
		bool BuildBindingRequest(uint8_t* out, size_t* outLen, uint32_t* outTransactionId);
		bool ParseBindingResponse(const uint8_t* data, size_t len, uint32_t expectedTransactionId, std::string& outIP,
								  uint16_t& outPort);

		std::vector<StunServer> m_Servers;
		int m_TimeoutMs = 3000;
		std::string m_LastPublicIP;
		uint16_t m_LastPublicPort = 0;
		bool m_HasResult = false;
		std::thread m_QueryThread;
	};

} // namespace Chained

#endif // CH_STUN_CLIENT_H
