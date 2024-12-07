#KNOWN ISSUE: If you can't build, run 'rm -rf build'.
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
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND TARGET server)
    message(FATAL_ERROR "Server compilation is not supported on iOS")
else()
    add_executable(server ${SERVER_SOURCES})
endif()

add_executable(client ${CLIENT_SOURCES})
add_executable(test_client ${TEST_CLIENT_SOURCES})

# Handle Emscripten-specific flags
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(client PRIVATE __MOBILE__)
    target_sources(client PRIVATE KeyboardHelper.m)
    target_link_libraries(client PRIVATE "-framework UIKit")
    target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs)
    target_link_libraries(client PRIVATE "-framework Foundation")
    target_link_libraries(client PRIVATE "-framework OpenGLES")
    target_link_libraries(client PRIVATE "-framework CoreGraphics")
    target_link_libraries(client PRIVATE "-framework QuartzCore")
    target_link_libraries(client PRIVATE "-framework UIKit")
    target_link_libraries(client PRIVATE "-framework CoreText")
    target_link_libraries(client PRIVATE "-framework CoreFoundation")
    target_link_libraries(client PRIVATE "-framework AudioToolbox")
    target_link_libraries(client PRIVATE "-framework AVFoundation")
    target_link_libraries(client PRIVATE "-framework CoreAudio")
    target_link_libraries(client PRIVATE "-framework CoreMIDI")
    target_link_libraries(client PRIVATE "-framework CoreMedia")
    target_link_libraries(client PRIVATE "-framework CoreVideo")
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
)

# Link libraries for client
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