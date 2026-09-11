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
#include <assert.h>

#if __linux__ || __APPLE__
#include <sys/stat.h>

bool createDirectory(const char* path)
{
	assert(path != NULL);
	return mkdir(path, 0777) == 0;
}
bool isDirectoryExists(const char* path)
{
	assert(path != NULL);
	struct stat sb;
	return stat(path, &sb) == 0 && S_ISDIR(sb.st_mode);
}

#if __linux__
#include <limits.h>
#include <stdlib.h>
#include <string.h>

char* getDataDirectory(bool isShared)
{
	char* homePath = getenv("HOME");
	if (!homePath)
		homePath = "~";
	size_t length = strlen(homePath);
	char* path = malloc(length + 1);
	if (!path)
		return NULL;
	memcpy(path, homePath, length + 1);
	return path;
}
char* getAppDataDirectory(const char* appName, bool isShared)
{
	assert(appName != NULL);
	char* dataPath = getDataDirectory(isShared);
	if (!dataPath)
		return NULL;

	size_t dataPathLength = strlen(dataPath);
	size_t appNameLength = strlen(appName);
	char* path = realloc(dataPath, dataPathLength + appNameLength + 3);
	if (!path)
	{
		free(dataPath);
		return NULL;
	}

	path[dataPathLength] = '/';
	path[dataPathLength + 1] = '.';
	memcpy(path + dataPathLength + 2, appName, appNameLength + 1);
	return path;
}
char* getResourcesDirectory()
{
	const char* resourcesPath = "resources"; // TODO: support flatpak/snap.
	size_t resourcesPathLength = strlen(resourcesPath);
	char* path = malloc(resourcesPathLength + 1);
	if (!path)
		return NULL;
	memcpy(path, resourcesPath, resourcesPathLength + 1);
	return path; 
}
#endif

#elif _WIN32
//**********************************************************************************************************************
#include <windows.h>
#include <initguid.h>
#include <knownfolders.h>
#include <ShlObj.h>

bool createDirectory(const char* path)
{
	assert(path != NULL);
	return CreateDirectoryA(path, NULL) == TRUE;
}
bool isDirectoryExists(const char* path)
{
	assert(path != NULL);
	DWORD attribs = GetFileAttributes(path);
	return (attribs != INVALID_FILE_ATTRIBUTES &&
		(attribs & FILE_ATTRIBUTE_DIRECTORY));
}

char* getDataDirectory(bool isShared)
{
	PWSTR wideDataPath;
	HRESULT result = SHGetKnownFolderPath(isShared ? &FOLDERID_ProgramData : 
		&FOLDERID_RoamingAppData, 0, NULL, &wideDataPath);
	if (FAILED(result))
	{
		CoTaskMemFree(wideDataPath);
		return NULL;
	}

	size_t wideLength = wcslen(wideDataPath);
	size_t byteSize = wideLength * 4 + 1;
	char* dataPath = malloc(byteSize);
	if (!dataPath)
	{
		CoTaskMemFree(wideDataPath);
		return NULL;
	}

	size_t length = 0;
	errno_t error = wcstombs_s(&length, dataPath, byteSize, wideDataPath, wideLength * 4);
	CoTaskMemFree(wideDataPath);
	if (error != 0)
	{
		free(dataPath);
		return NULL;
	}

	char* path = realloc(dataPath, length + 1);
	if (!path)
	{
		free(dataPath);
		return NULL;
	}

	path[length] = '\0';
	return path;
}
char* getAppDataDirectory(const char* appName, bool isShared)
{
	assert(appName != NULL);
	char* dataPath = getDataDirectory(isShared);
	if (!dataPath)
		return NULL;

	size_t dataPathLength = strlen(dataPath);
	size_t appNameLength = strlen(appName);
	char* path = realloc(dataPath, dataPathLength + appNameLength + 2);
	if (!path)
	{
		free(dataPath);
		return NULL;
	}

	path[dataPathLength] = '\\';
	memcpy(path + dataPathLength + 1, appName, appNameLength + 1);
	return path;
}
char* getResourcesDirectory()
{
	const char* resourcesPath = "resources"; // TODO: support .exe packed resources
	size_t resourcesPathLength = strlen(resourcesPath);
	char* path = malloc(resourcesPathLength + 1);
	if (!path)
		return NULL;
	memcpy(path, resourcesPath, resourcesPathLength + 1);
	return path; 
}
#else
#error Unknown operating system
#endif