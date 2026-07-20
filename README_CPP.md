# C++ Light Engine

This directory layout is the starting point for the C++ migration. The current
Python implementation remains available as a behavioral reference.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler (GCC 10+, Clang 12+, or a recent MSVC)
- VS Code extensions recommended by `.vscode/extensions.json`

On Alpine Linux:

```sh
apk add build-base cmake gdb
```

## Build and test

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/light-engine
```

For an optimized build:

```sh
cmake --preset release
cmake --build --preset release
```

## VS Code

Open the repository folder, install the recommended extensions, and select the
`debug` CMake preset. CMake Tools generates `compile_commands.json`, which is
used for IntelliSense, navigation, diagnostics, and refactoring.

The default build task is available with `Ctrl+Shift+B`. The executable can be
started under GDB from the Run and Debug view with `Debug light-engine`.
