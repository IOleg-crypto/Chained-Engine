#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/audio_asset.h"
#include "engine/core/service_locator.h"

#include "engine/project/project.h"

namespace Chained
{

	Audio::Audio()
		: m_engine(nullptr)
	{
	}

	Audio::~Audio()
	{
		if (m_engine)
		{
			CH_CORE_WARN("Audio System: Destructor called before explicit Shutdown(). Force shutting down.");
			Shutdown();
		}
	}

	void Audio::Initialize()
	{
		if (m_engine)
		{
			CH_CORE_WARN("Audio System: Already initialized.");
			return;
		}

		m_engine = std::unique_ptr<ma_engine, MiniaudioEngineDeleter>(new ma_engine());
		ma_result result = ma_engine_init(NULL, m_engine.get());
		if (result != MA_SUCCESS)
		{
			CH_CORE_ERROR("Audio System: Failed to initialize miniaudio engine.");
			m_engine.reset();
		}
		else
		{
			ma_device* pDevice = ma_engine_get_device(m_engine.get());
			const char* backendName =
				(pDevice && pDevice->pContext) ? ma_get_backend_name(pDevice->pContext->backend) : "Default";
			CH_CORE_INFO("Audio System: Initialized miniaudio engine successfully via Service (backend: {}).",
						 backendName);
		}
	}

	void Audio::Shutdown()
	{
		if (!m_engine)
		{
			return;
		}

		StopAll();

		m_engine.reset();

		CH_CORE_INFO("Audio System: Shutdown complete.");
	}

	static void UninitInstance(Chained::SoundInstance& instance)
	{
		if (instance.HasSound)
		{
			ma_sound_stop(&instance.Sound);
			ma_sound_uninit(&instance.Sound);
			instance.HasSound = false;
		}
		if (instance.HasDecoder)
		{
			ma_decoder_uninit(&instance.Decoder);
			instance.HasDecoder = false;
		}
	}

	void Audio::Update(Timestep ts)
	{
		if (!m_engine)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (auto it = m_ActiveSounds.begin(); it != m_ActiveSounds.end();)
		{
			auto& inst = **it;
			// HasSound is always true here (sounds that failed init are never added to queue)
			// Remove finished non-looping sounds
			if (inst.HasSound && ma_sound_at_end(&inst.Sound))
			{
				UninitInstance(inst);
				it = m_ActiveSounds.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	AssetHandle Audio::LoadSound(const std::string& filepath)
	{
		if (filepath.empty())
		{
			return AssetHandle(0);
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am)
		{
			CH_CORE_ERROR("Audio System: AssetManager not available");
			return AssetHandle(0);
		}

		auto asset = am->Load<AudioAsset>(filepath);
		if (!asset)
		{
			CH_CORE_ERROR("Audio System: Failed to load audio asset: {}", filepath);
			return AssetHandle(0);
		}

		return asset->GetID();
	}

	bool Audio::IsSoundLoaded(AssetHandle handle) const
	{
		if (handle == AssetHandle(0))
		{
			return false;
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am)
		{
			return false;
		}

		auto asset = am->GetAsset(handle);
		return asset != nullptr;
	}

	bool Audio::IsPlaying(AssetHandle handle) const
	{
		if (handle == AssetHandle(0))
		{
			return false;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (const auto& instance : m_ActiveSounds)
		{
			if (instance && instance->Handle == handle && instance->HasSound)
			{
				if (ma_sound_is_playing(&instance->Sound))
				{
					return true;
				}
			}
		}
		return false;
	}

	void Audio::SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
	{
		if (!m_engine)
		{
			return;
		}

		ma_engine_listener_set_position(m_engine.get(), 0, position.x, position.y, position.z);
		ma_engine_listener_set_direction(m_engine.get(), 0, forward.x, forward.y, forward.z);
		ma_engine_listener_set_world_up(m_engine.get(), 0, up.x, up.y, up.z);
	}

	void Audio::SetInstancePosition(AssetHandle handle, const glm::vec3& pos)
	{
		if (!m_engine || handle == AssetHandle(0))
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (const auto& instance : m_ActiveSounds)
		{
			if (instance && instance->Handle == handle && instance->HasSound)
			{
				ma_sound_set_position(&instance->Sound, pos.x, pos.y, pos.z);
			}
		}
	}

	void Audio::Play(AssetHandle handle, float volume, float pitch, bool loop, bool spatial, const glm::vec3& pos)
	{
		if (!m_engine || handle == AssetHandle(0))
		{
			return;
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am)
		{
			CH_CORE_WARN("Audio System: AssetManager not available");
			return;
		}

		auto asset = am->GetAsset(handle);
		if (!asset)
		{
			CH_CORE_WARN("Audio System: Try to play unknown handle {}", (uint64_t)handle);
			return;
		}

		std::string filepath = asset->GetPath();

		// Resolve to absolute path if relative, so ReadProjectAsset and
		// ma_sound_init_from_file both find the file on disk.
		std::filesystem::path resolvedPath = filepath;
		if (resolvedPath.is_relative())
		{
			if (auto project = Project::GetActive())
			{
				resolvedPath = project->GetAssetDirectory() / filepath;
			}
		}
		std::string resolvedStr = resolvedPath.string();

		auto instance = std::make_unique<SoundInstance>();
		instance->Handle = handle;

		// Use synchronous decode (no ASYNC) so ma_sound_at_end/is_playing are safe immediately
		ma_uint32 flags = MA_SOUND_FLAG_DECODE;
		ma_result result = MA_ERROR;

		// Try reading from pack/memory first (works in both editor and packaged builds)
		instance->SoundData = am->ReadProjectAsset(resolvedPath);
		if (instance->SoundData.empty())
		{
			instance->SoundData = am->ReadAssetData(resolvedStr);
		}

		if (!instance->SoundData.empty())
		{
			ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
			result = ma_decoder_init_memory(instance->SoundData.data(), instance->SoundData.size(), &config,
											&instance->Decoder);
			if (result == MA_SUCCESS)
			{
				instance->HasDecoder = true;
				result =
					ma_sound_init_from_data_source(m_engine.get(), &instance->Decoder, flags, NULL, &instance->Sound);
				if (result == MA_SUCCESS)
				{
					instance->HasSound = true;
				}
			}
		}

		if (!instance->HasSound)
		{
			// Fallback: load directly from disk
			result = ma_sound_init_from_file(m_engine.get(), resolvedStr.c_str(), flags, NULL, NULL, &instance->Sound);
			if (result == MA_SUCCESS)
			{
				instance->HasSound = true;
			}
		}

		if (!instance->HasSound)
		{
			CH_CORE_ERROR("Audio System: Failed to init sound '{}' (result={})", resolvedStr, (int)result);
			// Clean up decoder if it was created but sound init failed
			if (instance->HasDecoder)
			{
				ma_decoder_uninit(&instance->Decoder);
				instance->HasDecoder = false;
			}
			return;
		}

		ma_sound_set_volume(&instance->Sound, volume);
		ma_sound_set_pitch(&instance->Sound, pitch);
		ma_sound_set_looping(&instance->Sound, loop ? MA_TRUE : MA_FALSE);

		if (spatial)
		{
			ma_sound_set_position(&instance->Sound, pos.x, pos.y, pos.z);
			ma_sound_set_spatialization_enabled(&instance->Sound, MA_TRUE);
		}

		result = ma_sound_start(&instance->Sound);
		if (result != MA_SUCCESS)
		{
			UninitInstance(*instance);
			CH_CORE_ERROR("Audio System: Failed to start sound.");
			return;
		}

		std::lock_guard lock(m_DataMutex);
		m_ActiveSounds.push_back(std::move(instance));
	}

	void Audio::SetVolume(AssetHandle handle, float volume)
	{
		if (!m_engine || handle == AssetHandle(0))
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (const auto& instance : m_ActiveSounds)
		{
			if (instance && instance->Handle == handle)
			{
				ma_sound_set_volume(&instance->Sound, volume);
			}
		}
	}

	void Audio::SetPitch(AssetHandle handle, float pitch)
	{
		if (!m_engine || handle == AssetHandle(0))
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (const auto& instance : m_ActiveSounds)
		{
			if (instance && instance->Handle == handle)
			{
				ma_sound_set_pitch(&instance->Sound, pitch);
			}
		}
	}

	void Audio::Stop(const std::string& filepath)
	{
		if (!m_engine || filepath.empty())
		{
			return;
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am)
		{
			return;
		}

		AssetHandle handle = am->ResolveToHandle(filepath);
		if (handle == AssetHandle(0))
		{
			return;
		}

		Stop(handle);
	}

	void Audio::Stop(AssetHandle handle)
	{
		if (!m_engine || handle == AssetHandle(0))
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (auto it = m_ActiveSounds.begin(); it != m_ActiveSounds.end();)
		{
			if ((*it)->Handle == handle)
			{
				UninitInstance(**it);
				it = m_ActiveSounds.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void Audio::StopAll()
	{
		if (!m_engine)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_DataMutex);
		for (auto& instance : m_ActiveSounds)
		{
			UninitInstance(*instance);
		}
		m_ActiveSounds.clear();
	}

	ma_engine* Audio::GetEngine() const
	{
		return m_engine.get();
	}

} // namespace Chained
