
#ifndef CH_ZSTD_COMPRESSION_H
#define CH_ZSTD_COMPRESSION_H

#include <vector>
#include <string>
#include <zstd.h>
#include "engine/core/log.h"

namespace Chained
{
	namespace Zstd
	{
		inline std::vector<unsigned char> Compress(const void* src, size_t srcSize, int compressionLevel = 3)
		{
			size_t const maxBounds = ZSTD_compressBound(srcSize);
			std::vector<unsigned char> compressed(maxBounds);

			size_t const cSize = ZSTD_compress(compressed.data(), maxBounds, src, srcSize, compressionLevel);
			if (ZSTD_isError(cSize))
			{
				CH_CORE_ERROR("Zstd: Compression failed: {}", ZSTD_getErrorName(cSize));
				return {};
			}

			compressed.resize(cSize);
			return compressed;
		}

		inline std::vector<unsigned char> Decompress(const void* src, size_t srcSize, size_t expectedOriginalSize = 0)
		{
			if (expectedOriginalSize == 0)
			{
				unsigned long long const contentSize = ZSTD_getFrameContentSize(src, srcSize);
				if (contentSize != ZSTD_CONTENTSIZE_UNKNOWN && contentSize != ZSTD_CONTENTSIZE_ERROR)
				{
					expectedOriginalSize = static_cast<size_t>(contentSize);
				}
				else
				{
					expectedOriginalSize = srcSize * 4;
				}
			}

			std::vector<unsigned char> decompressed(expectedOriginalSize);

			size_t const dSize = ZSTD_decompress(decompressed.data(), expectedOriginalSize, src, srcSize);
			if (ZSTD_isError(dSize))
			{
				CH_CORE_ERROR("Zstd: Decompression failed: {}", ZSTD_getErrorName(dSize));
				return {};
			}

			decompressed.resize(dSize);
			return decompressed;
		}
	} // namespace Zstd
} // namespace Chained

#endif // CH_ZSTD_COMPRESSION_H
