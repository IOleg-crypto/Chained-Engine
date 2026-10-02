#include "engine/assets/loaders/model_loader.h"
#include "engine/assets/loaders/assimp_importer.h"
#include "engine/assets/model_data.h"

#include "engine/assets/types/model_asset.h"
#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/asset_metadata.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include "engine/common/zstd_compression.h"

namespace Chained
{
	// Lightweight streambuf that reads from an existing memory buffer without copying.
	struct MemoryStreamBuf : public std::streambuf
	{
		MemoryStreamBuf(const uint8_t* data, size_t size)
		{
			char* begin = const_cast<char*>(reinterpret_cast<const char*>(data));
			setg(begin, begin, begin + size);
		}

		size_t Tell() const
		{
			return static_cast<size_t>(gptr() - eback());
		}

	protected:
		pos_type seekoff(off_type off, std::ios_base::seekdir dir,
						 std::ios_base::openmode which = std::ios_base::in) override
		{
			if (dir == std::ios_base::cur)
			{
				gbump(static_cast<int>(off));
			}
			else if (dir == std::ios_base::end)
			{
				setg(eback(), egptr() + off, egptr());
			}
			else if (dir == std::ios_base::beg)
			{
				setg(eback(), eback() + off, egptr());
			}
			return gptr() - eback();
		}

		pos_type seekpos(pos_type sp, std::ios_base::openmode which = std::ios_base::in) override
		{
			return seekoff(sp - pos_type(0), std::ios_base::beg, which);
		}
	};

	std::shared_ptr<Asset> ModelLoader::Create()
	{
		return std::make_shared<ModelAsset>();
	}

	bool ModelLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{
		auto modelAsset = std::static_pointer_cast<ModelAsset>(asset);

		auto pendingData = LoadMeshDataFromDisk(resolvedPath);
		if (pendingData.isValid)
		{
			modelAsset->SetPendingData(std::move(pendingData));
			return true;
		}
		if (outError)
		{
			*outError = "ModelLoader: failed to import model data from '" + resolvedPath + "'";
		}
		return false;
	}

	PendingModelData ModelLoader::LoadMeshDataFromDisk(const std::filesystem::path& path, int samplingFPS)
	{
		std::filesystem::path chassetPath = path;
		bool isDirectMesh = (path.extension() == ".chmesh" || path.extension() == ".chasset");
		if (!isDirectMesh)
		{
			chassetPath.replace_extension(".chasset");
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();

		std::vector<uint8_t> chassetBytes;
		if (am)
		{
			chassetBytes = am->ReadProjectAsset(chassetPath);
			if (chassetBytes.empty())
			{
				chassetBytes = am->ReadAssetData(chassetPath.generic_string());
			}
			if (chassetBytes.empty())
			{
				chassetBytes = am->ReadAssetData((std::filesystem::path("assets") / chassetPath).generic_string());
			}
			if (chassetBytes.empty())
			{
				std::string pStr = chassetPath.generic_string();
				auto pos = pStr.find("assets/");
				if (pos != std::string::npos)
				{
					chassetBytes = am->ReadAssetData(pStr.substr(pos));
				}
			}

			if (chassetBytes.empty() && am->IsPacked())
			{
				CH_CORE_WARN("ModelLoader: .chasset '{}' not found in pack (all fallback paths exhausted)",
							 chassetPath.string());
			}
		}
		else if (std::filesystem::exists(chassetPath))
		{
			std::ifstream is(chassetPath, std::ios::binary | std::ios::ate);
			if (is.is_open())
			{
				auto sz = is.tellg();
				is.seekg(0);
				chassetBytes.resize(static_cast<size_t>(sz));
				is.read(reinterpret_cast<char*>(chassetBytes.data()), sz);
			}
		}

		if (!chassetBytes.empty())
		{
			try
			{
				// Wrap raw bytes directly — avoid copying 200+ MB into a std::string
				MemoryStreamBuf rawMsBuf(chassetBytes.data(), chassetBytes.size());
				std::istream rawStream(&rawMsBuf);

				ChainedAssetHeader header;
				{
					cereal::BinaryInputArchive archive(rawStream);
					archive(header);
				}

				ChainedAssetHeader currentHeader;

				if (header.magic != currentHeader.magic)
				{
					CH_CORE_WARN("Invalid .chasset/.chmesh format (magic mismatch) for: {}", chassetPath.string());
				}
				else if (header.version != currentHeader.version)
				{
					CH_CORE_WARN("Engine data structure changed! file is outdated for: {}", chassetPath.string());
				}
				else
				{
					bool hashValid = true;
					if (!isDirectMesh && header.sourceHash != 0 && (!am || !am->IsPacked()))
					{
						uint64_t currentHash = ComputeFileHash(path);
						if (currentHash != 0 && header.sourceHash != currentHash)
						{
							CH_CORE_WARN("Source file changed since .chasset was created, re-importing: {}",
										 path.string());
							hashValid = false;
						}
					}

					if (hashValid)
					{
						PendingModelData data;
						if (header.compressed)
						{
							// Point directly into chassetBytes — no extra allocation.
							// Previously a separate compressedData vector was allocated here,
							// meaning chassetBytes + compressedData + decompressed all lived in
							// RAM simultaneously (~3× uncompressed), causing bad_alloc on large
							// models (200MB+).
							const size_t headerSize = rawMsBuf.Tell();
							const size_t compressedSize =
								(header.compressedSize > 0 && headerSize + header.compressedSize <= chassetBytes.size())
									? static_cast<size_t>(header.compressedSize)
									: (chassetBytes.size() > headerSize ? chassetBytes.size() - headerSize : 0);

							auto decompressed = Zstd::Decompress(chassetBytes.data() + headerSize, compressedSize,
																 header.uncompressedSize);

							// Free the raw pack buffer before deserializing so peak RAM
							// is ~1.5× uncompressed instead of ~3×.
							chassetBytes.clear();
							chassetBytes.shrink_to_fit();

							if (decompressed.empty())
							{
								CH_CORE_WARN("Failed to decompress .chasset, falling back to Assimp: {}",
											 chassetPath.string());
							}
							else
							{
								// Wrap decompressed bytes directly — avoid copying into std::string
								MemoryStreamBuf decMsBuf(decompressed.data(), decompressed.size());
								std::istream decStream(&decMsBuf);
								{
									cereal::BinaryInputArchive decompressedArchive(decStream);
									decompressedArchive(data);
								}
								return data;
							}
						}
						else
						{
							cereal::BinaryInputArchive archive(rawStream);
							archive(data);
							return data;
						}
					}
				}
			} catch (const std::bad_alloc& e)
			{
				CH_CORE_ERROR("Out of memory deserializing .chasset ({}), falling back to Assimp: {}",
							  chassetPath.string(), e.what());
			} catch (const std::exception& e)
			{
				CH_CORE_WARN("Failed to deserialize .chasset ({}), falling back to Assimp: {}", chassetPath.string(),
							 e.what());
			}
		}

		CH_CORE_TRACE("ModelLoader: Falling back to Assimp import for '{}'", path.string());
		PendingModelData data = AssimpImporter::Import(path, samplingFPS);

		if (data.isValid && !path.string().starts_with(":"))
		{
			// Skip .chasset cache writes in packed/exported mode (directory may be read-only)
			auto* am = ServiceLocator::TryGet<AssetManager>();
			bool skipCache = am && am->IsPacked();

			if (!skipCache)
			{
				try
				{
					struct VectorStreamBuf : public std::streambuf
					{
						std::vector<char> buffer;
						VectorStreamBuf()
						{
							buffer.reserve(32 * 1024 * 1024);
						}
						int_type overflow(int_type ch) override
						{
							if (ch != traits_type::eof())
							{
								buffer.push_back(static_cast<char>(ch));
							}
							return ch;
						}
						std::streamsize xsputn(const char* s, std::streamsize count) override
						{
							buffer.insert(buffer.end(), s, s + count);
							return count;
						}
					};

					VectorStreamBuf sbuf;
					{
						std::ostream dataStream(&sbuf);
						cereal::BinaryOutputArchive dataArchive(dataStream);
						dataArchive(data);
					}

					uint64_t sourceHash = ComputeFileHash(path);
					auto compressed = Zstd::Compress(sbuf.buffer.data(), sbuf.buffer.size(), 3);

					ChainedAssetHeader header;
					header.sourceHash = sourceHash;
					header.compressed = !compressed.empty();
					header.compressedSize = compressed.size();
					header.uncompressedSize = sbuf.buffer.size();

					std::ofstream os(chassetPath, std::ios::binary);
					cereal::BinaryOutputArchive archive(os);
					archive(header);

					if (header.compressed)
					{
						os.write(reinterpret_cast<const char*>(compressed.data()),
								 static_cast<std::streamsize>(compressed.size()));
					}
					else
					{
						os.write(sbuf.buffer.data(), static_cast<std::streamsize>(sbuf.buffer.size()));
					}

					CH_CORE_INFO("ModelAsset: Saved .chasset '{}' (compressed: {}, ratio: {:.1f}%)",
								 chassetPath.filename().string(), header.compressed ? "yes" : "no",
								 header.compressed ? (100.0 * compressed.size() / sbuf.buffer.size()) : 100.0);
				} catch (const std::bad_alloc& e)
				{
					CH_CORE_ERROR("Out of memory serializing .chasset for {}: {}", chassetPath.string(), e.what());
				} catch (const std::exception& e)
				{
					CH_CORE_WARN("Failed to serialize .chasset for {}: {}", chassetPath.string(), e.what());
				}
			}
		}

		return data;
	}
} // namespace Chained