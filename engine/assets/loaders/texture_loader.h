#ifndef CH_TEXTURE_LOADER_H
#define CH_TEXTURE_LOADER_H

#include "engine/assets/loaders/iasset_loader.h"
#include "engine/assets/types/texture_asset.h"
#include <memory>
#include <string>

namespace Chained
{
	class TextureLoader : public IAssetLoader
	{
	public:
		bool IsAsync() const override
		{
			return true;
		}
		std::shared_ptr<Asset> Create() override;
		bool Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath,
				  std::string* outError = nullptr) override;

	private:
		void EnsureBasisuInit();
		void FlipImageVertically(void* pixels, int width, int height, int channels, size_t bytesPerChannel);
		bool TryTranscodeKTX2(const void* data, size_t dataSize, std::shared_ptr<TextureAsset> texAsset,
							  bool flipY = false);
	};
} // namespace Chained

#endif // CH_TEXTURE_LOADER_H
