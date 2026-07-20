cmake_minimum_required(VERSION 3.20)

project(
    raspi_dmx_lightengine
    VERSION 0.1.0
    DESCRIPTION "Raspberry Pi DMX light engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LIGHTENGINE_BUILD_TESTS "Build the light engine tests" ON)
option(LIGHTENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(LIGHTENGINE_RASPI_DEPLOY_HOST "root@raspi-dmx" CACHE STRING "SSH target used by the deploy-raspi target")
set(LIGHTENGINE_RASPI_DEPLOY_DIR "/tmp/light-engine-deploy" CACHE STRING "Temporary directory on the Raspberry Pi")

include(GNUInstallDirs)

add_library(lightengine_core STATIC)
add_library(lightengine::core ALIAS lightengine_core)

target_sources(
    lightengine_core
    PRIVATE
        src/artnet_sender.cpp
        src/control_command.cpp
        src/os2l_event.cpp
        src/os2l_receiver.cpp
        src/project.cpp
        include/lightengine/artnet_sender.hpp
        include/lightengine/control_command.hpp
        include/lightengine/dmx.hpp
        include/lightengine/engine_input.hpp
        include/lightengine/os2l_event.hpp
        include/lightengine/os2l_receiver.hpp
        include/lightengine/project.hpp
        include/lightengine/version.hpp
)

target_compile_features(lightengine_core PUBLIC cxx_std_20)
target_include_directories(
    lightengine_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if(MSVC)
    target_compile_options(lightengine_core PRIVATE /W4 /permissive-)
    if(LIGHTENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(lightengine_core PRIVATE /WX)
    endif()
else()
    target_compile_options(
        lightengine_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
    )
    if(LIGHTENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(lightengine_core PRIVATE -Werror)
    endif()
endif()

add_executable(light-engine app/main.cpp)
target_link_libraries(light-engine PRIVATE lightengine::core)

install(TARGETS light-engine RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

add_custom_target(
    deploy-raspi
    COMMAND
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/deploy_raspi.sh"
        "$<TARGET_FILE:light-engine>"
        "${LIGHTENGINE_RASPI_DEPLOY_HOST}"
        "${LIGHTENGINE_RASPI_DEPLOY_DIR}"
    DEPENDS light-engine
    USES_TERMINAL
    COMMENT "Deploying light-engine to ${LIGHTENGINE_RASPI_DEPLOY_HOST}"
)

if(LIGHTENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
