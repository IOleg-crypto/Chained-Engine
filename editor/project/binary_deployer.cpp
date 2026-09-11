#include "binary_deployer.h"

#include "engine/core/log.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace fs = std::filesystem;

namespace Chained
{
	namespace
	{
		std::string StringToLower(std::string str)
		{
			std::transform(str.begin(), str.end(), str.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return str;
		}

		bool IsCancelled(const std::atomic<bool>* flag)
		{
			return flag && flag->load(std::memory_order_relaxed);
		}

		bool IsDebugDll(const std::string& fname, const fs::path& exeDir)
		{
			// Safe debug DLL check: e.g. "assimpd.dll" where "assimp.dll" also exists,
			// or known debug patterns like "*_d.dll", "*-d.dll".
			// Avoids misclassifying release DLLs that naturally end with 'd' (e.g. zstd.dll, sound.dll).
			if (fname.size() <= 5 || !fname.ends_with(".dll"))
			{
				return false;
			}

			if (fname.ends_with("_d.dll") || fname.ends_with("-d.dll"))
			{
				return true;
			}

			if (fname[fname.size() - 5] == 'd')
			{
				std::string nonDebugName = fname.substr(0, fname.size() - 5) + ".dll";
				std::error_code ec;
				if (fs::exists(exeDir / nonDebugName, ec))
				{
					return true;
				}
				// Known thirdparty debug libraries
				if (fname == "assimpd.dll" || fname == "fmodd.dll" || fname == "fmodstudiod.dll")
				{
					return true;
				}
			}

			return false;
		}
	} // namespace

	bool BinaryDeployer::CopyFile(const fs::path& src, const fs::path& dst, std::string& outError)
	{
		std::error_code ec;
		fs::create_directories(dst.parent_path(), ec);
		if (ec)
		{
			outError = "Failed to create directory '" + dst.parent_path().string() + "': " + ec.message();
			return false;
		}

		fs::copy_file(src, dst, fs::copy_options::update_existing, ec);
		if (ec)
		{
			outError = "Failed to copy '" + src.string() + "': " + ec.message();
			return false;
		}
		return true;
	}

	bool BinaryDeployer::Deploy(const fs::path& exeDir, const fs::path& outputDir, const std::string& appName,
								bool isDebugExport, const std::atomic<bool>* cancelFlag)
	{
		std::error_code dirEc;

		// 1. Locate and deploy the Game Executable (renamed to {AppName}.exe)
		std::vector<fs::path> exeCandidates;
		for (const auto& entry : fs::directory_iterator(exeDir, dirEc))
		{
			if (IsCancelled(cancelFlag))
			{
				return false;
			}
			if (!entry.is_regular_file(dirEc))
			{
				continue;
			}

			std::string fname = StringToLower(entry.path().filename().string());
			if (fname.find("editor") != std::string::npos || fname.find("test") != std::string::npos ||
				fname.find("packer") != std::string::npos || fname.find("pack_tool") != std::string::npos ||
				fname.find("generator") != std::string::npos)
			{
				continue;
			}

			if (fname.ends_with(".exe") || (fname.find('.') == std::string::npos && !fname.empty()))
			{
				exeCandidates.push_back(entry.path());
			}
		}

		if (!exeCandidates.empty())
		{
			// Pick preferred match if game exe matches appName or ChainedDecos
			auto it = std::find_if(exeCandidates.begin(), exeCandidates.end(), [&](const fs::path& p) {
				std::string n = StringToLower(p.stem().string());
				return n == StringToLower(appName) || n == "chaineddecos";
			});
			const fs::path& srcExe = (it != exeCandidates.end()) ? *it : exeCandidates[0];

			fs::path dstExe = outputDir / (appName + srcExe.extension().string());
			std::string copyErr;
			if (!CopyFile(srcExe, dstExe, copyErr))
			{
				CH_CORE_ERROR("BinaryDeployer: Executable copy failed: {}", copyErr);
				return false;
			}
		}
		else
		{
			CH_CORE_WARN("BinaryDeployer: No game executable found in '{}'", exeDir.string());
		}

		// 2. Deploy Runtime DLLs
		for (const auto& entry : fs::directory_iterator(exeDir, dirEc))
		{
			if (IsCancelled(cancelFlag))
			{
				return false;
			}
			if (!entry.is_regular_file(dirEc))
			{
				continue;
			}

			const std::string ext = StringToLower(entry.path().extension().string());
			if (ext != ".dll" && ext != ".so" && ext != ".dylib")
			{
				continue;
			}

			const std::string fname = entry.path().filename().string();
			const std::string lowerName = StringToLower(fname);

			// Skip toolchain mismatches and build-time generators
			if (lowerName.find("-vc") != std::string::npos || lowerName.find("generator") != std::string::npos)
			{
				continue;
			}

			// Skip test or editor DLLs
			if (lowerName.find("editor") != std::string::npos || lowerName.find("test") != std::string::npos)
			{
				continue;
			}

			// Filter Debug vs Release
			const bool isDebug = IsDebugDll(lowerName, exeDir);
			if (isDebugExport && !isDebug)
			{
				// In debug export, prefer debug DLL if available
				std::string dbgName = entry.path().stem().string() + "d.dll";
				if (fs::exists(exeDir / dbgName, dirEc))
				{
					continue;
				}
			}
			else if (!isDebugExport && isDebug)
			{
				continue; // Skip debug DLLs in Release export
			}

			std::string copyErr;
			if (!CopyFile(entry.path(), outputDir / fname, copyErr))
			{
				CH_CORE_ERROR("BinaryDeployer: DLL copy failed for '{}': {}", fname, copyErr);
				return false;
			}
		}

		// 3. Deploy Runtime Subdirectories (nethost, dotnet, scripts)
		for (const std::string& subDir : {"nethost", "dotnet", "scripts"})
		{
			if (IsCancelled(cancelFlag))
			{
				return false;
			}

			fs::path srcSub = exeDir / subDir;
			if (fs::exists(srcSub, dirEc))
			{
				fs::path dstSub = outputDir / subDir;
				fs::copy(srcSub, dstSub, fs::copy_options::update_existing | fs::copy_options::recursive, dirEc);

				// Strip generator DLLs from copied subdirectories
				for (const auto& f : fs::recursive_directory_iterator(dstSub, dirEc))
				{
					if (f.is_regular_file(dirEc))
					{
						std::string name = StringToLower(f.path().filename().string());
						if (name.find("generator") != std::string::npos)
						{
							fs::remove(f.path(), dirEc);
						}
					}
				}
			}
		}

		// 4. Deploy Coral Managed Runtime Configs
		static const char* kCoralFiles[] = {"Coral.Managed.runtimeconfig.json", "Coral.Managed.deps.json",
											"Coral.Managed.pdb"};
		for (const auto& file : kCoralFiles)
		{
			fs::path src = exeDir / file;
			if (fs::exists(src, dirEc))
			{
				std::string copyErr;
				CopyFile(src, outputDir / file, copyErr);
			}
		}

		return true;
	}
} // namespace Chained
