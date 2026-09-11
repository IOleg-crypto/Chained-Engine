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

#include "mpio/directory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline static bool testGetDataDirectory()
{
	char* dataDirectory = getDataDirectory(false);
	if (!dataDirectory)
	{
		printf("Failed to get private data directory.\n");
		return false;
	}
	if (strlen(dataDirectory) == 0)
	{
		printf("Invalid private data directory.\n");
		return false;
	}
	free(dataDirectory);

	dataDirectory = getDataDirectory(true);
	if (!dataDirectory)
	{
		printf("Failed to get shared data directory.\n");
		return false;
	}
	if (strlen(dataDirectory) == 0)
	{
		printf("Invalid shared data directory.\n");
		return false;
	}
	free(dataDirectory);

	return true;
}
inline static bool testGetAppDataDirectory()
{
	char* appDataDirectory = getAppDataDirectory("Mpio", false);
	if (!appDataDirectory)
	{
		printf("Failed to get private app data directory.\n");
		return false;
	}
	if (strlen(appDataDirectory) == 0)
	{
		printf("Invalid private app data directory.\n");
		return false;
	}
	free(appDataDirectory);

	appDataDirectory = getAppDataDirectory("Mpio", true);
	if (!appDataDirectory)
	{
		printf("Failed to get shared app data directory.\n");
		return false;
	}
	if (strlen(appDataDirectory) == 0)
	{
		printf("Invalid shared app data directory.\n");
		return false;
	}
	free(appDataDirectory);

	return true;
}
inline static bool testGetResourcesDirectory()
{
	char* resourcesDirectory = getResourcesDirectory();
	if (!resourcesDirectory)
	{
		printf("Failed to get resources directory.\n");
		return false;
	}
	if (strlen(resourcesDirectory) == 0)
	{
		printf("Invalid resources directory.\n");
		return false;
	}
	free(resourcesDirectory);

	return true;
}

int main()
{
	bool result = testGetDataDirectory();
	result |= testGetAppDataDirectory();
	result |= testGetResourcesDirectory();
	return result ? EXIT_SUCCESS : EXIT_FAILURE;
}