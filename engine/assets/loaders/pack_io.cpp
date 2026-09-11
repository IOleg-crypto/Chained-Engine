#include "engine/assets/loaders/pack_io.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/common/platform_detection.h"

#include <cstring>

namespace Chained
{

	size_t PackIOStream::Read(void* pBuffer, size_t pSize, size_t pCount)
	{
		size_t bytesAvail = m_Data.size() - m_Pos;
		size_t bytesNeeded = pSize * pCount;
		size_t toRead = std::min(bytesAvail, bytesNeeded);
		if (toRead == 0)
		{
			return 0;
		}
		std::memcpy(pBuffer, m_Data.data() + m_Pos, toRead);
		m_Pos += toRead;
		return toRead / pSize;
	}

	size_t PackIOStream::Write(const void*, size_t, size_t)
	{
		return 0;
	}

	aiReturn PackIOStream::Seek(size_t pOffset, aiOrigin pOrigin)
	{
		switch (pOrigin)
		{
		case aiOrigin_SET:
			m_Pos = pOffset;
			break;
		case aiOrigin_CUR:
			m_Pos += pOffset;
			break;
		case aiOrigin_END:
			m_Pos = m_Data.size() + pOffset;
			break;
		}
		return (m_Pos <= m_Data.size()) ? aiReturn_SUCCESS : aiReturn_FAILURE;
	}

	size_t PackIOStream::Tell() const
	{
		return m_Pos;
	}

	size_t PackIOStream::FileSize() const
	{
		return m_Data.size();
	}

	void PackIOStream::Flush()
	{
	}

	PackIOSystem::PackIOSystem(const std::string& baseDir, bool packed)
		: m_BaseDir(baseDir),
		  m_Packed(packed)
	{
		if (!m_BaseDir.empty() && m_BaseDir.back() != '/')
		{
			m_BaseDir += '/';
		}
	}

	bool PackIOSystem::ChangeDirectory(const std::string&)
	{
		return true;
	}

	const std::string& PackIOSystem::CurrentDirectory() const
	{
		return m_BaseDir;
	}

	bool PackIOSystem::Exists(const char* pFile) const
	{
		if (!m_Packed)
		{
			return std::filesystem::exists(pFile);
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am || !am->IsPacked())
		{
			return std::filesystem::exists(pFile);
		}

		return am->HasAsset(pFile);
	}

	char PackIOSystem::getOsSeparator() const
	{
#if CH_PLATFORM_WINDOWS
		return '\\';
#else
		return '/';
#endif
	}

	Assimp::IOStream* PackIOSystem::Open(const char* pFile, const char* pMode)
	{
		if (std::string(pMode).find('w') != std::string::npos)
		{
			return nullptr;
		}

		if (!m_Packed)
		{
			auto* fs = new std::ifstream(pFile, std::ios::binary);
			if (!fs->is_open())
			{
				delete fs;
				return nullptr;
			}
			std::vector<char> data((std::istreambuf_iterator<char>(*fs)), std::istreambuf_iterator<char>());
			delete fs;
			return new PackIOStream(std::move(data));
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am || !am->IsPacked())
		{
			return nullptr;
		}

		auto packData = am->ReadAssetData(pFile);
		if (packData.empty())
		{
			return nullptr;
		}

		std::vector<char> charData(packData.begin(), packData.end());
		return new PackIOStream(std::move(charData));
	}

	void PackIOSystem::Close(Assimp::IOStream* pFile)
	{
		delete pFile;
	}

} // namespace Chained
