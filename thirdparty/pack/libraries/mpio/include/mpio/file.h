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
 * @brief Common file functions.
 */

#pragma once
#include <stdio.h>

#if __linux__ || __APPLE__

/**
 * @brief Opens a file indicated by filename.
 * 
 * @details File access flags:
 * - "r"  - Open a file for reading
 * - "w"  - Create a file for writing
 * - "a"  - Append to a file 	write to end
 * - "r+" - Open a file for read/write
 * - "w+" - Create a file for read/write
 * - "a+" - Open a file for read/write
 * 
 * @param[in] filePath file name to associate the file stream to 
 * @param[in] mode null-terminated character string determining file access mode
 * 
 * @return A file stream on success, otherwise NULL.
 */
#define openFile(filePath, mode) fopen(filePath, mode)

/**
 * @brief Changes the current file position associated with stream to a new location within the file.
 * 
 * @details
 * The origin must be one of the following constants defined in stdio.h
 * 
 * - SEEK_SET - Beginning of file
 * - SEEK_CUR - Current position of file pointer
 * - SEEK_END - End of file
 * 
 * @param[in] file file stream to modify
 * @param offset number of characters to shift the position relative to origin
 * @param origin position to which offset is added
 * 
 * @return ​0​ upon success, nonzero value otherwise. 
 */
#define seekFile(file, offset, origin) fseeko(file, offset, origin)

/**
 * @brief Returns the file position indicator for the file stream stream.
 * 
 * @details
 * If the stream is open in binary mode, the value obtained by this function is 
 * the number of bytes from the beginning of the file. If the stream is open in text mode, 
 * the value returned by this function is unspecified and is only meaningful as the input to fseek().
 * 
 * @param[in] file file stream to examine 
 * @return File position indicator on success or -1L if failure occurs. 
 */
#define tellFile(file) ftello(file)

#elif _WIN32

/***********************************************************************************************************************
 * @brief Opens a file indicated by filename.
 * 
 * @details
 * File access flags:
 * - "r"  - Open a file for reading
 * - "w"  - Create a file for writing
 * - "a"  - Append to a file 	write to end
 * - "r+" - Open a file for read/write
 * - "w+" - Create a file for read/write
 * - "a+" - Open a file for read/write
 * 
 * @param[in] filePath file name to associate the file stream to 
 * @param[in] mode null-terminated character string determining file access mode
 * 
 * @return A file stream on success, otherwise NULL.
 */
inline static FILE* openFile(const char* filePath, const char* mode)
{
	FILE* file;
	errno_t error = fopen_s(&file, filePath, mode);
	if (error != 0)
		return NULL;
	return file;
}

/**
 * @brief Changes the current file position associated with stream to a new location within the file.
 * 
 * @details
 * The origin must be one of the following constants defined in stdio.h
 * 
 * - SEEK_SET - Beginning of file
 * - SEEK_CUR - Current position of file pointer
 * - SEEK_END - End of file
 * 
 * @param[in] file file stream to modify
 * @param offset number of characters to shift the position relative to origin
 * @param origin position to which offset is added
 * 
 * @return ​0​ upon success, nonzero value otherwise. 
 */
#define seekFile(file, offset, origin) _fseeki64(file, offset, origin)

/**
 * @brief Returns the file position indicator for the file stream stream.
 * 
 * @details
 * If the stream is open in binary mode, the value obtained by this function is 
 * the number of bytes from the beginning of the file. If the stream is open in text mode, 
 * the value returned by this function is unspecified and is only meaningful as the input to fseek().
 * 
 * @param[in] file file stream to examine 
 * @return File position indicator on success or -1L if failure occurs. 
 */
#define tellFile(file) _ftelli64(file)
#else
#error Unsupported operating system
#endif

/**
 * @brief Closes the given file stream.
 * 
 * @details
 * Any unwritten buffered data are flushed to the OS. Any unread buffered data are discarded. 
 * The behavior is undefined if the value of the pointer stream is used after fclose returns.
 * 
 * @param[in] file the file stream to close 
 * @return ​0​ on success, EOF otherwise.
 */
#define closeFile(file) fclose(file)