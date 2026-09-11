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
#include <sys/param.h>
#include <CoreFoundation/CFBundle.h>
#include <Foundation/NSFileManager.h>
#include <assert.h>

char* getDataDirectory(bool isShared)
{
	NSArray<NSString*>* array = NSSearchPathForDirectoriesInDomains(
		NSApplicationSupportDirectory,
		isShared ? NSLocalDomainMask : NSUserDomainMask, YES);
	if (!array)
		return NULL;

	NSString* string = [array lastObject];
	if (!string)
		return NULL;

	size_t length = [string length];
	char* path = malloc(length + 1);
	if (!path)
		return NULL;

	memcpy(path, [string UTF8String], length + 1);
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

	path[dataPathLength] = '/';
	memcpy(path + dataPathLength + 1, appName, appNameLength + 1);
	return path;
}
char* getResourcesDirectory()
{
	CFBundleRef bundle = CFBundleGetMainBundle();
	if (!bundle)
		return NULL;

	char* string = malloc(MAXPATHLEN + 1);
	if (!string)
		return NULL;

	CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(bundle);
	if (!CFURLGetFileSystemRepresentation(resourcesURL,
		true, (UInt8*)string, MAXPATHLEN))
	{
		CFRelease(resourcesURL);
		return NULL;
	}

	CFRelease(resourcesURL);
	size_t length = strlen(string);
	char* path = realloc(string, length + 1);
	if (!path)
		return string;
	return path;
}