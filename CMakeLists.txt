cmake_minimum_required(VERSION 3.10)
project(multiplayer-game-cpp)

find_package(Boost COMPONENTS system asio REQUIRED)

add_executable(server server.cpp)
add_executable(client client.cpp)
target_link_libraries(server Boost::System Boost::Asio)