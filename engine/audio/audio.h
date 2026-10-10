#ifndef CH_AUDIO_H
#define CH_AUDIO_H

#include "engine/assets/asset.h"
#include "engine/common/timestep.h"
#include <glm/glm.hpp>
#include <memory>
#include <miniaudio.h>
#include <mutex>
#include <string>
#include <vector>
#include "engine/core/service.h"

namespace Chained
{

	struct SoundInstance
	{
		ma_sound Sound;
		ma_decoder Decoder;
		std::vector<uint8_t> SoundData;
		bool HasDecoder = false;
		bool HasSound = false; // true only after ma_sound_init_* succeeded
		AssetHandle Handle;
	};

	// Custom deleter for ma_engine
	struct MiniaudioEngineDeleter
	{
		void operator()(ma_engine* engine) const
		{
			if (engine)
			{
				ma_engine_uninit(engine);
				delete engine;
			}
		}
	};

	class Audio : public Service
	{
	public:
		virtual void Initialize() override;
		void Update(Timestep ts);
		virtual void Shutdown() override;

	public:
		Audio();
		virtual ~Audio() override;

	public:
		void SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

	public:
		AssetHandle LoadSound(const std::string& filepath);
		bool IsSoundLoaded(AssetHandle handle) const;
		bool IsPlaying(AssetHandle handle) const;

		void Play(AssetHandle handle, float volume = 1.0f, float pitch = 1.0f, bool loop = false, bool spatial = false,
				  const glm::vec3& pos = {0, 0, 0});

		void SetInstancePosition(AssetHandle handle, const glm::vec3& pos);
		void SetVolume(AssetHandle handle, float volume);
		void SetPitch(AssetHandle handle, float pitch);
		void SetMasterVolume(float volume);
		float GetMasterVolume() const;

		void Stop(const std::string& filepath);
		void Stop(AssetHandle handle);
		void StopAll();

	public:
		ma_engine* GetEngine() const;

	private:
		std::unique_ptr<ma_engine, MiniaudioEngineDeleter> m_engine;

	private:
		mutable std::mutex m_DataMutex;
		std::vector<std::unique_ptr<SoundInstance>> m_ActiveSounds;
	};

} // namespace Chained

#endif // CH_AUDIO_H
