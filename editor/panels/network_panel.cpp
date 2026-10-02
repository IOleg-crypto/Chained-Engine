#include "network_panel.h"
#include "engine/networking/network_service.h"
#include "engine/scene/systems/network_system.h"
#include "engine/core/service_locator.h"
#include "imgui.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace Chained
{

	/// Performs a blocking HTTP GET to api.ipify.org and returns the plain-text
	/// public IPv4 address. Called from a background thread via std::async — must not touch
	/// ImGui or any engine state.
	static std::string FetchPublicIPBlocking()
	{
#ifdef _WIN32
		HINTERNET hSession = WinHttpOpen(L"ChainedEditor/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
										 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
		{
			return "(error: WinHttpOpen)";
		}

		HINTERNET hConnect = WinHttpConnect(hSession, L"api.ipify.org", INTERNET_DEFAULT_HTTP_PORT, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return "(error: WinHttpConnect)";
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/?format=text", nullptr, WINHTTP_NO_REFERER,
												WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return "(error: WinHttpOpenRequest)";
		}

		BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return "(error: send/receive)";
		}

		std::string result;
		DWORD bytesAvail = 0;
		while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0)
		{
			std::string chunk(bytesAvail, '\0');
			DWORD bytesRead = 0;
			if (WinHttpReadData(hRequest, chunk.data(), bytesAvail, &bytesRead))
			{
				result.append(chunk.data(), bytesRead);
			}
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return result.empty() ? "(empty response)" : result;
#else
		char cmd[256];
		snprintf(cmd, sizeof(cmd), "curl -s --max-time 5 https://api.ipify.org 2>/dev/null");
		FILE* pipe = popen(cmd, "r");
		if (!pipe)
		{
			return "(install curl)";
		}
		char buf[128] = {};
		fgets(buf, sizeof(buf), pipe);
		pclose(pipe);
		std::string r(buf);
		while (!r.empty() && (r.back() == '\n' || r.back() == '\r'))
		{
			r.pop_back();
		}
		return r.empty() ? "(empty response)" : r;
#endif
	}

	NetworkPanel::NetworkPanel()
	{
		m_Name = "Network";
	}

	NetworkPanel::~NetworkPanel() = default;

	void NetworkPanel::OnImGuiRender(bool /*readOnly*/)
	{
		if (!m_IsOpen)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);

		if (ImGui::Begin(m_Name.c_str(), &m_IsOpen))
		{
			auto* net = ServiceLocator::TryGet<Network>();

			// --- Status bar ---
			if (net && net->IsConnected())
			{
				const char* roleStr = net->IsHost() ? "HOST" : "CLIENT";
				ImGui::Text("Status: %s", roleStr);
				ImGui::SameLine();
				ImGui::TextDisabled("(%zu connected)", net->GetClientCount());
			}
			else
			{
				ImGui::TextDisabled("Status: Offline");
			}

			ImGui::Separator();

			// --- Tab Bar ---
			if (ImGui::BeginTabBar("##NetworkTabs"))
			{
				if (ImGui::BeginTabItem("Host"))
				{
					DrawHostTab();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Connect"))
				{
					DrawConnectTab();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Players"))
				{
					DrawPlayersTab();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			// --- Status message ---
			if (!m_StatusMessage.empty())
			{
				ImGui::Separator();
				if (m_StatusIsError)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_StatusMessage.c_str());
				}
				else
				{
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", m_StatusMessage.c_str());
				}
			}
		}

		ImGui::End();
	}

	void NetworkPanel::DrawHostTab()
	{
		auto* net = ServiceLocator::TryGet<Network>();

		// --- Poll async public IP result ---
		if (m_FetchingIP && m_IpFuture.valid())
		{
			if (m_IpFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
			{
				m_PublicIP = m_IpFuture.get();
				m_FetchingIP = false;
			}
		}

		if (net && net->IsHost())
		{
			ImGui::Text("Server is running.");
			ImGui::Separator();

			ImGui::Text("Port: %u", net->GetPort());
			ImGui::Text("Max clients: %d", m_MaxClients);
			ImGui::Text("Connected: %zu", net->GetClientCount());

			ImGui::Separator();

			// ── Public IPv4 block ───────────────────────────────────────────────
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Public IPv4 (for internet play):");
			ImGui::SameLine();
			if (m_FetchingIP)
			{
				ImGui::TextDisabled("fetching...");
			}
			else if (m_PublicIP.empty())
			{
				ImGui::TextDisabled("(not fetched)");
				ImGui::SameLine();
				if (ImGui::SmallButton("Fetch##ipv4"))
				{
					m_FetchingIP = true;
					m_IpFuture = std::async(std::launch::async, []() { return FetchPublicIPBlocking(); });
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Fetch your public IPv4 address");
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "%s:%u", m_PublicIP.c_str(), net->GetPort());
				ImGui::SameLine();
				if (ImGui::SmallButton("Copy##ipv4"))
				{
					std::string full = m_PublicIP + ":" + std::to_string(net->GetPort());
					ImGui::SetClipboardText(full.c_str());
					m_StatusMessage = "Copied: " + full;
					m_StatusIsError = false;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Copy IPv4 address to clipboard");
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Refresh##ipv4"))
				{
					m_PublicIP.clear();
					m_FetchingIP = true;
					m_IpFuture = std::async(std::launch::async, []() { return FetchPublicIPBlocking(); });
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Re-fetch your public IPv4 address");
				}
			}
			ImGui::TextDisabled("Forward UDP port %u on your router to play over the internet.", net->GetPort());

			ImGui::Separator();

			if (net->IsUpnpAvailable())
			{
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "UPnP: Active — port forwarded automatically");
			}
			else
			{
				ImGui::TextDisabled("UPnP: not available (manual port forwarding needed)");
			}

			if (!net->IsUpnpAvailable())
			{
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "For direct internet play:");
				ImGui::BulletText("Share your Public IP with clients, or forward UDP port %u on your router",
								  net->GetPort());
			}

			ImGui::Separator();

			if (ImGui::Button("Stop Server", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				net->Shutdown();
				m_StatusMessage = "Server stopped.";
				m_StatusIsError = false;
				m_PublicIP.clear();
				m_FetchingIP = false;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Stop the running server");
			}
		}
		else
		{
			ImGui::Text("Start a server to host a game.");
			ImGui::Separator();

			ImGui::SetNextItemWidth(120);
			ImGui::InputText("Port", m_HostPort, sizeof(m_HostPort), ImGuiInputTextFlags_CharsDecimal);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("UDP port to listen on (default: %d)", kDefaultPort);
			}

			ImGui::SetNextItemWidth(120);
			ImGui::InputInt("Max Clients", &m_MaxClients, 1, 10);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Maximum number of connected clients");
			}
			if (m_MaxClients < 1)
			{
				m_MaxClients = 1;
			}
			if (m_MaxClients > 32)
			{
				m_MaxClients = 32;
			}

			ImGui::TextDisabled("Direct IP / UPnP connection supported.");

			ImGui::Separator();

			if (ImGui::Button("Start Server", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				uint16_t port = static_cast<uint16_t>(std::atoi(m_HostPort));
				if (port == 0)
				{
					port = kDefaultPort;
				}

				if (net)
				{
					ServiceLocator::Get<NetworkSystem>()->SetPlayerPrefab("prefab/player.chprefab");
					net->HostGame(port, m_MaxClients);
					// Auto-fetch public IP when server starts
					m_PublicIP.clear();
					m_FetchingIP = true;
					m_IpFuture = std::async(std::launch::async, []() { return FetchPublicIPBlocking(); });

					m_StatusMessage = "Server started on port " + std::string(m_HostPort);
					m_StatusIsError = false;
				}
				else
				{
					m_StatusMessage = "Network service not available!";
					m_StatusIsError = true;
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Start hosting a game server");
			}
		}
	}

	void NetworkPanel::DrawConnectTab()
	{
		auto* net = ServiceLocator::TryGet<Network>();

		if (net && net->IsClient())
		{
			ImGui::Text("Connected to server.");
			ImGui::Separator();

			ImGui::Text("Server: %s:%s", m_ConnectIP, m_ConnectPort);

			ImGui::Separator();

			if (ImGui::Button("Disconnect", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				net->Disconnect();
				m_StatusMessage = "Disconnected.";
				m_StatusIsError = false;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Disconnect from the server");
			}
		}
		else
		{
			ImGui::Text("Connect to an existing server.");
			ImGui::Separator();

			ImGui::SetNextItemWidth(200);
			ImGui::InputText("IP", m_ConnectIP, sizeof(m_ConnectIP));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Server IP address to connect to");
			}

			ImGui::SetNextItemWidth(120);
			ImGui::InputText("Port", m_ConnectPort, sizeof(m_ConnectPort), ImGuiInputTextFlags_CharsDecimal);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Server port to connect to (default: %d)", kDefaultPort);
			}

			ImGui::Separator();

			if (ImGui::Button("Connect", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				uint16_t port = static_cast<uint16_t>(std::atoi(m_ConnectPort));
				if (port == 0)
				{
					port = kDefaultPort;
				}

				if (net)
				{
					ServiceLocator::Get<NetworkSystem>()->SetPlayerPrefab("prefab/player.chprefab");
					net->ConnectTo(m_ConnectIP, port);
					m_StatusMessage =
						"Connecting to " + std::string(m_ConnectIP) + ":" + std::string(m_ConnectPort) + "...";
					m_StatusIsError = false;
				}
				else
				{
					m_StatusMessage = "Network service not available!";
					m_StatusIsError = true;
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Connect to the specified server");
			}
		}
	}

	void NetworkPanel::DrawPlayersTab()
	{
		auto* net = ServiceLocator::TryGet<Network>();

		if (!net || net->GetRole() == Role::Offline)
		{
			ImGui::TextDisabled("Not connected.");
			return;
		}

		ImGui::Text("Connected players:");
		ImGui::Separator();

		// Table header
		ImGui::Columns(3, "##PlayerColumns", true);
		ImGui::SetColumnWidth(0, 60);
		ImGui::SetColumnWidth(1, 180);
		ImGui::SetColumnWidth(2, 100);

		ImGui::Text("ID");
		ImGui::NextColumn();
		ImGui::Text("Role");
		ImGui::NextColumn();
		ImGui::Text("Status");
		ImGui::NextColumn();
		ImGui::Separator();

		// Host entry (self)
		if (net->IsHost())
		{
			ImGui::Text("0");
			ImGui::NextColumn();
			ImGui::Text("Host (You)");
			ImGui::NextColumn();
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Playing");
			ImGui::NextColumn();
		}

		// Client entries
		size_t clientCount = net->GetClientCount();
		for (size_t i = 0; i < clientCount; ++i)
		{
			ImGui::Text("%zu", i + 1);
			ImGui::NextColumn();
			ImGui::Text("Client");
			ImGui::NextColumn();
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Connected");
			ImGui::NextColumn();
		}

		ImGui::Columns(1);

		if (clientCount == 0 && net->IsHost())
		{
			ImGui::TextDisabled("No clients connected yet.");
		}
	}

} // namespace Chained
