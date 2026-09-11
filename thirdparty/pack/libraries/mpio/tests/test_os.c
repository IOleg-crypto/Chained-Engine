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
#include <stdbool.h>
#include <string.h>

inline static bool testGetCurrentClock()
{
	double startClock = getCurrentClock();
	printf("Current clock: %lf\n", startClock);
	fflush(stdout);
	double stopClock = getCurrentClock();

	if (stopClock - startClock <= 0.0)
	{
		printf("Invalid clock time difference.\n");
		return false;
	}

	return true;
}
inline static bool testGetLogicalCpuCount()
{
	int cpuCount = getLogicalCpuCount();
	if (cpuCount <= 0)
	{
		printf("Invalid logical CPU count.\n");
		return false;
	}

	printf("Logical CPU count: %d\n", cpuCount);
	return true;
}
inline static bool testGetPhysicalCpuCount()
{
	int cpuCount = getPhysicalCpuCount();
	if (cpuCount <= 0)
	{
		printf("Invalid physical CPU count.\n");
		return false;
	}

	printf("Physical CPU count: %d\n", cpuCount);
	return true;
}
inline static bool testGetPerformanceCpuCount()
{
	int cpuCount = getPerformanceCpuCount();
	if (cpuCount <= 0)
	{
		printf("Invalid performance CPU count.\n");
		return false;
	}

	printf("Performance CPU count: %d\n", cpuCount);
	return true;
}
inline static bool testGetTotalRamSize()
{
	int64_t ramSize = getTotalRamSize();
	if (ramSize < 0)
	{
		printf("Failed to get total RAM size.\n");
		return false;
	}

	printf("Total RAM size: %lld\n", ramSize);
	return true;
}
inline static bool testGetFreeRamSize()
{
	int64_t ramSize = getFreeRamSize();
	if (ramSize < 0)
	{
		printf("Failed to get free RAM size.\n");
		return false;
	}

	printf("Free RAM size: %lld\n", ramSize);
	return true;
}
inline static bool testGetCpuName()
{
	char* cpuName = getCpuName();
	if (!cpuName)
	{
		printf("Failed to get CPU name.\n");
		return false;
	}
	if (strlen(cpuName) == 0)
	{
		printf("Invalid CPU name.\n");
		return false;
	}

	printf("CPU name: %s\n", cpuName);
	free(cpuName);
	return true;
}

int main()
{
	bool result = testGetCurrentClock();
	result |= testGetLogicalCpuCount();
	result |= testGetPhysicalCpuCount();
	result |= testGetPerformanceCpuCount();
	result |= testGetTotalRamSize();
	result |= testGetFreeRamSize();
	result |= testGetCpuName();
	return result ? EXIT_SUCCESS : EXIT_FAILURE;
}