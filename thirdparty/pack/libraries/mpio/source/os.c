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

#include "mpio/os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if __linux__ || __APPLE__
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
	#if __x86_64__ || __i386__
	#include <cpuid.h>
	#endif
#elif _WIN32
#include <intrin.h>
#include <windows.h>
#include <process.h>
#else
#error Unknown operating system
#endif

#if __linux__
#include <sys/sysinfo.h>
#elif __APPLE__
#include <sys/sysctl.h>
#include <mach/mach_host.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#if __x86_64__ || _M_X64 || __i386__
#if __linux__ || __APPLE__
#define CPUID(id, cpuInfo) __cpuid(id, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3])
#elif _WIN32
#define CPUID(id, cpuInfo) __cpuid((int*)cpuInfo, id)
#endif
#endif

double getCurrentClock()
{
#if __linux__ || __APPLE__
	struct timespec time;
	if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) abort();
	return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
#elif _WIN32
	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency) != TRUE)
		abort();
	LARGE_INTEGER counter;
	if (QueryPerformanceCounter(&counter) != TRUE)
		abort();
	return (double)counter.QuadPart / (double)frequency.QuadPart;
#endif
}

//**********************************************************************************************************************
int getLogicalCpuCount()
{
	int cpuCount = -1;
#if __linux__
	cpuCount = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (cpuCount <= 0)
	{
		FILE* file = fopen("/proc/cpuinfo", "r");
		if (file)
		{
			const size_t bufferSize = 256;
			char* line = malloc(bufferSize);
			if (!line)
			{
				fclose(file);
				return -1;
			}

			while (fgets(line, bufferSize, file))
			{
				size_t lineLength = strlen(line);
				if (lineLength == 0 || memcmp(line, "processor", 9) != 0)
					continue;
				char* pointer = memchr(line + 9, ':', lineLength - 9);
				if (!pointer)
					continue;

				int count = atoi(pointer + 1) + 1;
				if (count > cpuCount)
					cpuCount = count;
			}
			free(line); fclose(file);
		}
	}
#elif __APPLE__
	size_t size = sizeof(cpuCount);
	sysctlbyname("hw.logicalcpu", &cpuCount, &size, NULL, 0);

	if (cpuCount <= 0)
	{
		size = sizeof(cpuCount);
		sysctlbyname("machdep.cpu.thread_count", &cpuCount, &size, NULL, 0);
	}
	if (cpuCount <= 0)
	{
		size = sizeof(cpuCount);
		sysctlbyname("hw.activecpu", &cpuCount, &size, NULL, 0);
	}
#elif _WIN32
	SYSTEM_INFO systemInfo; GetSystemInfo(&systemInfo);
	cpuCount = (int)systemInfo.dwNumberOfProcessors;
#endif
	return cpuCount;
}

//**********************************************************************************************************************
int getPhysicalCpuCount()
{
	int cpuCount = -1;
#if __linux__
	int processorCount = -1;
	FILE* file = fopen("/proc/cpuinfo", "r");
	if (file)
	{
		const size_t bufferSize = 256;
		char* line = malloc(bufferSize);
		if (!line)
		{
			fclose(file);
			return -1;
		}

		while (fgets(line, bufferSize, file))
		{
			size_t lineLength = strlen(line);
			if (lineLength == 0)
				continue;

			if (memcmp(line, "cpu cores", 9) == 0)
			{
				char* pointer = memchr(line + 9, ':', lineLength - 9);
				if (!pointer)
					continue;
				cpuCount = atoi(pointer + 1);
				free(line); fclose(file);
				return cpuCount;
			}
			if (memcmp(line, "processor", 9) == 0)
			{
				char* pointer = memchr(line + 9, ':', lineLength - 9);
				if (!pointer)
					continue;
				int count = atoi(pointer + 1) + 1;
				if (count > processorCount)
					processorCount = count;
			}
			if (memcmp(line, "core id", 7) != 0)
			{
				char* pointer = memchr(line + 7, ':', lineLength - 7);
				if (!pointer)
					continue;
				int count = atoi(pointer + 1) + 1;
				if (count > cpuCount)
					cpuCount = count;
			}
		}
		free(line); fclose(file);
	}

	if (cpuCount <= 0)
		cpuCount = processorCount;
#elif __APPLE__
	size_t size = sizeof(cpuCount);
	sysctlbyname("hw.physicalcpu", &cpuCount, &size, NULL, 0);

	if (cpuCount <= 0)
	{
		size = sizeof(cpuCount);
		sysctlbyname("machdep.cpu.core_count", &cpuCount, &size, NULL, 0);
	}
#elif _WIN32
	DWORD infoSize = 0; GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &infoSize);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return -1;

	char* infoBuffer = malloc(infoSize);
	if (!infoBuffer)
		return -1;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)infoBuffer;
	if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &infoSize) != TRUE)
	{
		free(infoBuffer);
		return -1;
	}

	cpuCount = 0; size_t offset = 0;
	do
	{
		const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX currentInfo =
			(const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(infoBuffer + offset);
		offset += currentInfo->Size; cpuCount++;
	}
	while (offset < infoSize);

	free(infoBuffer);
#endif
	return cpuCount;
}

//**********************************************************************************************************************
int getPerformanceCpuCount()
{
	int cpuCount = -1;
#if __linux__
	// TODO: use libcpuid approach with cpuid and affinity.
	// https://github.com/anrieff/libcpuid/blob/master/libcpuid/cpuid_main.c#L96
#elif _WIN32
	DWORD infoSize = 0; GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &infoSize);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return -1;

	char* infoBuffer = malloc(infoSize);
	if (!infoBuffer)
		return -1;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)infoBuffer;
	if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &infoSize) != TRUE)
	{
		free(infoBuffer);
		return -1;
	}

	cpuCount = 0; size_t offset = 0;
	do
	{
		const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX currentInfo =
			(const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(infoBuffer + offset);
		if (currentInfo->Relationship == RelationProcessorCore)
		{
			if (currentInfo->Processor.EfficiencyClass > 0)
				cpuCount++;
		}
		offset += currentInfo->Size;
	} while (offset < infoSize);

	free(infoBuffer);
#elif __APPLE__
	size_t size = sizeof(cpuCount);
	sysctlbyname("hw.perflevel0.physicalcpu", &cpuCount, &size, NULL, 0);
#endif
	if(cpuCount <= 0)
		cpuCount = getPhysicalCpuCount();
	return cpuCount;
}

//**********************************************************************************************************************
int64_t getTotalRamSize()
{
#if __linux__
	struct sysinfo info;
	if (sysinfo(&info) != 0)
		return -1;
	return (int64_t)info.totalram * (int64_t)info.mem_unit;
#elif __APPLE__
	int mib [] = { CTL_HW, HW_MEMSIZE };
	int64_t value = 0; size_t length = sizeof(int64_t);
	if (sysctl(mib, 2, &value, &length, NULL, 0) != 0)
		return -1;
	return value;
#elif _WIN32
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	if (GlobalMemoryStatusEx(&statex) == FALSE)
		return -1;
	return statex.ullTotalPhys;
#endif
}

int64_t getFreeRamSize()
{
#if __linux__
	FILE* file = fopen("/proc/meminfo", "r");
	if (file)
	{
		const size_t bufferSize = 256;
		char* line = malloc(bufferSize);
		if (!line)
		{
			fclose(file);
			return -1;
		}
	
		int64_t memFree = 0, buffers = 0, cached = 0, sReclaimable = 0;
		while (fgets(line, bufferSize, file))
		{
			size_t lineLength = strlen(line);
			if (lineLength == 0)
				continue;

			if (memcmp(line, "MemAvailable", 12) == 0)
			{
				char* pointer = memchr(line + 12, ':', lineLength - 12);
				if (!pointer)
					continue;
				memFree = atoll(pointer + 1);
				free(line); fclose(file);
				return memFree * 1024;
			}

			if (memcmp(line, "MemFree", 7) == 0)
			{
				char* pointer = memchr(line + 7, ':', lineLength - 7);
				if (!pointer)
					continue;
				memFree = atoll(pointer + 1);
			}
			else if (memcmp(line, "Buffers", 7) == 0)
			{
				char* pointer = memchr(line + 7, ':', lineLength - 7);
				if (!pointer)
					continue;
				buffers = atoll(pointer + 1);
			}
			else if (memcmp(line, "Cached", 6) == 0)
			{
				char* pointer = memchr(line + 6, ':', lineLength - 6);
				if (!pointer)
					continue;
				cached = atoll(pointer + 1);
			}
			else if (memcmp(line, "SReclaimable", 12) == 0)
			{
				char* pointer = memchr(line + 12, ':', lineLength - 12);
				if (!pointer)
					continue;
				sReclaimable = atoll(pointer + 1);
			}
		}
		free(line); fclose(file);

		return (memFree + buffers + cached + sReclaimable) * 1024;
	}

	struct sysinfo info;
	if (sysinfo(&info) != 0)
		return -1;
	return (int64_t)info.freeram * (int64_t)info.mem_unit;
#elif __APPLE__
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT; vm_statistics64_data_t vmStat;
	if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStat, &count) != KERN_SUCCESS)
		return -1;
	return (vmStat.inactive_count + vmStat.free_count + vmStat.speculative_count) * (int64_t)getpagesize();
#elif _WIN32
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	if (GlobalMemoryStatusEx(&statex) == FALSE)
		return -1;
	return statex.ullAvailPhys;
#endif
}

//**********************************************************************************************************************
char* getCpuName()
{
	char* cpuName = calloc(65, sizeof(char));
	if (!cpuName)
		return NULL;

#if __x86_64__ || _M_X64 || __i386__
	unsigned int cpuInfo[4] = { 0, 0, 0, 0 };
	CPUID(0x80000000, cpuInfo);

	unsigned int nExIds = cpuInfo[0];
	for (unsigned int i = 0x80000000; i <= nExIds; ++i)
	{
		CPUID(i, cpuInfo);
		if (i == 0x80000002)
			memcpy(cpuName, cpuInfo, sizeof(cpuInfo));
		else if (i == 0x80000003)
			memcpy(cpuName + 16, cpuInfo, sizeof(cpuInfo));
		else if (i == 0x80000004)
			memcpy(cpuName + 32, cpuInfo, sizeof(cpuInfo));
	}
#else
	memcpy(cpuName, "Unknown\0", 8);

	#if __APPLE__
	size_t brandLength = 64;
	sysctlbyname("machdep.cpu.brand_string", cpuName, &brandLength, NULL, 0);
	#else
	FILE* file = fopen("/proc/cpuinfo", "r");
	if (file)
	{
		const size_t bufferSize = 256;
		char* line = malloc(bufferSize);
		if (!line)
		{
			fclose(file);
			return cpuName;
		}

		while (fgets(line, bufferSize, file))
		{
			size_t lineLength = strlen(line);
			if (lineLength == 0 || memcmp(line, "model name", 10) != 0 && memcmp(line, "Model", 5) != 0)
				continue;
			char* pointer = memchr(line + 5, ':', lineLength - 5);
			if (!pointer)
				continue;

			size_t index = (pointer - line) + 2;
			if (index >= lineLength)
				continue;

			for (size_t i = index; i < lineLength; i++)
			{
				char value = line[i];
				if (value != '\n' && value != '\0')
					continue;

				if (i > 64)
				{
					free(line); fclose(file);
					return cpuName;
				}

				size_t count = i - index;
				memcpy(cpuName, line + index, count);
				cpuName[count] = '\0';
				break;
			}
		}
		free(line); fclose(file);
	}
	#endif
#endif

	size_t nameLength = strlen(cpuName);
	for (int64_t i = nameLength - 1; i > 0; i--)
	{
		if (cpuName[i] != ' ')
		{
			nameLength = i + 1;
			break;
		}
	}
	cpuName[nameLength] = '\0';

	char* newCpuName = realloc(cpuName, nameLength + 1);
	if (!newCpuName)
	{
		free(cpuName);
		return NULL;
	}
	return newCpuName;
}

//**********************************************************************************************************************
int executeFileA(const char* filePath, char** args)
{
	assert(filePath);
	assert(args);

#if __linux__ || __APPLE__
	pid_t pid = fork();
	if (pid < 0)
		return -1;

	if (pid == 0)
	{
		execvp(filePath, args);
		_exit(1);
	}

	int status = 0; waitpid(pid, &status, 0);
	if (!WIFEXITED(status))
		return -1;
	return WEXITSTATUS(status);
#elif _WIN32
	return _spawnvp(_P_WAIT, filePath, args);
#endif
}
int executeFileVA(const char* filePath, va_list args)
{
	assert(filePath);

	size_t capacity = 2;
	char** argv = malloc(capacity * sizeof(const char*));
	if (!argv)
		return -1;

	argv[0] = (char*)filePath;
	char* arg; size_t count = 1; 
	do
	{
		arg = va_arg(args, char*);
		if (count >= capacity)
		{
			capacity *= 2;
			char** newArgv = realloc(argv, capacity * sizeof(char*));
			if (!newArgv)
			{
				free(argv);
				return -1;
			}
			argv = newArgv;
		}
		argv[count++] = arg;
	}
	while (arg != NULL);

	return executeFileA(filePath, argv);
}
int executeFile(const char* filePath, ...)
{
	assert(filePath);
	va_list args;
	va_start(args, filePath);
	int exitCode = executeFileVA(filePath, args);
	va_end(args);
	return exitCode;
}

//**********************************************************************************************************************
bool openFileManager(const char* path)
{
#if __linux__
	return executeFile("xdg-open", path, NULL) == 0;
#elif __APPLE__
	return executeFile("open", path, NULL) == 0;
#elif _WIN32
	return executeFile("cmd", "/c", "start", path, NULL) == 0;
#endif
}
void showError(const char* title, const char* message)
{
#if __linux__
	executeFile("notify-send", "-a", "Garden Engine", "-u", "critical", title, message, NULL);
#elif __APPLE__
	CFStringRef header = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
	CFStringRef msg = CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8);
	CFUserNotificationDisplayAlert(0, kCFUserNotificationCautionAlertLevel, 
		NULL, NULL, NULL, header, msg, NULL, NULL, NULL, NULL);
	CFRelease(header); CFRelease(msg);
#elif _WIN32
	MessageBoxA(NULL, message, title, MB_ICONERROR | MB_SYSTEMMODAL);
#endif
}