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
set(LIGHTENGINE_DEFAULT_ARTNET_HOST "192.168.137.2" CACHE STRING "Default ArtNet target for local run/debug targets")
set(LIGHTENGINE_DEFAULT_ARTNET_UNIVERSE "0" CACHE STRING "Default ArtNet universe for local run/debug targets")

include(GNUInstallDirs)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(lightengine_core STATIC)
add_library(lightengine::core ALIAS lightengine_core)

target_sources(
    lightengine_core
    PRIVATE
        src/artnet_sender.cpp
        src/beat_clock.cpp
        src/control_command.cpp
        src/fixture_runtime.cpp
        src/music_dynamics.cpp
        src/motion_scene.cpp
        src/rgb_scene.cpp
        src/show_layers.cpp
        src/simple_engine.cpp
        src/os2l_event.cpp
        src/os2l_receiver.cpp
        src/project.cpp
        src/rgb_par_scene.cpp
        src/web_server.cpp
        include/lightengine/artnet_sender.hpp
        include/lightengine/beat_clock.hpp
        include/lightengine/color.hpp
        include/lightengine/control_command.hpp
        include/lightengine/dmx.hpp
        include/lightengine/engine_input.hpp
        include/lightengine/fixture_runtime.hpp
        include/lightengine/music_dynamics.hpp
        include/lightengine/motion_scene.hpp
        include/lightengine/show_layers.hpp
        include/lightengine/simple_engine.hpp
        include/lightengine/os2l_event.hpp
        include/lightengine/os2l_receiver.hpp
        include/lightengine/project.hpp
        include/lightengine/rgb_par_scene.hpp
        include/lightengine/rgb_scene.hpp
        include/lightengine/version.hpp
        include/lightengine/web_server.hpp
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

add_custom_target(
    run-cpp
    COMMAND "$<TARGET_FILE:light-engine>"
    DEPENDS light-engine
    USES_TERMINAL
    COMMENT "Running the C++ light-engine executable"
)

add_custom_target(
    listen-os2l
    COMMAND "$<TARGET_FILE:light-engine>" --listen-os2l 0.0.0.0 9996
    DEPENDS light-engine
    USES_TERMINAL
    COMMENT "Listening for VirtualDJ OS2L TCP events on 0.0.0.0:9996"
)

add_custom_target(
    serve-web-cpp
    COMMAND "$<TARGET_FILE:light-engine>" --serve-web 127.0.0.1 8088 "${CMAKE_CURRENT_SOURCE_DIR}/web"
    DEPENDS light-engine
    USES_TERMINAL
    COMMENT "Serving the C++ web UI stub on http://127.0.0.1:8088"
)

add_custom_target(
    run-simple-engine
    COMMAND
        "$<TARGET_FILE:light-engine>"
        --run-simple-engine
        127.0.0.1
        8088
        "${CMAKE_CURRENT_SOURCE_DIR}/web"
        0.0.0.0
        9996
        "${LIGHTENGINE_DEFAULT_ARTNET_HOST}"
        "${LIGHTENGINE_DEFAULT_ARTNET_UNIVERSE}"
    DEPENDS light-engine
    USES_TERMINAL
    COMMENT "Running the simple C++ light engine on http://127.0.0.1:8088"
)

add_custom_target(
    run-python-web-reference
    COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/light_engine.py"
        --web-host 127.0.0.1
        --web-port 8088
        --os2l-host 0.0.0.0
        --os2l-port 9996
        --artnet-host 127.0.0.1
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
    COMMENT "Running the existing Python web UI reference on http://127.0.0.1:8088"
)

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
