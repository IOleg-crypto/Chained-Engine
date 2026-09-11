#ifndef CH_SCENE_STATE_H
#define CH_SCENE_STATE_H

#include <cstdint>

namespace Chained
{

	enum class SceneState : uint8_t
	{
		Edit = 0,
		Play = 1,
		Simulate = 2
	};

} // namespace Chained

#endif // CH_SCENE_STATE_H
