#ifndef CH_SCRIPT_GLUE_INTERNAL_H
#define CH_SCRIPT_GLUE_INTERNAL_H

#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "scriptengine.h"
#include "scriptengine_services.h"
#include <Coral/StringHelper.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#define CH_SCRIPT_FUNC extern "C"

namespace Chained
{

	// Cross-platform string conversion wrappers for C# <-> C++ interop.
	// C# ALWAYS passes and expects 2-byte UTF-16 characters (char* / Marshal.PtrToStringUni) across all OS platforms.
	inline std::string ToUtf8FromU16(const char16_t* str)
	{
		if (!str)
		{
			return {};
		}
		std::string result;
		while (*str)
		{
			uint16_t u = static_cast<uint16_t>(*str++);
			if (u < 0x80)
			{
				result.push_back(static_cast<char>(u));
			}
			else if (u < 0x800)
			{
				result.push_back(static_cast<char>(0xC0 | (u >> 6)));
				result.push_back(static_cast<char>(0x80 | (u & 0x3F)));
			}
			else
			{
				result.push_back(static_cast<char>(0xE0 | (u >> 12)));
				result.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
				result.push_back(static_cast<char>(0x80 | (u & 0x3F)));
			}
		}
		return result;
	}

	inline std::u16string ToUtf16FromUtf8(const std::string& str)
	{
		std::u16string result;
		size_t i = 0;
		while (i < str.size())
		{
			unsigned char c = static_cast<unsigned char>(str[i]);
			if (c < 0x80)
			{
				result.push_back(static_cast<char16_t>(c));
				i += 1;
			}
			else if ((c & 0xE0) == 0xC0 && i + 1 < str.size())
			{
				char16_t u = ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[i + 1]) & 0x3F);
				result.push_back(u);
				i += 2;
			}
			else if ((c & 0xF0) == 0xE0 && i + 2 < str.size())
			{
				char16_t u = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(str[i + 2]) & 0x3F);
				result.push_back(u);
				i += 3;
			}
			else
			{
				result.push_back(static_cast<char16_t>(c));
				i += 1;
			}
		}
		return result;
	}

	inline std::string ToUtf8(const char16_t* str)
	{
		return ToUtf8FromU16(str);
	}

	inline std::string ToUtf8(const Coral::UCChar* str)
	{
		if (!str)
		{
			return {};
		}
		return ToUtf8FromU16(reinterpret_cast<const char16_t*>(str));
	}

	inline Coral::UCString ToWide(const std::string& str)
	{
		return Coral::StringHelper::ConvertUtf8ToWide(str);
	}

	// Backward-compat aliases so existing .cpp files compile without changes.
	static inline std::string ch_u16_to_string(const Coral::UCChar* ptr)
	{
		return ToUtf8(ptr);
	}

	static inline std::string ch_u16_to_string(const char16_t* ptr)
	{
		return ToUtf8FromU16(ptr);
	}

	static inline std::u16string ch_utf8_to_u16(const std::string& str)
	{
		return ToUtf16FromUtf8(str);
	}

	// Unified string-return abstraction for glue functions.
	// Replaces per-file thread_local buffers with a single rotating pool.
	// Each thread gets 4 slots; ReturnString cycles through them so multiple
	// return values can coexist within one interop call chain.
	class GlueStringPool
	{
	public:
		static constexpr int kSlots = 4;

		// Store a string and return a stable C pointer valid until the next
		// ReturnString call that wraps around to the same slot.
		static const Coral::UCChar* ReturnString(const std::string& str)
		{
			thread_local std::u16string s_Buffers[kSlots];
			thread_local int s_Index = 0;

			s_Buffers[s_Index] = ToUtf16FromUtf8(str);
			const Coral::UCChar* result = reinterpret_cast<const Coral::UCChar*>(s_Buffers[s_Index].c_str());
			s_Index = (s_Index + 1) % kSlots;
			return result;
		}

		// Overload for wide strings (pass-through).
		static const Coral::UCChar* ReturnString(const Coral::UCChar* str)
		{
			return str;
		}

		static const Coral::UCChar* ReturnString(const char16_t* str)
		{
			return reinterpret_cast<const Coral::UCChar*>(str);
		}
	};

	Scene* GetActiveScene();

	Entity GetEntity(uint64_t entityID);
} // namespace Chained

#endif // CH_SCRIPT_GLUE_INTERNAL_H