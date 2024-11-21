#!/bin/bash

# Define source files
SERVER_SRC="server.cpp"
CLIENT_SRC="client.cpp"

# Define output binaries
SERVER_BIN="server"
CLIENT_BIN="client"

#Define Optimization level
OPTIMIZATION_LEVEL="-O0"

# Define include directories
INCLUDE_DIRS="-I/usr/local/include -I/usr/include/raylib -I/usr/include/nlohmann"

# Define libraries to link against
LIBS="-lboost_system -lraylib -lpthread"
read -p "Do you want to have advanced information? (y/n) " advancedInfo

# Compile server
if [ ${advancedInfo} == "y" ]; then
    clang++ -c  "${SERVER_SRC}" ${INCLUDE_DIRS} ${OPTIMIZATION_LEVEL} -o server.o -g
    clang++ -c  "${CLIENT_SRC}" ${INCLUDE_DIRS} ${OPTIMIZATION_LEVEL} -o client.o -g
else
    clang++ -c  "${SERVER_SRC}" ${INCLUDE_DIRS} ${OPTIMIZATION_LEVEL} -o server.o
    clang++ -c  "${CLIENT_SRC}" ${INCLUDE_DIRS} ${OPTIMIZATION_LEVEL} -o client.o

fi

# Link server
clang++ server.o ${LIBS} -o "${SERVER_BIN}"

# Link client
clang++ client.o ${LIBS} -o "${CLIENT_BIN}"

echo "Compilation finished"