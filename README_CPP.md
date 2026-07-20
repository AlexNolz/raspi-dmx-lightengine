# C++ Light Engine

This directory layout is the starting point for the C++ migration. The current
Python implementation remains available as a behavioral reference.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler (GCC 10+, Clang 12+, or a recent MSVC)
- VS Code extensions recommended by `.vscode/extensions.json`

On this Ubuntu/WSL development machine:

```sh
sudo apt update
sudo apt install cmake build-essential gdb
```

This workspace also supports a local CMake install in `.tools/cmake-python`.
When system CMake is not available, install it with:

```sh
python3 -m pip install --target .tools/cmake-python cmake
.tools/cmake-python/cmake/data/bin/cmake --version
```

On Alpine Linux:

```sh
apk add build-base cmake gdb
```

## PC debug build

```sh
cmake --preset pc-debug
cmake --build --preset pc-debug
ctest --preset pc-debug
./build/pc-debug/light-engine
```

For an optimized build:

```sh
cmake --preset pc-release
cmake --build --preset pc-release
```

## Raspberry Pi build and deploy

For a native build directly on the Raspberry Pi:

```sh
cmake --preset raspi-native-release
cmake --build --preset raspi-native-release
```

For a cross build on the development PC, install or provide an ARMv6 Alpine
toolchain. The preset expects `arm-linux-musleabihf-g++` by default. Override it
when needed:

```sh
export RASPI_CXX=/path/to/arm-linux-musleabihf-g++
export RASPI_SYSROOT=/path/to/raspi/sysroot
cmake --preset raspi-cross-release
cmake --build --preset raspi-cross-release
```

Deploy the cross-built binary to `root@raspi-dmx`:

```sh
cmake --build --preset deploy-raspi
```

## VS Code

Open the repository folder, install the recommended extensions, and select the
`pc-debug` CMake preset for local debugging or `raspi-cross-release` for the
Pi build. CMake Tools generates `compile_commands.json`, which is used for
IntelliSense, navigation, diagnostics, and refactoring.

The default build task is available with `Ctrl+Shift+B`. The executable can be
started under GDB from the Run and Debug view with `Debug light-engine`.
