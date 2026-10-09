#ifndef CH_STRING_UTILS_H
#define CH_STRING_UTILS_H

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace Chained
{
	/// @brief Returns a lowercased copy of the input string.
	/// Uses the sink (by-value) pattern — pass std::move(str) or an rvalue
	/// to avoid an extra copy when the caller no longer needs the original.
	inline std::string StringToLower(std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return str;
	}

	/// @brief Returns a lowercased copy of a string_view (always allocates).
	inline std::string StringToLower(std::string_view sv)
	{
		return StringToLower(std::string(sv));
	}

	/// @brief Case-insensitive equality check without allocation.
	inline bool StringEqualCI(std::string_view a, std::string_view b)
	{
		if (a.size() != b.size())
		{
			return false;
		}
		for (size_t i = 0; i < a.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
			{
				return false;
			}
		}
		return true;
	}

} // namespace Chained

#endif // CH_STRING_UTILS_H
