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
 * @brief Common operating system functions.
 */

#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

/**
 * @brief Returns high resolution time stamp in seconds. (MT-Safe)
 * @details That can be used for time-interval measurements.
 * @return The time since some unspecified starting point.
 */
double getCurrentClock();

/**
 * @brief Returns system logical CPU count. (MT-Safe)
 * @details Useful for a background thread pool thread count.
 */
int getLogicalCpuCount();

/**
 * @brief Returns system physical CPU count. (MT-Safe)
 * @details Useful for a foreground thread pool thread count.
 */
int getPhysicalCpuCount();

/**
 * @brief Returns system performance CPU count. (MT-Safe)
 * @details Usefull for a foreground thread pool thread count.
 */
int getPerformanceCpuCount();

/**
 * @brief Returns system total physical RAM size. (MT-Safe)
 * @details Usefull for an OS information logging.
 * @return The total RAM size in bytes on success, otherwise -1.
 */
int64_t getTotalRamSize();

/**
 * @brief Returns system free physical RAM size. (MT-Safe)
 * @details Usefull for an OS information logging.
 * @return The free RAM size in bytes on success, otherwise -1.
 */
int64_t getFreeRamSize();

/**
 * @brief Returns system CPU name string. (MT-Safe)
 * @details Usefull for an OS information logging.
 * @note You should free() the allocated string manually.
 * @return An allocated CPU name string.
 */
char* getCpuName();

/**
 * @brief Executes specified file with arguments. (MT-Safe)
 * @details Safe std::system() function alternative with arguments.
 * @return Program exit code or -1 if process stopped or crashed.
 *
 * @param[in] filePath file path string
 * @param[in] args program arguments (last should be NULL!)
 */
int executeFileA(const char* filePath, char** args);
/**
 * @brief Executes specified file with arguments. (MT-Safe)
 * @details Safe std::system() function alternative with arguments.
 * @return Program exit code or -1 if process stopped or crashed.
 *
 * @param[in] filePath file path string
 * @param args program arguments
 */
int executeFileVA(const char* filePath, va_list args);
/**
 * @brief Executes specified file with arguments. (MT-Safe)
 * @details Safe std::system() function alternative with arguments.
 * @warning Always pass NULL as the last argument in the list!!!
 * @return Program exit code or -1 if process stopped or crashed.
 *
 * @param[in] filePath file path string
 * @param ... program arguments
 */
int executeFile(const char* filePath, ...);

/**
 * @brief Opens file using preferred application. (Explorer, Finder, Browser, etc.)
 * @param[in] path directory or file path string
 * @return True on success, otherwise false.
 */
bool openFileManager(const char* path);
/**
 * @brief Shows system error message. (Window or popup)
 *
 * @param[in] title message title string
 * @param[in] message error message string
 */
void showError(const char* title, const char* message);