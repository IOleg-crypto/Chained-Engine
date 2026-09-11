extern "C" {
#include "pack/writer.h"
#include "pack/common.h"
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

static void printUsage()
{
	std::cout << "Usage: chpacker [options] <pack-path> [file-path-1 item-path-1 ...]\n"
			  << "\n"
			  << "Options:\n"
			  << "  -z <zipThreshold>  Compression threshold (0-100%, default: 10)\n"
			  << "  -v <dataVersion>   Pack data version (default: 0)\n"
			  << "  -s                 Prefer speed (LZ4) over compression size (ZSTD)\n"
			  << "  -m <manifest-file> Read file/item pairs from a manifest file (tab-separated)\n"
			  << "  -h, --help         Show this help message\n";
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printUsage();
		return EXIT_FAILURE;
	}

	float zipThreshold = 0.1f;
	uint32_t dataVersion = 0;
	bool preferSpeed = false;
	std::string manifestFile;
	std::string packPath;
	int argOffset = 1;

	while (argOffset < argc)
	{
		std::string arg = argv[argOffset];
		if (arg == "-z" && argOffset + 1 < argc)
		{
			int zipPercents = std::atoi(argv[++argOffset]);
			if (zipPercents < 0 || zipPercents > 100)
			{
				std::cerr << "Error: zip threshold must be between 0 and 100.\n";
				return EXIT_FAILURE;
			}
			zipThreshold = static_cast<float>(zipPercents) * 0.01f;
			argOffset++;
		}
		else if (arg == "-v" && argOffset + 1 < argc)
		{
			long long version = std::atoll(argv[++argOffset]);
			if (version < 0 || version > UINT32_MAX)
			{
				std::cerr << "Error: data version out of range.\n";
				return EXIT_FAILURE;
			}
			dataVersion = static_cast<uint32_t>(version);
			argOffset++;
		}
		else if (arg == "-s")
		{
			preferSpeed = true;
			argOffset++;
		}
		else if (arg == "-m" && argOffset + 1 < argc)
		{
			manifestFile = argv[++argOffset];
			argOffset++;
		}
		else if (arg == "-h" || arg == "--help")
		{
			printUsage();
			return EXIT_SUCCESS;
		}
		else if (!arg.empty() && arg[0] == '-')
		{
			std::cerr << "Error: unknown option: " << arg << "\n";
			printUsage();
			return EXIT_FAILURE;
		}
		else
		{
			// First non-flag argument is the output pack file path
			if (packPath.empty())
			{
				packPath = argv[argOffset++];
			}
			else
			{
				break;
			}
		}
	}

	if (packPath.empty())
	{
		std::cerr << "Error: Output pack file path is required.\n";
		printUsage();
		return EXIT_FAILURE;
	}

	std::vector<std::string> fileList;

	if (!manifestFile.empty())
	{
		std::ifstream mf(manifestFile);
		if (!mf.is_open())
		{
			std::cerr << "Error: Cannot open manifest file: " << manifestFile << "\n";
			return EXIT_FAILURE;
		}
		std::string line;
		while (std::getline(mf, line))
		{
			// Trim carriage return if present
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			if (line.empty() || line[0] == '#')
			{
				continue;
			}

			size_t tabPos = line.find('\t');
			if (tabPos != std::string::npos)
			{
				std::string filePath = line.substr(0, tabPos);
				std::string itemPath = line.substr(tabPos + 1);
				if (!filePath.empty() && !itemPath.empty())
				{
					fileList.push_back(filePath);
					fileList.push_back(itemPath);
				}
			}
			else
			{
				std::cerr << "Warning: Skipping line without tab delimiter: " << line << "\n";
			}
		}
	}
	else
	{
		while (argOffset < argc)
		{
			fileList.emplace_back(argv[argOffset++]);
		}
	}

	if (fileList.empty() || fileList.size() % 2 != 0)
	{
		std::cerr << "Error: Invalid items count (" << fileList.size()
				  << "). Expected pairs of <file-path> <item-path>.\n";
		return EXIT_FAILURE;
	}

	size_t fileCount = fileList.size() / 2;
	std::vector<const char*> rawPointers;
	rawPointers.reserve(fileList.size());
	for (const auto& entry : fileList)
	{
		rawPointers.push_back(entry.c_str());
	}

	std::cout << "Packaging " << fileCount << " file(s) into " << packPath << "...\n";

	PackResult result = packFiles(packPath.c_str(), static_cast<uint64_t>(fileCount), rawPointers.data(), dataVersion,
								  zipThreshold, preferSpeed, true, nullptr, nullptr);

	if (result != SUCCESS_PACK_RESULT)
	{
		std::cerr << "\nError: " << packResultToString(result) << "\n";
		return EXIT_FAILURE;
	}

	std::cout << "Packaging completed successfully.\n";
	return EXIT_SUCCESS;
}
