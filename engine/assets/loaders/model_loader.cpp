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
#include "zstd.h"
#include <cstring>

namespace Chained
{
	// ---------------------------------------------------------------------------
	// Float16 (half-float) conversion — no extra library dependency.
	// Used by chasset v5 to halve the serialized size of mesh float arrays.
	// ---------------------------------------------------------------------------
	static inline uint16_t FloatToHalf(float v)
	{
		uint32_t bits;
		std::memcpy(&bits, &v, sizeof(bits));
		uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000u);
		int exp = static_cast<int>((bits >> 23) & 0xFFu) - 127 + 15;
		uint32_t mant = bits & 0x7FFFFFu;
		if (exp <= 0)
		{
			return sign; // underflow → ±0
		}
		if (exp >= 31)
		{
			return sign | 0x7C00u; // overflow  → ±inf
		}
		return sign | static_cast<uint16_t>(exp << 10) | static_cast<uint16_t>(mant >> 13);
	}

	static inline float HalfToFloat(uint16_t h)
	{
		uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
		int exp = (h >> 10) & 0x1Fu;
		uint32_t mant = h & 0x3FFu;
		if (exp == 0)
		{
			if (mant == 0)
			{
				float r;
				std::memcpy(&r, &sign, 4);
				return r;
			} // ±0
			while (!(mant & 0x400u))
			{
				mant <<= 1;
				--exp;
			} // normalize denormal
			mant &= ~0x400u;
			++exp;
		}
		else if (exp == 31) // inf / nan
		{
			uint32_t bits = sign | 0x7F800000u | (mant << 13);
			float r;
			std::memcpy(&r, &bits, 4);
			return r;
		}
		uint32_t bits = sign | (static_cast<uint32_t>(exp + 127 - 15) << 23) | (mant << 13);
		float r;
		std::memcpy(&r, &bits, 4);
		return r;
	}

	// Write vector<float> as vector<uint16_t> (half-float) via cereal
	template <class Archive> static void WriteHalfArray(Archive& ar, const std::vector<float>& v)
	{
		std::vector<uint16_t> half(v.size());
		for (size_t i = 0; i < v.size(); ++i)
		{
			half[i] = FloatToHalf(v[i]);
		}
		ar(half);
	}

	// Read vector<uint16_t> from archive and expand back to vector<float>
	template <class Archive> static void ReadHalfArray(Archive& ar, std::vector<float>& v)
	{
		std::vector<uint16_t> half;
		ar(half);
		v.resize(half.size());
		for (size_t i = 0; i < half.size(); ++i)
		{
			v[i] = HalfToFloat(half[i]);
		}
	}

	// ---------------------------------------------------------------------------
	// V5 MeshData serialization — float arrays stored as half-float.
	// Order must match exactly between WriteMeshDataV5 and ReadMeshDataV5.
	// ---------------------------------------------------------------------------
	static void WriteMeshDataV5(cereal::BinaryOutputArchive& ar, const MeshData& m)
	{
		WriteHalfArray(ar, m.vertices);	 // 12B/vtx → 6B/vtx
		WriteHalfArray(ar, m.texcoords); // 8B/vtx  → 4B/vtx
		WriteHalfArray(ar, m.normals);	 // 12B/vtx → 6B/vtx
		WriteHalfArray(ar, m.tangents);	 // 16B/vtx → 8B/vtx
		ar(m.colors);					 // uint8 — unchanged
		ar(m.indices);					 // uint32 — unchanged (need full range)
		ar(m.joints);					 // uint8 — unchanged
		WriteHalfArray(ar, m.weights);	 // 16B/vtx → 8B/vtx
		ar(m.materialIndex, m.MinBounds, m.MaxBounds);
	}

	static void ReadMeshDataV5(cereal::BinaryInputArchive& ar, MeshData& m)
	{
		ReadHalfArray(ar, m.vertices);
		ReadHalfArray(ar, m.texcoords);
		ReadHalfArray(ar, m.normals);
		ReadHalfArray(ar, m.tangents);
		ar(m.colors);
		ar(m.indices);
		ar(m.joints);
		ReadHalfArray(ar, m.weights);
		ar(m.materialIndex, m.MinBounds, m.MaxBounds);
	}

	// Serialize the full PendingModelData in v5 format.
	// Non-mesh data (materials, animations, transforms) keeps float32 precision.
	static void SerializePendingModelDataV5(cereal::BinaryOutputArchive& ar, const PendingModelData& d)
	{
		ar(d.fullPath);
		uint64_t meshCount = d.meshes.size();
		ar(meshCount);
		for (const auto& m : d.meshes)
		{
			WriteMeshDataV5(ar, m);
		}
		ar(d.materials, d.embeddedTextures, d.bones, d.bindPose, d.instances, d.nodeNames, d.nodeCount, d.nodeParents,
		   d.nodeLocalTransforms, d.globalBindPoses, d.offsetMatrices, d.animations, d.isValid);
	}

	static void DeserializePendingModelDataV5(cereal::BinaryInputArchive& ar, PendingModelData& d)
	{
		ar(d.fullPath);
		uint64_t meshCount = 0;
		ar(meshCount);
		d.meshes.resize(static_cast<size_t>(meshCount));
		for (auto& m : d.meshes)
		{
			ReadMeshDataV5(ar, m);
		}
		ar(d.materials, d.embeddedTextures, d.bones, d.bindPose, d.instances, d.nodeNames, d.nodeCount, d.nodeParents,
		   d.nodeLocalTransforms, d.globalBindPoses, d.offsetMatrices, d.animations, d.isValid);
	}

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
									if (header.version >= 5)
									{
										DeserializePendingModelDataV5(decompressedArchive, data);
									}
									else
									{
										decompressedArchive(data); // v4: legacy float32
									}
								}
								return data;
							}
						}
						else
						{
							cereal::BinaryInputArchive archive(rawStream);
							if (header.version >= 5)
							{
								DeserializePendingModelDataV5(archive, data);
							}
							else
							{
								archive(data); // v4: legacy float32
							}
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

					// Extract embedded textures to separate files on disk.
					// GLB models embed JPEG/PNG bytes that are already compressed — storing them
					// inside the chasset makes the entire payload incompressible by ZSTD (e.g.
					// after_the_rain: 155 MB JPEG inside 190 MB chasset → ZSTD can't help).
					// Extracting them allows the chasset to shrink from 190 MB to ~35 MB mesh
					// data, which ZSTD compresses to ~7 MB.
					if (!data.embeddedTextures.empty())
					{
						namespace fs = std::filesystem;
						fs::path texDir = chassetPath.parent_path();
						std::string chassetStem = chassetPath.stem().string();
						size_t extractedBytes = 0;
						std::vector<std::string> extractedKeys;

						for (auto& [key, tex] : data.embeddedTextures)
						{
							// Only extract compressed byte buffers (width==0 means JPEG/PNG/WebP)
							if (tex.width != 0 || tex.data.empty())
							{
								continue;
							}

							// Detect format from magic bytes
							std::string ext = ".bin";
							if (tex.data.size() >= 2 && tex.data[0] == 0xFF && tex.data[1] == 0xD8)
							{
								ext = ".jpg";
							}
							else if (tex.data.size() >= 4 && tex.data[0] == 0x89 && tex.data[1] == 'P')
							{
								ext = ".png";
							}
							else if (tex.data.size() >= 4 && tex.data[0] == 'R' && tex.data[1] == 'I')
							{
								ext = ".webp";
							}

							// Clean key for filename: "*0" -> "0"
							std::string cleanKey = key;
							if (!cleanKey.empty() && cleanKey[0] == '*')
							{
								cleanKey = cleanKey.substr(1);
							}

							std::string texFilename = chassetStem + "_embtex_" + cleanKey + ext;
							fs::path texPath = texDir / texFilename;

							std::ofstream texFile(texPath, std::ios::binary);
							if (texFile.is_open())
							{
								texFile.write(reinterpret_cast<const char*>(tex.data.data()),
											  static_cast<std::streamsize>(tex.data.size()));
								texFile.close();

								// Compute path relative to asset directory root
								std::string matPath = texFilename;
								if (auto* am = ServiceLocator::TryGet<AssetManager>())
								{
									const fs::path& assetRoot = am->GetAssetDirectory();
									if (!assetRoot.empty())
									{
										std::error_code ecRel;
										fs::path rel = fs::relative(texPath, assetRoot, ecRel);
										if (!ecRel && !rel.empty())
										{
											matPath = rel.generic_string();
										}
									}
								}

								// Update MaterialData paths: replace "*N" with relative extracted path
								for (auto& mat : data.materials)
								{
									auto rep = [&](std::string& p) {
										if (p == key)
										{
											p = matPath;
										}
									};
									rep(mat.albedoPath);
									rep(mat.normalPath);
									rep(mat.emissivePath);
									rep(mat.metallicRoughnessPath);
									rep(mat.occlusionPath);
								}

								extractedBytes += tex.data.size();
								extractedKeys.push_back(key);
							}
						}

						for (const auto& k : extractedKeys)
						{
							data.embeddedTextures.erase(k);
						}

						if (!extractedKeys.empty())
						{
							CH_CORE_INFO("ModelAsset: Extracted {} embedded textures ({:.1f} MB) from '{}' to disk",
										 extractedKeys.size(), extractedBytes / (1024.0 * 1024.0),
										 chassetPath.filename().string());
						}
					}

					VectorStreamBuf sbuf;
					{
						std::ostream dataStream(&sbuf);
						cereal::BinaryOutputArchive dataArchive(dataStream);
						// v5: mesh float arrays stored as float16 — ~2× smaller before ZSTD
						SerializePendingModelDataV5(dataArchive, data);
					}

					// Log component size breakdown for debugging pack size
					{
						size_t meshVtxBytes = 0, meshIdxBytes = 0, meshOtherBytes = 0;
						for (const auto& m : data.meshes)
						{
							meshVtxBytes += m.vertices.size() * 2 + m.normals.size() * 2 + m.tangents.size() * 2 +
											m.texcoords.size() * 2 + m.weights.size() * 2; // half-float = 2B each
							meshIdxBytes += m.indices.size() * 4;
							meshOtherBytes += m.colors.size() + m.joints.size();
						}
						size_t embTexBytes = 0;
						for (const auto& [key, tex] : data.embeddedTextures)
						{
							embTexBytes += tex.data.size();
						}
						size_t animBytes = 0;
						for (const auto& anim : data.animations)
						{
							animBytes += anim.framePoses.size() * sizeof(TransformData); // 40B each
						}
						CH_CORE_INFO("  chasset breakdown for '{}': meshVtx={:.1f}MB meshIdx={:.1f}MB "
									 "meshOther={:.1f}MB embTex={:.1f}MB anim={:.1f}MB total={:.1f}MB",
									 chassetPath.filename().string(), meshVtxBytes / (1024.0 * 1024.0),
									 meshIdxBytes / (1024.0 * 1024.0), meshOtherBytes / (1024.0 * 1024.0),
									 embTexBytes / (1024.0 * 1024.0), animBytes / (1024.0 * 1024.0),
									 sbuf.buffer.size() / (1024.0 * 1024.0));
					}

					uint64_t sourceHash = ComputeFileHash(path);
					auto compressed = Zstd::Compress(sbuf.buffer.data(), sbuf.buffer.size(), ZSTD_maxCLevel());

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