cmake_minimum_required(VERSION 3.15)
project(multiplayer_game_cpp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# Determine the user's home directory based on platform
if(WIN32)
    # On Windows, use %USERPROFILE% as the user's home directory
    set(USER_HOME "$ENV{USERPROFILE}")
elseif(APPLE OR UNIX)
    # On macOS/Linux, use $HOME as the user's home directory
    set(USER_HOME "$ENV{HOME}")
else()
    message(FATAL_ERROR "Unsupported platform.")
endif()

# Define the expected vcpkg path in the user's home directory
set(VCPKG_DIR "${USER_HOME}/vcpkg")

# Check if the vcpkg toolchain exists in the user's home directory
if(EXISTS "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")
    # Set the CMake toolchain to vcpkg
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "Path to the CMake toolchain file for vcpkg")
    message(STATUS "Using vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
else()
    message(FATAL_ERROR "vcpkg toolchain not found in the user's home directory: ${VCPKG_DIR}")
endif()

# Find required packages
find_package(Boost REQUIRED COMPONENTS system thread filesystem regex chrono date_time)
find_package(nlohmann_json REQUIRED)
find_package(raylib REQUIRED)
find_package(glfw3 REQUIRED) # Explicitly find GLFW

# Add source files
set(SERVER_SOURCES
    server.cpp
    coolfunctions.hpp
    libs/pathfinding.hpp
)

set(CLIENT_SOURCES
    client.cpp
    coolfunctions.hpp
)

# Create executables
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Only build client for iOS
    add_executable(client ${CLIENT_SOURCES})
    
    # iOS-specific settings
    set_target_properties(client PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourdomain.client"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        XCODE_ATTRIBUTE_ENABLE_BITCODE NO
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH YES
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"   # iPhone and iPad
        XCODE_ATTRIBUTE_VALID_ARCHS "arm64"
    )
    
    # iOS-specific definitions and libraries
    target_compile_definitions(client PRIVATE __MOBILE__)
    target_sources(client PRIVATE KeyboardHelper.m)
    
    # iOS frameworks
    target_link_libraries(client PRIVATE
        "-framework UIKit"
        "-framework Foundation"
        "-framework OpenGLES"
        "-framework CoreGraphics"
        "-framework QuartzCore"
        "-framework CoreText"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework AVFoundation"
        "-framework CoreAudio"
        "-framework CoreMIDI"
        "-framework CoreMedia"
        "-framework CoreVideo"
    )
else()
    # Build all executables for non-iOS platforms
    add_executable(server ${SERVER_SOURCES})
    add_executable(client ${CLIENT_SOURCES})
endif()

# Handle Emscripten-specific flags
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# Link libraries for server
target_link_libraries(server PRIVATE
    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::regex
    Boost::chrono
    Boost::date_time
    nlohmann_json::nlohmann_json
    raylib
    glfw  # Link GLFW explicitly
)

# Link libraries for client (all platforms)
target_link_libraries(client PRIVATE
    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::regex
    Boost::chrono
    Boost::date_time
    nlohmann_json::nlohmann_json
    raylib
    glfw  # Link GLFW explicitly
)

# Add system-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(server PRIVATE GL m pthread dl rt X11)
    target_link_libraries(client PRIVATE GL m pthread dl rt X11)
elseif(APPLE)
    target_link_libraries(server PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    target_link_libraries(client PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
elseif(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00) # Target Windows 10 or later for WinSock2
    set(PLATFORM_LIBS ws2_32)
    target_link_libraries(server PRIVATE ${PLATFORM_LIBS})
    target_link_libraries(client PRIVATE ${PLATFORM_LIBS})
endif()

# Include directories
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
