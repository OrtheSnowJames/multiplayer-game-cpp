#!/bin/bash

# Define source files
SERVER_SRC="server.cpp"
CLIENT_SRC="client.cpp"

# Define output binaries
SERVER_BIN="server"
CLIENT_BIN="client"

# Define include directories
INCLUDE_DIRS="-I/usr/local/include -I/usr/include/raylib -I/usr/include/nlohmann"

# Define libraries to link against
LIBS="-lboost_system -lraylib -lpthread"

# Compile server
clang++ -c "${SERVER_SRC}" ${INCLUDE_DIRS} -o server.o

# Compile client
clang++ -c "${CLIENT_SRC}" ${INCLUDE_DIRS} -o client.o

# Link server
clang++ server.o ${LIBS} -o "${SERVER_BIN}"

# Link client
clang++ client.o ${LIBS} -o "${CLIENT_BIN}"

echo "Compilation finished!"