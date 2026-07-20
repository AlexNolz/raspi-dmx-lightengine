set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv6)

set(_raspi_c_compiler "$ENV{RASPI_CC}")
set(_raspi_cxx_compiler "$ENV{RASPI_CXX}")
set(_raspi_sysroot "$ENV{RASPI_SYSROOT}")

if(_raspi_c_compiler)
    set(CMAKE_C_COMPILER "${_raspi_c_compiler}" CACHE FILEPATH "Raspberry Pi C compiler")
endif()

if(_raspi_cxx_compiler)
    set(CMAKE_CXX_COMPILER "${_raspi_cxx_compiler}" CACHE FILEPATH "Raspberry Pi C++ compiler")
else()
    set(CMAKE_CXX_COMPILER arm-linux-musleabihf-g++ CACHE FILEPATH "Raspberry Pi C++ compiler")
endif()

if(_raspi_sysroot)
    set(CMAKE_SYSROOT "${_raspi_sysroot}" CACHE PATH "Raspberry Pi sysroot")
    set(CMAKE_FIND_ROOT_PATH "${_raspi_sysroot}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

add_compile_options(
    -mcpu=arm1176jzf-s
    -mfpu=vfp
    -mfloat-abi=hard
)
