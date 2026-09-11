# MPIO

A [library](https://github.com/cfnptr/mpio) providing generic interface for file system **input / output** operations across different platforms.

See the [documentation](https://cfnptr.github.io/mpio).

## Features

* Common directory and file functions
* App data and resources path getters
* CPU name (brand, model) getters
* Free and total RAM size getters
* Logical, physical, performance CPU count getters
* Current clock (time stamp) getter
* Universal file execution
* System error window show
* C and C++ implementations
* Supports Windows, macOS and Linux

## Build requirements

* C99 compiler
* C++17 compiler (optional)
* Objective-C compiler (macOS only)
* [Git 2.53+](https://git-scm.com/)
* [CMake 3.16+](https://cmake.org/)

Use building [instructions](BUILDING.md) to install all required tools and libraries.

### CMake options

| Name              | Description               | Default value |
|-------------------|---------------------------|---------------|
| MPIO_BUILD_SHARED | Build MPIO shared library | `ON`          |
| MPIO_BUILD_TESTS  | Build MPIO library tests  | `ON`          |

### CMake targets

| Name        | Description          | Windows | macOS    | Linux |
|-------------|----------------------|---------|----------|-------|
| mpio-static | Static MPIO library  | `.lib`  | `.a`     | `.a`  |
| mpio-shared | Dynamic MPIO library | `.dll`  | `.dylib` | `.so` |

## Cloning

```
git clone https://github.com/cfnptr/mpio
```

## Building ![CI](https://github.com/cfnptr/mpio/actions/workflows/cmake.yml/badge.svg)

* Windows: ```./scripts/build-release.bat```
* macOS / Linux: ```./scripts/build-release.sh```
