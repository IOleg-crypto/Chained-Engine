#include "signaling_client.h"
#include "engine/core/log.h"
#include "engine/common/platform_detection.h"

#if CH_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <cstdio>
#include <array>
#endif

#include <sstream>
#include <cstring>
#include <chrono>

namespace Chained
{
	namespace
	{
		std::string ExtractJsonString(const std::string& json, const std::string& key)
		{
			std::string pattern = "\"" + key + "\":\"";
			size_t start = json.find(pattern);
			if (start == std::string::npos)
			{
				pattern = "\"" + key + "\": \"";
				start = json.find(pattern);
				if (start == std::string::npos)
				{
					return "";
				}
			}
			start += pattern.length();
			size_t end = json.find('\"', start);
			if (end == std::string::npos)
			{
				return "";
			}
			return json.substr(start, end - start);
		}

		int ExtractJsonInt(const std::string& json, const std::string& key)
		{
			std::string pattern = "\"" + key + "\":";
			size_t start = json.find(pattern);
			if (start == std::string::npos)
			{
				pattern = "\"" + key + "\": ";
				start = json.find(pattern);
				if (start == std::string::npos)
				{
					return 0;
				}
			}
			start += pattern.length();
			while (start < json.length() && (json[start] == ' ' || json[start] == '\t'))
			{
				start++;
			}
			size_t end = start;
			while (end < json.length() && (json[end] >= '0' && json[end] <= '9'))
			{
				end++;
			}
			if (end > start)
			{
				try
				{
					return std::stoi(json.substr(start, end - start));
				} catch (...)
				{
				}
			}
			return 0;
		}

		bool ExtractJsonBool(const std::string& json, const std::string& key)
		{
			std::string pattern = "\"" + key + "\":";
			size_t start = json.find(pattern);
			if (start == std::string::npos)
			{
				pattern = "\"" + key + "\": ";
				start = json.find(pattern);
				if (start == std::string::npos)
				{
					return false;
				}
			}
			start += pattern.length();
			while (start < json.length() && (json[start] == ' ' || json[start] == '\t'))
			{
				start++;
			}
			if (json.compare(start, 4, "true") == 0)
			{
				return true;
			}
			return false;
		}

#if CH_PLATFORM_WINDOWS
		std::wstring Utf8ToWide(const std::string& str)
		{
			if (str.empty())
			{
				return {};
			}
			int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
			std::wstring result(len, 0);
			MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), result.data(), len);
			return result;
		}
#endif
	} // namespace

	std::string SignalingClient::HttpRequest(const std::string& method, const std::string& path,
											 const std::string& jsonBody, int timeoutMs)
	{
		std::string urlPath = path;

#if CH_PLATFORM_WINDOWS
		HINTERNET hSession = WinHttpOpen(L"ChainedDecos/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
										 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
		{
			CH_CORE_WARN("[Signaling] WinHttpOpen failed (error={})", GetLastError());
			return "";
		}

		std::wstring wHost = Utf8ToWide(m_FirebaseHost);
		HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!hConnect)
		{
			CH_CORE_WARN("[Signaling] WinHttpConnect failed for '{}'", m_FirebaseHost);
			WinHttpCloseHandle(hSession);
			return "";
		}

		std::wstring wMethod = Utf8ToWide(method);
		std::wstring wPath = Utf8ToWide(urlPath);
		HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), nullptr, WINHTTP_NO_REFERER,
												WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!hRequest)
		{
			CH_CORE_WARN("[Signaling] WinHttpOpenRequest failed for {} {}", method, urlPath);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return "";
		}

		// Set timeout in milliseconds
		DWORD timeout = static_cast<DWORD>(timeoutMs);
		WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);

		std::wstring wHeaders = L"Content-Type: application/json";
		BOOL sent;
		if (!jsonBody.empty())
		{
			sent = WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)wHeaders.size(), (LPVOID)jsonBody.c_str(),
									  (DWORD)jsonBody.size(), (DWORD)jsonBody.size(), 0);
		}
		else
		{
			sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		}

		if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
		{
			CH_CORE_TRACE("[Signaling] HTTP request failed: {} {} (error={})", method, urlPath, GetLastError());
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return "";
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
		return result;
#else
		// POSIX: use curl for HTTPS
		std::string fullUrl = "https://" + m_FirebaseHost + urlPath;

		std::ostringstream cmd;
		cmd << "curl -s --max-time " << (timeoutMs / 1000 + 1) << " -X " << method
			<< " -H \"Content-Type: application/json\"";
		if (!jsonBody.empty())
		{
			// Escape single quotes in JSON body for shell safety
			std::string escaped;
			for (char c : jsonBody)
			{
				if (c == '\'')
				{
					escaped += "'\\''";
				}
				else
				{
					escaped += c;
				}
			}
			cmd << " -d '" << escaped << "'";
		}
		cmd << " \"" << fullUrl << "\" 2>/dev/null";

		std::array<char, 4096> buf;
		std::string result;
		FILE* pipe = popen(cmd.str().c_str(), "r");
		if (!pipe)
		{
			CH_CORE_WARN("[Signaling] curl popen failed for {} {}", method, urlPath);
			return "";
		}
		while (fgets(buf.data(), (int)buf.size(), pipe) != nullptr)
		{
			result += buf.data();
		}
		int status = pclose(pipe);
		if (status != 0)
		{
			CH_CORE_TRACE("[Signaling] curl returned {} for {} {}", status, method, urlPath);
			return "";
		}
		return result;
#endif
	}

	SignalingClient::SignalingResult SignalingClient::CreateRoomSync(const std::string& code, const std::string& hostIP,
																	 uint16_t hostPort, int timeoutMs)
	{
		SignalingResult res;
		res.RoomCode = code;

		// Firebase REST: PUT /rooms/<code>.json
		auto now = std::chrono::system_clock::now();
		auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		std::ostringstream json;
		json << "{\"hostIP\":\"" << hostIP << "\",\"hostPort\":" << hostPort << ",\"active\":true,\"created\":" << epoch
			 << "}";

		std::string path = "/rooms/" + code + ".json";
		std::string resp = HttpRequest("PUT", path, json.str(), timeoutMs);
		if (resp.empty())
		{
			res.Error = "No response from Firebase";
			return res;
		}

		// Firebase returns the written JSON on success
		if (resp.find("hostIP") != std::string::npos)
		{
			res.Success = true;
			res.HostIP = hostIP;
			res.HostPort = hostPort;
			CH_CORE_INFO("[Signaling] Room '{}' created on Firebase", code);
		}
		else
		{
			res.Error = "Failed to create room: " + resp;
		}

		return res;
	}

	SignalingClient::SignalingResult SignalingClient::JoinRoomSync(const std::string& code, const std::string& clientIP,
																   uint16_t clientPort, int timeoutMs)
	{
		SignalingResult res;
		res.RoomCode = code;

		// Step 1: PATCH client info into the room
		std::ostringstream patchJson;
		patchJson << "{\"clientIP\":\"" << clientIP << "\",\"clientPort\":" << clientPort << "}";

		std::string path = "/rooms/" + code + ".json";
		std::string patchResp = HttpRequest("PATCH", path, patchJson.str(), timeoutMs);
		if (patchResp.empty())
		{
			res.Error = "No response from Firebase (PATCH)";
			return res;
		}

		// Step 2: GET the full room to retrieve host IP/port
		std::string getResp = HttpRequest("GET", path, "", timeoutMs);
		if (getResp.empty())
		{
			res.Error = "No response from Firebase (GET)";
			return res;
		}

		res.HostIP = ExtractJsonString(getResp, "hostIP");
		res.HostPort = static_cast<uint16_t>(ExtractJsonInt(getResp, "hostPort"));

		if (!res.HostIP.empty() && res.HostPort != 0)
		{
			res.Success = true;
			CH_CORE_INFO("[Signaling] Joined room '{}' -> Host endpoint: {}:{}", code, res.HostIP, res.HostPort);
		}
		else
		{
			res.Error = "Room not found or invalid: " + getResp;
		}

		return res;
	}

	SignalingClient::SignalingResult SignalingClient::PollRoomSync(const std::string& code, int timeoutMs)
	{
		SignalingResult res;
		res.RoomCode = code;

		std::string path = "/rooms/" + code + ".json";
		std::string resp = HttpRequest("GET", path, "", timeoutMs);
		if (resp.empty())
		{
			res.Error = "No response from Firebase";
			return res;
		}

		// Check if client has joined (clientIP field exists and is non-empty)
		res.ClientIP = ExtractJsonString(resp, "clientIP");
		res.ClientPort = static_cast<uint16_t>(ExtractJsonInt(resp, "clientPort"));

		if (!res.ClientIP.empty())
		{
			res.Success = true;
			res.ClientReady = true;
		}
		else
		{
			// Room exists but no client yet — not an error, just not ready
			res.Success = true;
			res.ClientReady = false;
		}

		return res;
	}

	SignalingClient::SignalingResult SignalingClient::DeleteRoomSync(const std::string& code, int timeoutMs)
	{
		SignalingResult res;
		res.RoomCode = code;

		std::string path = "/rooms/" + code + ".json";
		std::string resp = HttpRequest("DELETE", path, "", timeoutMs);
		if (resp.empty())
		{
			res.Error = "No response from Firebase (DELETE)";
			return res;
		}

		// Firebase returns null on successful delete
		res.Success = true;
		CH_CORE_INFO("[Signaling] Room '{}' deleted from Firebase", code);
		return res;
	}

	SignalingClient::SignalingResult SignalingClient::ClearClientDataSync(const std::string& code, int timeoutMs)
	{
		SignalingResult res;
		res.RoomCode = code;

		// PATCH to null out client fields
		std::string path = "/rooms/" + code + ".json";
		std::string resp = HttpRequest("PATCH", path, "{\"clientIP\":null,\"clientPort\":null}", timeoutMs);
		if (resp.empty())
		{
			res.Error = "No response from Firebase (PATCH clear)";
			return res;
		}

		res.Success = true;
		CH_CORE_INFO("[Signaling] Client data cleared for room '{}'", code);
		return res;
	}

} // namespace Chained
