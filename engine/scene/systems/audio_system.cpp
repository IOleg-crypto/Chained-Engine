#include "audio_system.h"
#include "engine/audio/audio.h"
#include "engine/core/profiler.h"
#include "engine/core/service_locator.h"
#include "engine/scene/components/audio/audio_component.h"
#include "engine/scene/components/render/camera_component.h"
#include "engine/scene/components/core/transform_component.h"

namespace Chained::AudioSystem
{

	void Update(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();

		// 1. Sync listener with primary camera
		auto cameraView = reg.view<CameraComponent, TransformComponent>();
		for (auto entity : cameraView)
		{
			auto& camera = cameraView.get<CameraComponent>(entity);
			if (camera.Primary)
			{
				auto& transform = cameraView.get<TransformComponent>(entity);
				glm::vec3 pos = glm::vec3(transform.WorldTransform[3]);
				glm::mat3 rot = glm::mat3(transform.WorldTransform);
				glm::vec3 forward = rot * glm::vec3(0, 0, -1);
				glm::vec3 up = rot * glm::vec3(0, 1, 0);

				if (auto* audioSvc = ServiceLocator::TryGet<Audio>())
				{
					audioSvc->SetListenerPosition(pos, forward, up);
				}
				break;
			}
		}

		// 2. Manage audio components
		auto audioView = reg.view<AudioComponent, TransformComponent>();
		auto* audioSvc = ServiceLocator::TryGet<Audio>();
		if (!audioSvc)
		{
			return;
		}

		for (auto entity : audioView)
		{
			auto& audio = audioView.get<AudioComponent>(entity);

			// Load sound if path is set but handle is invalid OR path has changed
			if (!audio.SoundPath.empty())
			{
				bool pathChanged = audio.SoundPath != audio.LoadedSoundPath;
				if (pathChanged && audio.SoundHandle != AssetHandle(0))
				{
					// Stop the old sound and reset so we reload below
					audioSvc->Stop(audio.SoundHandle);
					audio.SoundHandle = AssetHandle(0);
					audio.SoundUUID = 0;
					audio.IsPlaying = false;
				}

				if (audio.SoundHandle == AssetHandle(0) || !audioSvc->IsSoundLoaded(audio.SoundHandle))
				{
					CH_CORE_INFO("AudioComponent: Loading sound: {}", audio.SoundPath);
					audio.SoundHandle = audioSvc->LoadSound(audio.SoundPath);
					// Sync UUID field and loaded path tracker
					audio.SoundUUID = (uint64_t)audio.SoundHandle;
					audio.LoadedSoundPath = audio.SoundPath;
					// Auto-start playback if PlayOnStart was requested
					if (audio.PlayOnStart && audio.SoundHandle != AssetHandle(0))
					{
						audio.IsPlaying = true;
					}
				}
			}

			if (audio.SoundHandle == AssetHandle(0))
			{
				continue;
			}

			// Sync IsPlaying back when a non-looping sound finishes naturally
			const bool actuallyPlaying = audioSvc->IsPlaying(audio.SoundHandle);
			if (audio.IsPlaying && !audio.Loop && !actuallyPlaying)
			{
				audio.IsPlaying = false;
				continue;
			}

			// IsPlaying=true but sound isn't running — start it
			if (audio.IsPlaying && !actuallyPlaying)
			{
				auto& transform = audioView.get<TransformComponent>(entity);
				glm::vec3 worldPos = glm::vec3(transform.WorldTransform[3]);
				audioSvc->Play(audio.SoundHandle, audio.Volume, audio.Pitch, audio.Loop, audio.Spatialized, worldPos);
			}
			// IsPlaying=false but sound is still running — stop it
			else if (!audio.IsPlaying && actuallyPlaying)
			{
				audioSvc->Stop(audio.SoundHandle);
			}
			// IsPlaying=true and sound is running and spatialized — keep position in sync
			else if (audio.IsPlaying && actuallyPlaying && audio.Spatialized)
			{
				auto& transform = audioView.get<TransformComponent>(entity);
				glm::vec3 worldPos = glm::vec3(transform.WorldTransform[3]);
				audioSvc->SetInstancePosition(audio.SoundHandle, worldPos);
			}
		}
	}

	void OnRuntimeStart(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();

		auto view = reg.view<AudioComponent>();
		for (auto entity : view)
		{
			auto& audio = view.get<AudioComponent>(entity);
			audio.IsPlaying = audio.PlayOnStart;
		}
	}

	void OnRuntimeStop(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();

		if (auto* audioSvc = ServiceLocator::TryGet<Audio>())
		{
			audioSvc->StopAll();
		}

		auto view = reg.view<AudioComponent>();
		for (auto entity : view)
		{
			auto& audio = view.get<AudioComponent>(entity);
			audio.IsPlaying = false;
		}
	}

} // namespace Chained::AudioSystem
