# Copyright 2021-2026 Nikita Fediuchin. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "Prevented in-tree build")
endif()

if(TARGET mpio-static)
	return()
endif()

project(mpio VERSION 1.6.0 LANGUAGES C
	DESCRIPTION "Multi-platform common IO library"
	HOMEPAGE_URL "https://github.com/cfnptr/mpio")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

option(MPIO_BUILD_SHARED "Build MPIO shared library" ON)
option(MPIO_BUILD_TESTS "Build MPIO library tests" ON)

configure_file(cmake/defines.h.in include/mpio/defines.h)

set(MPIO_SOURCES source/directory.c source/os.c)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	enable_language(OBJC)

	set(MPIO_APPLE_SOURCES source/directory.m)
	list(APPEND MPIO_SOURCES ${MPIO_APPLE_SOURCES})

	# Note: We need this to fix Xcode compilation errors.
	set_source_files_properties(${MPIO_APPLE_SOURCES} PROPERTIES LANGUAGE OBJC)
endif()

set(MPIO_INCLUDE_DIRS ${PROJECT_BINARY_DIR}/include
	${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/wrappers/cpp)

add_library(mpio-static STATIC ${MPIO_SOURCES})
target_include_directories(mpio-static PUBLIC ${MPIO_INCLUDE_DIRS})

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_link_libraries(mpio-static PUBLIC
		"-framework Foundation -framework CoreFoundation")
endif()

if(MPIO_BUILD_SHARED)
	add_library(mpio-shared SHARED ${MPIO_SOURCES})
	set_target_properties(mpio-shared PROPERTIES
		OUTPUT_NAME "mpio" WINDOWS_EXPORT_ALL_SYMBOLS ON)
	target_include_directories(mpio-shared PUBLIC ${MPIO_INCLUDE_DIRS})
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		target_link_libraries(mpio-shared PUBLIC
			"-framework Foundation -framework CoreFoundation")
	endif()
endif()

if(MPIO_BUILD_TESTS)
	enable_testing()

	add_executable(TestMpioDirectory tests/test_directory.c)
	target_link_libraries(TestMpioDirectory PUBLIC mpio-static)
	add_test(NAME TestMpioDirectory COMMAND TestMpioDirectory)

	add_executable(TestMpioOS tests/test_os.c)
	target_link_libraries(TestMpioOS PUBLIC mpio-static)
	add_test(NAME TestMpioOS COMMAND TestMpioOS)
endif()
