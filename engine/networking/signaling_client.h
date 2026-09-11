#ifndef CH_SIGNALING_CLIENT_H
#define CH_SIGNALING_CLIENT_H

#include <string>
#include <cstdint>

namespace Chained
{
	/// Firebase Realtime Database REST signaling client for room coordination.
	/// Uses WinHTTP (HTTPS) on Windows, curl on POSIX.
	class SignalingClient
	{
	public:
		struct SignalingResult
		{
			bool Success = false;
			std::string Error;
			std::string RoomCode;
			std::string HostIP;
			uint16_t HostPort = 0;
			std::string ClientIP;
			uint16_t ClientPort = 0;
			bool ClientReady = false;
		};

		SignalingClient() = default;
		~SignalingClient() = default;

		void SetFirebaseUrl(const std::string& host)
		{
			m_FirebaseHost = host;
		}

		const std::string& GetFirebaseHost() const
		{
			return m_FirebaseHost;
		}

		SignalingResult CreateRoomSync(const std::string& code, const std::string& hostIP, uint16_t hostPort,
									   int timeoutMs = 5000);

		SignalingResult JoinRoomSync(const std::string& code, const std::string& clientIP, uint16_t clientPort,
									 int timeoutMs = 5000);

		SignalingResult PollRoomSync(const std::string& code, int timeoutMs = 3000);

		SignalingResult DeleteRoomSync(const std::string& code, int timeoutMs = 3000);

		SignalingResult ClearClientDataSync(const std::string& code, int timeoutMs = 3000);

	private:
		std::string HttpRequest(const std::string& method, const std::string& path, const std::string& jsonBody,
								int timeoutMs);

		std::string m_FirebaseHost = "chained-decos-default-rtdb.europe-west1.firebasedatabase.app";
	};
} // namespace Chained

#endif // CH_SIGNALING_CLIENT_H
