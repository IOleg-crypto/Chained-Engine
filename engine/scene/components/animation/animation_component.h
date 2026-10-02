#ifndef CH_ANIMATION_COMPONENT_H
#define CH_ANIMATION_COMPONENT_H

#include "engine/reflection/reflection.h"
#include "engine/reflection/reflection_rfl.h"
#include "engine/assets/asset.h"
#include <string>
#include <unordered_map>

namespace Chained
{
	struct AnimationComponent
	{
		// --- Config (author-time, serialized) ---
		std::string GraphPath;
		uint64_t GraphUUID = 0;
		float BlendDuration = 0.25f;
		bool DefaultIsLooping = true;
		bool PlayOnStart = true;

		// --- Runtime (transient) ---
		AssetHandle GraphAssetHandle = 0;
		int CurrentNodeID = -1;
		int TargetNodeID = -1; // Target node during blend
		int CurrentAnimationIndex = 0;
		int TargetAnimationIndex = -1;
		int StartFrame = 0;
		int EndFrame = -1;
		float Speed = 1.0f;
		bool IsLooping = true;

		// Target node params (captured at transition trigger)
		int TargetStartFrame = 0;
		int TargetEndFrame = -1;
		float TargetSpeed = 1.0f;
		bool TargetIsLooping = true;

		float FrameTimeCounter = 0.0f;
		float TargetFrameTimeCounter = 0.0f; // For target animation during blend
		float BlendTimer = 0.0f;
		int CurrentFrame = 0;
		int TargetFrame = 0;
		bool IsPlaying = true;
		bool Blending = false;
		float Duration = 0.0f;
		float CurrentTime = 0.0f;
		float NormalizedTime = 0.0f;
		bool IsFinished = false;

		// --- Graph Variables ---
		std::unordered_map<std::string, float> Variables;

		// --- API ---
		void SetFloat(const std::string& name, float value)
		{
			Variables[name] = value;
		}

		void SetBool(const std::string& name, bool value)
		{
			Variables[name] = value ? 1.0f : 0.0f;
		}

		float GetFloat(const std::string& name) const
		{
			auto it = Variables.find(name);
			return (it != Variables.end()) ? it->second : 0.0f;
		}

		bool IsGraphDriven() const
		{
			return !GraphPath.empty();
		}

		static const char* GetStaticName()
		{
			return "AnimationComponent";
		}

		struct UI
		{
			UIMeta GraphPath = {.Hint = PropertyMeta::WidgetHint::FilePicker,
								.Tooltip = "Path to the animation graph file (.chag)",
								.Extensions = ".chag"};
			UIMeta GraphUUID = {.ReadOnly = true};
			UIMeta BlendDuration = {
				.Min = 0.0f, .Max = 2.0f, .Speed = 0.05f, .Tooltip = "Time to blend between animations (in seconds)"};
			UIMeta DefaultIsLooping = {.Tooltip = "Default loop setting (used when not graph-driven)"};
			UIMeta PlayOnStart = {.Tooltip = "Whether the animation will play at the start of the scene"};

			UIMeta GraphAssetHandle = {.ReadOnly = true, .Transient = true};
			UIMeta CurrentNodeID = {.ReadOnly = true, .Transient = true};
			UIMeta TargetNodeID = {.ReadOnly = true, .Transient = true};
			UIMeta Variables = {.ReadOnly = false};
			UIMeta CurrentAnimationIndex = {
				.Hint = PropertyMeta::WidgetHint::Enum, .ReadOnly = false, .Transient = true};
			UIMeta TargetAnimationIndex = {.ReadOnly = true, .Transient = true};
			UIMeta StartFrame = {.ReadOnly = false, .Transient = true};
			UIMeta EndFrame = {.ReadOnly = false, .Transient = true};
			UIMeta Speed = {.ReadOnly = false, .Transient = true};
			UIMeta IsLooping = {.Hint = PropertyMeta::WidgetHint::Checkbox, .ReadOnly = false, .Transient = true};
			UIMeta TargetStartFrame = {.ReadOnly = true, .Transient = true};
			UIMeta TargetEndFrame = {.ReadOnly = true, .Transient = true};
			UIMeta TargetSpeed = {.ReadOnly = true, .Transient = true};
			UIMeta TargetIsLooping = {.ReadOnly = true, .Transient = true};
			UIMeta FrameTimeCounter = {.ReadOnly = true, .Transient = true};
			UIMeta TargetFrameTimeCounter = {.ReadOnly = true, .Transient = true};
			UIMeta BlendTimer = {.ReadOnly = true, .Transient = true};
			UIMeta CurrentFrame = {.ReadOnly = true, .Transient = true};
			UIMeta TargetFrame = {.ReadOnly = true, .Transient = true};
			UIMeta IsPlaying = {.Hint = PropertyMeta::WidgetHint::Checkbox, .ReadOnly = false, .Transient = true};
			UIMeta Blending = {.ReadOnly = true, .Transient = true};
			UIMeta Duration = {.ReadOnly = true, .Transient = true};
			UIMeta CurrentTime = {.ReadOnly = true, .Transient = true};
			UIMeta NormalizedTime = {.ReadOnly = true, .Transient = true};
			UIMeta IsFinished = {.ReadOnly = true, .Transient = true};
		};
	};

	CH_MARK_RFL(AnimationComponent);

} // namespace Chained

#endif // CH_ANIMATION_COMPONENT_H
