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
 * @details See the @ref os.h
 */

#pragma once
#include "mpio/error.hpp"
#include <filesystem>

extern "C"
{
#include "mpio/os.h"
}

namespace mpio
{

using namespace std;

/**
 * @brief Common operating system functions. (MT-Safe)
 * @details See the @ref os.h
 */
class OS
{
public:
	/**
	 * @brief Returns high resolution time stamp in seconds. (MT-Safe)
	 * @details See the @ref getCurrentClock().
	 */
	static double getCurrentClock()
	{
		return ::getCurrentClock();
	}

	/**
	 * @brief Returns system logical CPU count. (MT-Safe)
	 * @details See the @ref getLogicalCpuCount().
	 * @throw Error if failed to get logical CPU count.
	 */
	static int getLogicalCpuCount()
	{
		auto cpuCount = ::getLogicalCpuCount();
		if (cpuCount <= 0)
			throw Error("Failed to get logical CPU count.");
		return cpuCount;
	}

	/**
	 * @brief Returns system physical CPU count. (MT-Safe)
	 * @details See the @ref getPhysicalCpuCount().
	 * @throw Error if failed to get physical CPU count.
	 */
	static int getPhysicalCpuCount()
	{
		auto cpuCount = ::getPhysicalCpuCount();
		if (cpuCount <= 0)
			throw Error("Failed to get physical CPU count.");
		return cpuCount;
	}

	/**
	 * @brief Returns system performance CPU count. (MT-Safe)
	 * @details See the @ref getPerformanceCpuCount().
	 * @throw Error if failed to get performance CPU count.
	 */
	static int getPerformanceCpuCount()
	{
		auto cpuCount = ::getPerformanceCpuCount();
		if (cpuCount <= 0)
			throw Error("Failed to get performance CPU count.");
		return cpuCount;
	}

	/**
	 * @brief Returns system total physical RAM size. (MT-Safe)
	 * @details See the @ref getTotalRamSize().
	 * @throw Error if failed to get total RAM size.
	 */
	static int64_t getTotalRamSize()
	{
		auto ramSize = ::getTotalRamSize();
		if (ramSize <= 0)
			throw Error("Failed to get total RAM size.");
		return ramSize;
	}

	/**
	 * @brief Returns system free physical RAM size. (MT-Safe)
	 * @details See the @ref getFreeRamSize().
	 * @throw Error if failed to get free RAM size.
	 */
	static int64_t getFreeRamSize()
	{
		auto ramSize = ::getFreeRamSize();
		if (ramSize <= 0)
			throw Error("Failed to get free RAM size.");
		return ramSize;
	}

	/**
	 * @brief Returns system CPU name string. (MT-Safe)
	 * @details See the @ref getCpuName().
	 * @throw Error if failed to get CPU name.
	 */
	static string getCpuName()
	{
		auto cpuName = ::getCpuName();
		if (!cpuName)
			throw Error("Failed to get CPU name.");
		auto cpuString = string(cpuName);
		free(cpuName);
		return cpuString;
	}

	/**
	 * @brief Executes specified file with arguments. (MT-Safe)
	 * @details See the @ref executeFileA().
	 *
	 * @param[in] filePath file path string
	 * @param args program arguments
	 *
	 * @return Program exit code or -1 if process stopped or crashed.
	 */
	static int executeFile(const char* filePath, char** args) noexcept
	{
		return executeFileA(filePath, args);
	}
	/**
	 * @brief Executes specified file with arguments. (MT-Safe)
	 * @details See the @ref executeFileVA().
	 *
	 * @param[in] filePath file path string
	 * @param args program arguments
	 *
	 * @return Program exit code or -1 if process stopped or crashed.
	 */
	static int executeFile(const char* filePath, va_list args) noexcept
	{
		va_list stdArgs;
		va_copy(stdArgs, args);
		auto exitCode = executeFileVA(filePath, stdArgs);
		va_end(stdArgs);
		return exitCode;
	}
	/**
	 * @brief Executes specified file with arguments. (MT-Safe)
	 * @details See the @ref executeFileVA().
	 * @warning Always pass NULL as the last argument in the list!!!
	 *
	 * @param[in] filePath file path string
	 * @param args program arguments
	 *
	 * @return Program exit code or -1 if process stopped or crashed.
	 */
	static int executeFile(const char* filePath, ...) noexcept
	{
		va_list args;
		va_start(args, filePath);
		auto exitCode = executeFileVA(filePath, args);
		va_end(args);
		return exitCode;
	}

	/**
	 * @brief Opens file using preferred application. (Explorer, Finder, Browser, etc.)
	 * @details See the @ref executeFileVA().
	 *
	 * @param[in] path directory or file path string
	 * @return True on success, otherwise false.
	 */
	static bool openFileManager(const string& path)
	{
		return ::openFileManager(path.c_str());
	}
	/**
	 * @brief Shows system error message. (Window or popup)
	 * @details See the @ref showError().
	 *
	 * @param[in] title message title string
	 * @param[in] message error message string
	 */
	static void showError(const string& title, const string& message)
	{
		::showError(title.c_str(), message.c_str());
	}
};

}; // mpio