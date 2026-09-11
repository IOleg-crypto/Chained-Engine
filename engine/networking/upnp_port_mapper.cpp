#include "upnp_port_mapper.h"

#include <miniupnpc.h>
#include <upnpcommands.h>
#include <igd_desc_parse.h>

#include <cstring>
#include <string.h>
#include <cstdlib>

namespace Chained
{

	UpnpPortMapper::~UpnpPortMapper()
	{
		Shutdown();
	}

	void UpnpPortMapper::CleanupDiscovery()
	{
		if (m_URLs)
		{
			FreeUPNPUrls(static_cast<UPNPUrls*>(m_URLs));
			delete static_cast<UPNPUrls*>(m_URLs);
			m_URLs = nullptr;
		}
		if (m_DevList)
		{
			freeUPNPDevlist(static_cast<UPNPDev*>(m_DevList));
			m_DevList = nullptr;
		}
		if (m_IgdData)
		{
			delete static_cast<IGDdatas*>(m_IgdData);
			m_IgdData = nullptr;
		}
		if (m_ControlURL)
		{
			free(m_ControlURL);
			m_ControlURL = nullptr;
		}
		if (m_ServiceType)
		{
			free(m_ServiceType);
			m_ServiceType = nullptr;
		}
		m_Available = false;
		m_PortMapped = false;
		m_PublicIPFetched = false;
		m_CachedPublicIP[0] = '\0';
	}

	bool UpnpPortMapper::Initialize()
	{
		CleanupDiscovery();

		int error = 0;
		m_DevList = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);

		if (error != 0 || !m_DevList)
		{
			CH_CORE_WARN("UPnP: Discovery failed (error={}). UPnP not available.", error);
			CleanupDiscovery();
			return false;
		}

		m_IgdData = new IGDdatas{};
		m_URLs = new UPNPUrls{};
		m_ControlURL = static_cast<void*>(malloc(256));
		m_ServiceType = static_cast<void*>(malloc(256));
		static_cast<char*>(m_ControlURL)[0] = '\0';
		static_cast<char*>(m_ServiceType)[0] = '\0';

		char wanAddr[64] = {};

		int status = UPNP_GetValidIGD(static_cast<UPNPDev*>(m_DevList), static_cast<UPNPUrls*>(m_URLs),
									  static_cast<IGDdatas*>(m_IgdData), m_LanAddress, sizeof(m_LanAddress), wanAddr,
									  sizeof(wanAddr));

		if (status < 1)
		{
			CH_CORE_WARN("UPnP: No valid IGD found (status={}). Port forwarding unavailable.", status);
			CleanupDiscovery();
			return false;
		}

		// Copy control URL from the discovered IGD
		auto* urls = static_cast<UPNPUrls*>(m_URLs);
		auto* data = static_cast<IGDdatas*>(m_IgdData);

		if (urls->controlURL && urls->controlURL[0])
		{
			free(m_ControlURL);
			m_ControlURL = strdup(urls->controlURL);
		}

		// Use the WAN IP connection service type
		if (data->first.servicetype[0])
		{
			free(m_ServiceType);
			m_ServiceType = strdup(data->first.servicetype);
		}

		if (!m_ControlURL || !static_cast<char*>(m_ControlURL)[0])
		{
			CH_CORE_WARN("UPnP: Control URL is empty. Port forwarding unavailable.");
			CleanupDiscovery();
			return false;
		}

		if (wanAddr[0] != '\0')
		{
			std::strncpy(m_CachedPublicIP, wanAddr, sizeof(m_CachedPublicIP) - 1);
			m_PublicIPFetched = true;
		}

		m_Available = true;
		CH_CORE_INFO("UPnP: IGD found. ControlURL='{}', LAN='{}', WAN='{}'", static_cast<char*>(m_ControlURL),
					 m_LanAddress, wanAddr);
		return true;
	}

	void UpnpPortMapper::Shutdown()
	{
		CleanupDiscovery();
	}

	bool UpnpPortMapper::AddMapping(uint16_t port, const char* protocol, const char* description)
	{
		if (!m_Available || !m_ControlURL || !m_ServiceType)
		{
			m_PortMapped = false;
			return false;
		}

		char extPort[8] = {};
		char intPort[8] = {};
		std::snprintf(extPort, sizeof(extPort), "%u", port);
		std::snprintf(intPort, sizeof(intPort), "%u", port);

		char intClient[64] = {};
		std::strncpy(intClient, m_LanAddress, sizeof(intClient) - 1);

		// First, attempt to delete any existing stale mapping on this port to prevent Error 718
		// (ConflictInMappingEntry)
		UPNP_DeletePortMapping(static_cast<char*>(m_ControlURL), static_cast<char*>(m_ServiceType), extPort, protocol,
							   nullptr);

		int result = UPNP_AddPortMapping(static_cast<char*>(m_ControlURL), static_cast<char*>(m_ServiceType), extPort,
										 intPort, intClient, description, protocol, nullptr, "0");

		if (result != UPNPCOMMAND_SUCCESS)
		{
			// Error 718 = ConflictInMappingEntry — port already mapped by another client
			// (e.g. manual port forwarding rule). Check if the existing mapping is correct.
			if (result == 718)
			{
				char existingClient[16] = {};
				char existingPort[6] = {};
				char existingDesc[80] = {};
				char existingEnabled[4] = {};
				char existingLease[16] = {};

				int getRes = UPNP_GetSpecificPortMappingEntry(
					static_cast<char*>(m_ControlURL), static_cast<char*>(m_ServiceType), extPort, protocol, nullptr,
					existingClient, existingPort, existingDesc, existingEnabled, existingLease);

				if (getRes == UPNPCOMMAND_SUCCESS && existingClient[0] != '\0')
				{
					// A mapping exists — check if it points to our LAN IP
					if (std::strcmp(existingClient, m_LanAddress) == 0)
					{
						m_PortMapped = true;
						CH_CORE_INFO("UPnP: Port {} ({}) already mapped to {} by manual rule — treating as OK", port,
									 protocol, existingClient);
						return true;
					}
					CH_CORE_WARN("UPnP: Port {} ({}) mapped to different client {} (expected {}). Conflict.", port,
								 protocol, existingClient, m_LanAddress);
				}
			}

			CH_CORE_WARN("UPnP: Failed to add port mapping {}:{} ({}) — error code: {}", intClient, port, protocol,
						 result);
			m_PortMapped = false;
			return false;
		}

		m_PortMapped = true;
		CH_CORE_INFO("UPnP: Port mapping added — {}:{} ({}) [Service: {}]", intClient, port, protocol, description);
		return true;
	}

	bool UpnpPortMapper::RemoveMapping(uint16_t port, const char* protocol)
	{
		m_PortMapped = false;
		if (!m_Available || !m_ControlURL || !m_ServiceType)
		{
			return false;
		}

		char portStr[8] = {};
		std::snprintf(portStr, sizeof(portStr), "%u", port);

		int result = UPNP_DeletePortMapping(static_cast<char*>(m_ControlURL), static_cast<char*>(m_ServiceType),
											portStr, protocol, nullptr);

		if (result != UPNPCOMMAND_SUCCESS)
		{
			CH_CORE_WARN("UPnP: Failed to remove port mapping {}:{}", port, protocol);
			return false;
		}

		CH_CORE_INFO("UPnP: Port mapping removed — {}:{}", port, protocol);
		return true;
	}

	std::string UpnpPortMapper::GetPublicIP()
	{
		if (!m_Available)
		{
			return {};
		}

		if (m_PublicIPFetched)
		{
			return std::string(m_CachedPublicIP);
		}

		if (!m_ControlURL || !m_ServiceType)
		{
			return {};
		}

		char publicIP[64] = {};
		int result =
			UPNP_GetExternalIPAddress(static_cast<char*>(m_ControlURL), static_cast<char*>(m_ServiceType), publicIP);

		m_PublicIPFetched = true;
		if (result == UPNPCOMMAND_SUCCESS && publicIP[0] != '\0')
		{
			std::strncpy(m_CachedPublicIP, publicIP, sizeof(m_CachedPublicIP) - 1);
			return std::string(publicIP);
		}

		CH_CORE_WARN("UPnP: Failed to get external IP address");
		return {};
	}

} // namespace Chained
