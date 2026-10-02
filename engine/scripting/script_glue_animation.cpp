#include "script_glue_entity.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"

namespace Chained
{

	// ── AnimationComponent ───────────────────────────────────────────────

	int AnimationComponent_GetCurrentAnimationIndex(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>()
				   ? entity.GetComponent<AnimationComponent>().CurrentAnimationIndex
				   : -1;
	}
	void AnimationComponent_SetCurrentAnimationIndex(uint64_t entityID, int index)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			auto& anim = entity.GetComponent<AnimationComponent>();
			if (anim.CurrentAnimationIndex != index)
			{
				anim.CurrentAnimationIndex = index;
				anim.CurrentFrame = 0;
				anim.FrameTimeCounter = 0.0f;
				anim.IsFinished = false;
			}
		}
	}
	uint32_t AnimationComponent_GetIsPlaying(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>()
				   ? (entity.GetComponent<AnimationComponent>().IsPlaying ? 1 : 0)
				   : 0;
	}
	void AnimationComponent_SetIsPlaying(uint64_t entityID, uint32_t isPlaying)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			entity.GetComponent<AnimationComponent>().IsPlaying = isPlaying != 0;
		}
	}
	uint8_t AnimationComponent_GetIsLooping(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>() ? entity.GetComponent<AnimationComponent>().IsLooping
																   : false;
	}
	void AnimationComponent_SetIsLooping(uint64_t entityID, uint8_t isLooping)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			entity.GetComponent<AnimationComponent>().IsLooping = isLooping;
		}
	}
	uint8_t AnimationComponent_GetIsFinished(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>()
				   ? entity.GetComponent<AnimationComponent>().IsFinished
				   : false;
	}
	float AnimationComponent_GetDuration(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>() ? entity.GetComponent<AnimationComponent>().Duration
																   : 0.0f;
	}
	float AnimationComponent_GetNormalizedTime(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>()
				   ? entity.GetComponent<AnimationComponent>().NormalizedTime
				   : 0.0f;
	}
	float AnimationComponent_GetBlendDuration(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<AnimationComponent>()
				   ? entity.GetComponent<AnimationComponent>().BlendDuration
				   : 0.0f;
	}
	void AnimationComponent_SetBlendDuration(uint64_t entityID, float blendDuration)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			entity.GetComponent<AnimationComponent>().BlendDuration = blendDuration;
		}
	}
	void AnimationComponent_CrossFade(uint64_t entityID, int targetIndex, float blendDuration)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			auto& anim = entity.GetComponent<AnimationComponent>();
			if (anim.CurrentAnimationIndex == targetIndex && !anim.Blending)
			{
				return;
			}
			anim.TargetAnimationIndex = targetIndex;
			anim.TargetFrame = 0;
			anim.BlendDuration = blendDuration;
			anim.BlendTimer = 0.0f;
			anim.Blending = true;
			anim.IsPlaying = true;
			anim.IsFinished = false;
		}
	}

	// ── AnimationComponent graph variables ────────────────────────────────

	void AnimationComponent_SetFloat(uint64_t entityID, const Coral::UCChar* name, float value)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			std::string strName = ch_u16_to_string(name);
			entity.GetComponent<AnimationComponent>().SetFloat(strName, value);
		}
	}

	void AnimationComponent_SetBool(uint64_t entityID, const Coral::UCChar* name, uint8_t value)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			std::string strName = ch_u16_to_string(name);
			entity.GetComponent<AnimationComponent>().SetBool(strName, value);
		}
	}

	float AnimationComponent_GetFloat(uint64_t entityID, const Coral::UCChar* name)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<AnimationComponent>())
		{
			std::string strName = ch_u16_to_string(name);
			return entity.GetComponent<AnimationComponent>().GetFloat(strName);
		}
		return 0.0f;
	}

} // namespace Chained
