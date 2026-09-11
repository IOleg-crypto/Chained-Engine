#ifndef CH_UPNP_PORT_MAPPER_H
#define CH_UPNP_PORT_MAPPER_H

#include <cstdint>
#include <string>

namespace Chained
{

	class UpnpPortMapper
	{
	public:
		UpnpPortMapper() = default;
		~UpnpPortMapper();

		UpnpPortMapper(const UpnpPortMapper&) = delete;
		UpnpPortMapper& operator=(const UpnpPortMapper&) = delete;

		bool Initialize();
		void Shutdown();

		bool AddMapping(uint16_t port, const char* protocol = "UDP", const char* description = "Chained Engine");
		bool RemoveMapping(uint16_t port, const char* protocol = "UDP");
		std::string GetPublicIP();
		const char* GetLanIP() const
		{
			return m_LanAddress;
		}
		bool IsAvailable() const
		{
			return m_Available;
		}
		bool IsPortMapped() const
		{
			return m_PortMapped;
		}

	private:
		void CleanupDiscovery();

		void* m_DevList = nullptr;
		void* m_URLs = nullptr;
		void* m_IgdData = nullptr;
		void* m_ControlURL = nullptr;
		void* m_ServiceType = nullptr;
		bool m_Available = false;
		bool m_PortMapped = false;
		char m_LanAddress[64] = {};
		char m_CachedPublicIP[64] = {};
		bool m_PublicIPFetched = false;
	};

} // namespace Chained

#endif // CH_UPNP_PORT_MAPPER_H
