#ifndef CH_NETWORK_PANEL_H
#define CH_NETWORK_PANEL_H

#include "panel.h"
#include <future>
#include <string>

namespace Chained
{

	class NetworkPanel : public Panel
	{
	public:
		NetworkPanel();
		~NetworkPanel() override;

		void OnImGuiRender(bool readOnly = false) override;

	private:
		void DrawHostTab();
		void DrawConnectTab();
		void DrawPlayersTab();

		// Host settings
		char m_HostPort[16] = "7777";
		int m_MaxClients = 8;

		// Connect settings
		char m_ConnectIP[128] = "127.0.0.1";
		char m_ConnectPort[16] = "7777";

		// Status
		std::string m_StatusMessage;
		bool m_StatusIsError = false;

		// Public IP fetch
		std::string m_PublicIP;
		std::future<std::string> m_IpFuture;
		bool m_FetchingIP = false;
	};

} // namespace Chained

#endif // CH_NETWORK_PANEL_H
