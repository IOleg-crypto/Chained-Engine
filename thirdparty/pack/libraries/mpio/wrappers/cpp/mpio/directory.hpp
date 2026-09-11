// Copyright 2021-2026 Nikita Fediuchin. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/***********************************************************************************************************************
 * @file
 * @brief Common directory functions.
 * @details See the @ref directory.h
 */

#pragma once
#include "mpio/error.hpp"
#include <filesystem>

extern "C"
{
#include "mpio/directory.h"
}

namespace mpio
{

using namespace std;

/**
 * @brief Common directory functions. (MT-Safe)
 * @details See the @ref directory.h
 */
class Directory
{
public:
	// Note: Use std:: functions instead of createDirectory(), isDirectoryExists()!

	/**
	 * @brief Returns application data directory. (MT-Safe)
	 * @details See the @ref getDataDirectory().
	 * 
	 * @param isShared is data directory shared between multiple users
	 * @return Data directory path on success.
	 * @throw Error if failed to get data directory.
	 */
	static filesystem::path getDataPath(bool isShared = false)
	{
		auto dataPath = getDataDirectory(isShared);
		if (!dataPath)
			throw Error("Failed to get data directory.");
		auto path = filesystem::path(dataPath);
		free(dataPath);
		return path;
	}

	/**
	 * @brief Returns application data directory + name. (MT-Safe)
	 * @details See the @ref getAppDataDirectory().
	 *
	 * @param appName target application name string
	 * @param isShared is data directory shared between multiple users
	 * @return Data directory path on success.
	 * @throw Error if failed to get app data directory.
	 */
	static filesystem::path getAppDataPath(const string& appName, bool isShared = false)
	{
		auto dataPath = getAppDataDirectory(appName.c_str(), isShared);
		if (!dataPath)
			throw Error("Failed to get app data directory.");
		auto path = filesystem::path(dataPath);
		free(dataPath);
		return path;
	}

	/**
	 * @brief Returns bundled resources directory. (MT-Safe)
	 * @details See the @ref getResourcesDirectory().
	 * 
	 * @return Resources directory path on success.
	 * @throw Error if failed to get resources directory.
	 */
	static filesystem::path getResourcesPath()
	{
		auto dataPath = getResourcesDirectory();
		if (!dataPath)
			throw Error("Failed to get resources directory.");
		auto path = filesystem::path(dataPath);
		free(dataPath);
		return path;
	}
};

} // mpio