#include "engine/audio/audio.h"
#include "engine/assets/loaders/audio_loader.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(AudioTest, EmptyBufferPlaybackIsNoOp)
{
	Audio audio;

	EXPECT_NO_THROW(audio.Play(AssetHandle(0)));
	EXPECT_NO_THROW(audio.Update(Timestep(0.016f)));
	EXPECT_NO_THROW(audio.StopAll());
}

TEST(AudioTest, TinyBufferPlaybackAndStopAllAreSafe)
{
	Audio audio;

	EXPECT_NO_THROW(audio.Play(AssetHandle(0), 0.25f, 1.0f, false, false));
	EXPECT_NO_THROW(audio.Update(Timestep(0.016f)));
	EXPECT_NO_THROW(audio.StopAll());
}

TEST(AudioTest, LoadAudio)
{
	AudioLoader loader;
	auto audio = loader.Create();
	loader.Load(audio, "resources/audio/default.wav");
	EXPECT_EQ(audio->GetType(), AssetType::Audio);
}
TEST(AudioTest, IsPlaying)
{
	Audio audio;

	// Play a sound with an invalid handle
	audio.Play(AssetHandle(0));
	EXPECT_FALSE(audio.IsPlaying(AssetHandle(0)));

	// Play a sound with a valid handle (assuming 1 is a valid handle)
	audio.Play(AssetHandle(1));
	EXPECT_FALSE(audio.IsPlaying(AssetHandle(1))); // Should be false since we didn't actually load a sound
}
