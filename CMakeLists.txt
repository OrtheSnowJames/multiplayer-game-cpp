#KNOWN ISSUE: If you can't build, run 'rm -rf build'.
cmake_policy(SET CMP0167 NEW)
cmake_minimum_required(VERSION 3.15)
project(multiplayer_game_cpp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set cmake dir
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")


# Find required packages
find_package(Boost REQUIRED COMPONENTS system thread filesystem regex chrono date_time)
find_package(nlohmann_json REQUIRED)
find_package(raylib REQUIRED)
find_package(GTest REQUIRED)

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

set(TEST_CLIENT_SOURCES
    test_client.cpp
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
    add_executable(test_client ${TEST_CLIENT_SOURCES})
endif()

# Handle Emscripten-specific flags
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# Modify library linking section to be platform-aware
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
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
    )

    # Link libraries for test_client
    target_link_libraries(test_client PRIVATE
        Boost::system
        Boost::thread
        Boost::filesystem
        Boost::regex
        Boost::chrono
        Boost::date_time
        nlohmann_json::nlohmann_json
        raylib
        GTest::GTest
        GTest::Main
    )
endif()

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
)

# Add system-specific libraries
if(UNIX)
    target_link_libraries(server PRIVATE GL m pthread dl rt X11)
    target_link_libraries(client PRIVATE GL m pthread dl rt X11)
    target_link_libraries(test_client PRIVATE GL m pthread dl rt X11)
endif()

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00) # Target Windows 10 or later for WinSock2
    set(PLATFORM_LIBS ws2_32)
    target_link_libraries(server PRIVATE ${PLATFORM_LIBS})
else()
    set(PLATFORM_LIBS pthread)
    target_link_libraries(server PRIVATE ${PLATFORM_LIBS})
endif()

# Include directories
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(test_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})