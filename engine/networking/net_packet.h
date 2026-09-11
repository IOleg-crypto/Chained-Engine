#ifndef CH_NET_PACKET_H
#define CH_NET_PACKET_H

#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static_assert(std::endian::native == std::endian::little, "Network packets assume little-endian byte order. "
														  "Add byte-swapping for big-endian platforms.");

#include "network_types.h"

namespace Chained
{

	// Legacy channel constants (deprecated: use ePacketChannel)
	constexpr int kChannel_Reliable = ePacketChannel::SYSTEM;
	constexpr int kChannel_Unreliable = ePacketChannel::SYNC;

	enum MessageType : uint16_t
	{
		MessageType_InputState = 0,
		MessageType_WorldState,
		MessageType_EntitySpawn,
		MessageType_EntityDestroy,
		MessageType_SceneChange,
		MessageType_PlayerAssign,
		MessageType_PlayerInfo,
		MessageType_PlayerList,
		MessageType_ChatMessage,
		MessageType_SceneLoaded,
		MessageType_Heartbeat,
		MessageType_Count
	};

	inline ePacketChannel GetChannelForMessageType(MessageType type)
	{
		switch (type)
		{
		case MessageType_InputState:
		case MessageType_WorldState:
			return ePacketChannel::SYNC;

		case MessageType_ChatMessage:
			return ePacketChannel::EVENT;

		case MessageType_EntitySpawn:
		case MessageType_EntityDestroy:
		case MessageType_SceneChange:
		case MessageType_SceneLoaded:
		case MessageType_PlayerAssign:
		case MessageType_PlayerInfo:
		case MessageType_PlayerList:
		case MessageType_Heartbeat:
		default:
			return ePacketChannel::SYSTEM;
		}
	}

	inline bool IsChannelReliable(ePacketChannel channel)
	{
		return channel != ePacketChannel::SYNC;
	}

	enum InputAction : uint8_t
	{
		InputAction_None = 0,
		InputAction_Jump = 1 << 0,
		InputAction_Sprint = 1 << 1,
		InputAction_Interact = 1 << 2,
	};

	// ── Byte serialization ───────────────────────────────────────────────

	class ByteWriter
	{
	public:
		void WriteU8(uint8_t v)
		{
			m_Buffer.push_back(v);
		}

		void WriteU16(uint16_t v)
		{
			m_Buffer.push_back(static_cast<uint8_t>(v & 0xFF));
			m_Buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
		}

		void WriteU32(uint32_t v)
		{
			for (int i = 0; i < 4; ++i)
			{
				m_Buffer.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
			}
		}

		void WriteU64(uint64_t v)
		{
			for (int i = 0; i < 8; ++i)
			{
				m_Buffer.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
			}
		}

		void WriteFloat(float v)
		{
			uint32_t u;
			std::memcpy(&u, &v, sizeof(u));
			WriteU32(u);
		}

		void WriteString(const char* str, size_t maxLen)
		{
			size_t len = str ? strnlen(str, maxLen) : 0;
			WriteU8(static_cast<uint8_t>(len));
			if (len > 0)
			{
				m_Buffer.insert(m_Buffer.end(), str, str + len);
			}
		}

		void WriteBytes(const uint8_t* data, size_t len)
		{
			m_Buffer.insert(m_Buffer.end(), data, data + len);
		}

		const std::vector<uint8_t>& Data() const
		{
			return m_Buffer;
		}
		void Clear()
		{
			m_Buffer.clear();
		}

	private:
		std::vector<uint8_t> m_Buffer;
	};

	class ByteReader
	{
	public:
		ByteReader(const uint8_t* data, size_t size)
			: m_Data(data),
			  m_Size(size)
		{
		}

		bool ReadU8(uint8_t& v)
		{
			if (m_Pos >= m_Size)
			{
				return false;
			}
			v = m_Data[m_Pos++];
			return true;
		}

		bool ReadU16(uint16_t& v)
		{
			if (m_Pos + 2 > m_Size)
			{
				return false;
			}
			v = static_cast<uint16_t>(m_Data[m_Pos] | (m_Data[m_Pos + 1] << 8));
			m_Pos += 2;
			return true;
		}

		bool ReadU32(uint32_t& v)
		{
			if (m_Pos + 4 > m_Size)
			{
				return false;
			}
			v = 0;
			for (int i = 0; i < 4; ++i)
			{
				v |= static_cast<uint32_t>(m_Data[m_Pos + i]) << (i * 8);
			}
			m_Pos += 4;
			return true;
		}

		bool ReadU64(uint64_t& v)
		{
			if (m_Pos + 8 > m_Size)
			{
				return false;
			}
			v = 0;
			for (int i = 0; i < 8; ++i)
			{
				v |= static_cast<uint64_t>(m_Data[m_Pos + i]) << (i * 8);
			}
			m_Pos += 8;
			return true;
		}

		bool ReadFloat(float& v)
		{
			uint32_t u;
			if (!ReadU32(u))
			{
				return false;
			}
			std::memcpy(&v, &u, sizeof(v));
			return true;
		}

		bool ReadString(char* str, size_t maxLen)
		{
			uint8_t len = 0;
			if (!ReadU8(len))
			{
				return false;
			}
			if (len >= maxLen)
			{
				return false;
			}
			if (m_Pos + len > m_Size)
			{
				return false;
			}
			std::memcpy(str, m_Data + m_Pos, len);
			str[len] = '\0';
			m_Pos += len;
			return true;
		}

		bool ReadBytes(uint8_t* out, size_t len)
		{
			if (m_Pos + len > m_Size)
			{
				return false;
			}
			std::memcpy(out, m_Data + m_Pos, len);
			m_Pos += len;
			return true;
		}

		bool Eof() const
		{
			return m_Pos >= m_Size;
		}
		size_t Remaining() const
		{
			return m_Size - m_Pos;
		}

	private:
		const uint8_t* m_Data;
		size_t m_Size;
		size_t m_Pos = 0;
	};

	// ── Message structs ──────────────────────────────────────────────────

	struct InputStateMessage
	{
		uint32_t Tick = 0;
		float MoveX = 0.0f;
		float MoveZ = 0.0f;
		uint8_t ActionFlags = 0;
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		float DeltaTime = 0.0f;

		void Encode(ByteWriter& w) const
		{
			w.WriteU32(Tick);
			w.WriteFloat(MoveX);
			w.WriteFloat(MoveZ);
			w.WriteU8(ActionFlags);
			w.WriteFloat(MouseX);
			w.WriteFloat(MouseY);
			w.WriteFloat(DeltaTime);
		}

		bool Decode(ByteReader& r)
		{
			return r.ReadU32(Tick) && r.ReadFloat(MoveX) && r.ReadFloat(MoveZ) && r.ReadU8(ActionFlags) &&
				   r.ReadFloat(MouseX) && r.ReadFloat(MouseY) && r.ReadFloat(DeltaTime);
		}
	};

	struct WorldStateMessage
	{
		uint32_t Tick = 0;
		uint64_t NetworkID = 0;
		float Position[3] = {0, 0, 0};
		float Rotation[4] = {1, 0, 0, 0};
		float Velocity[3] = {0, 0, 0};
		uint8_t IsGrounded = 0;
		uint8_t ActionFlags = 0;

		void Encode(ByteWriter& w) const
		{
			w.WriteU32(Tick);
			w.WriteU64(NetworkID);
			for (int i = 0; i < 3; ++i)
			{
				w.WriteFloat(Position[i]);
			}
			for (int i = 0; i < 4; ++i)
			{
				w.WriteFloat(Rotation[i]);
			}
			for (int i = 0; i < 3; ++i)
			{
				w.WriteFloat(Velocity[i]);
			}
			w.WriteU8(IsGrounded);
			w.WriteU8(ActionFlags);
		}

		bool Decode(ByteReader& r)
		{
			if (!r.ReadU32(Tick) || !r.ReadU64(NetworkID))
			{
				return false;
			}
			for (int i = 0; i < 3; ++i)
			{
				if (!r.ReadFloat(Position[i]))
				{
					return false;
				}
			}
			for (int i = 0; i < 4; ++i)
			{
				if (!r.ReadFloat(Rotation[i]))
				{
					return false;
				}
			}
			for (int i = 0; i < 3; ++i)
			{
				if (!r.ReadFloat(Velocity[i]))
				{
					return false;
				}
			}
			r.ReadU8(IsGrounded);
			r.ReadU8(ActionFlags);
			return true;
		}
	};

	struct EntitySpawnMessage
	{
		uint64_t NetworkID = 0;
		char PrefabPath[128] = {};

		void Encode(ByteWriter& w) const
		{
			w.WriteU64(NetworkID);
			w.WriteString(PrefabPath, sizeof(PrefabPath));
		}

		bool Decode(ByteReader& r)
		{
			return r.ReadU64(NetworkID) && r.ReadString(PrefabPath, sizeof(PrefabPath));
		}
	};

	struct EntityDestroyMessage
	{
		uint64_t NetworkID = 0;

		void Encode(ByteWriter& w) const
		{
			w.WriteU64(NetworkID);
		}
		bool Decode(ByteReader& r)
		{
			return r.ReadU64(NetworkID);
		}
	};

	struct SceneChangeMessage
	{
		char ScenePath[256] = {};

		void Encode(ByteWriter& w) const
		{
			w.WriteString(ScenePath, sizeof(ScenePath));
		}
		bool Decode(ByteReader& r)
		{
			return r.ReadString(ScenePath, sizeof(ScenePath));
		}
	};

	struct PlayerAssignMessage
	{
		uint64_t NetworkID = 0;

		void Encode(ByteWriter& w) const
		{
			w.WriteU64(NetworkID);
		}
		bool Decode(ByteReader& r)
		{
			return r.ReadU64(NetworkID);
		}
	};

	struct PlayerInfoMessage
	{
		char Name[32] = {};
		uint8_t SkinIndex = 0;

		void Encode(ByteWriter& w) const
		{
			w.WriteString(Name, sizeof(Name));
			w.WriteU8(SkinIndex);
		}

		bool Decode(ByteReader& r)
		{
			return r.ReadString(Name, sizeof(Name)) && r.ReadU8(SkinIndex);
		}
	};

	struct PlayerListEntryInfo
	{
		uint64_t NetworkID = 0;
		char Name[32] = {};
		uint8_t SkinIndex = 0;
		uint8_t IsHost = 0;
	};

	struct PlayerListMessage
	{
		uint8_t Count = 0;
		PlayerListEntryInfo Entries[64];

		void Encode(ByteWriter& w) const
		{
			w.WriteU8(Count);
			for (int i = 0; i < Count && i < 64; ++i)
			{
				w.WriteU64(Entries[i].NetworkID);
				w.WriteString(Entries[i].Name, sizeof(Entries[i].Name));
				w.WriteU8(Entries[i].SkinIndex);
				w.WriteU8(Entries[i].IsHost);
			}
		}

		bool Decode(ByteReader& r)
		{
			if (!r.ReadU8(Count))
			{
				return false;
			}
			for (int i = 0; i < Count && i < 64; ++i)
			{
				if (!r.ReadU64(Entries[i].NetworkID) || !r.ReadString(Entries[i].Name, sizeof(Entries[i].Name)) ||
					!r.ReadU8(Entries[i].SkinIndex) || !r.ReadU8(Entries[i].IsHost))
				{
					return false;
				}
			}
			return true;
		}
	};

	struct ChatMessageMessage
	{
		uint64_t SenderNetworkID = 0;
		char SenderName[32] = {};
		char Message[256] = {};

		void Encode(ByteWriter& w) const
		{
			w.WriteU64(SenderNetworkID);
			w.WriteString(SenderName, sizeof(SenderName));
			w.WriteString(Message, sizeof(Message));
		}

		bool Decode(ByteReader& r)
		{
			return r.ReadU64(SenderNetworkID) && r.ReadString(SenderName, sizeof(SenderName)) &&
				   r.ReadString(Message, sizeof(Message));
		}
	};

	struct SceneLoadedMessage
	{
		char ScenePath[256] = {0};

		void Encode(ByteWriter& w) const
		{
			w.WriteString(ScenePath, sizeof(ScenePath));
		}
		bool Decode(ByteReader& r)
		{
			return r.ReadString(ScenePath, sizeof(ScenePath));
		}
	};

	// ── Internal engine types ────────────────────────────────────────────

	struct PlayerNetInfo
	{
		uint64_t NetworkID = 0;
		uint32_t Ping = 0;
		std::string Name;
		uint8_t SkinIndex = 0;
		uint8_t IsHost = 0;
	};

} // namespace Chained

#endif /* CH_NET_PACKET_H */
