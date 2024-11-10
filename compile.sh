#!/bin/bash

CXX=g++
CXXFLAGS="-g -Wall -std=c++17 -I/usr/include/nlohmann -I./include -I/usr/local/include"

BOOST_LIBS="-lboost_system -lboost_thread -lboost_filesystem -lboost_regex -lboost_chrono -lboost_date_time"

# Make sure you put Correct Raylib paths
RAYLIB_INCLUDE="-I/usr/local/include"
RAYLIB_LIB="-L/usr/local/lib"
RAYLIB_STATIC_LIB="/usr/local/lib/libraylib.a"
RAYLIB_LIBS="${RAYLIB_STATIC_LIB} -lGL -lm -lpthread -ldl -lrt -lX11"

${CXX} ${CXXFLAGS} ${RAYLIB_INCLUDE} server.cpp -o server ${BOOST_LIBS}
${CXX} ${CXXFLAGS} ${RAYLIB_INCLUDE} client.cpp -o client ${BOOST_LIBS} ${RAYLIB_LIBS}