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
 */

#pragma once
#include <stdbool.h>

/**
 * @brief Creates a new directory, if it do not already exist. (MT-Safe)
 * @param[in] path target directory path string
 * @return True if a new directory was created, otherwise false.
 */
bool createDirectory(const char* path);

/**
 * @brief Returns true if directory exists. (MT-Safe)
 * @param[in] path target directory path string
 */
bool isDirectoryExists(const char* path);

/**
 * @brief Returns application data directory. (MT-Safe)
 * @details The resulting path can be used to store data between application runs, saves, downloaded updates.
 * @note You should free() allocated string manually.
 * 
 * @param isShared is data directory shared between multiple users
 * @return An allocated data directory string on success, otherwise NULL.
 */
char* getDataDirectory(bool isShared);

/**
 * @brief Returns application data directory + name. (MT-Safe)
 * @details The resulting path can be used to store data between application runs, saves, downloaded updates.
 * @note You should free() allocated string manually.
 *
 * @param[in] appName target application name string
 * @param isShared is data directory shared between multiple users
 * @return An allocated data directory string on success, otherwise NULL.
 */
char* getAppDataDirectory(const char* appName, bool isShared);

/**
 * @brief Returns bundled resources directory. (MT-Safe)
 *
 * @details
 * This function returns the path to the resources within the built application.
 * It is needed on macOS, iOS, Android to load bundled resources at runtime.
 * 
 * @note You should free() allocated string manually.
 * @return An allocated resources directory string on success, otherwise NULL.
 */
char* getResourcesDirectory();