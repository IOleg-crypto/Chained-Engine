#ifndef CH_STRUCTURES_H
#define CH_STRUCTURES_H

#include "engine/common/base.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Chained
{
	struct CH_API Color
	{
		unsigned char r, g, b, a;

		Color()
			: r(0),
			  g(0),
			  b(0),
			  a(255)
		{
		}
		Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
			: r(r),
			  g(g),
			  b(b),
			  a(a)
		{
			if (r < 0 || g < 0 || b < 0 || a < 0)
			{
				CH_CORE_WARN("Color values should be in the range [0, 255]. Negative values will be clamped to 255.");
				if (r < 0)
				{
					r = 255;
				}
				if (g < 0)
				{
					g = 255;
				}
				if (b < 0)
				{
					b = 255;
				}
				if (a < 0)
				{
					a = 255;
				}
			}
		}

		static Color White()
		{
			return {255, 255, 255, 255};
		}
		static Color Black()
		{
			return {0, 0, 0, 255};
		}
		static Color Red()
		{
			return {255, 0, 0, 255};
		}
		static Color Green()
		{
			return {0, 255, 0, 255};
		}
		static Color Blue()
		{
			return {0, 0, 255, 255};
		}
	};

} // namespace Chained

#endif // CH_STRUCTURES_H
