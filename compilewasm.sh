#!/bin/bash

# Setup Emscripten
cd ~/repos/emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Change to project directory
cd /home/james/Documents/GitHub/multiplayer-game-cpp

# Create web directory
mkdir -p web

# Set up nlohmann-json for Emscripten properly
EMSDK_INCLUDE="/home/james/repos/emsdk/upstream/emscripten/cache/sysroot/include"
rm -rf "${EMSDK_INCLUDE}/nlohmann"
git clone --depth 1 https://github.com/nlohmann/json.git temp_json
mkdir -p "${EMSDK_INCLUDE}/nlohmann"
cp temp_json/single_include/nlohmann/json.hpp "${EMSDK_INCLUDE}/nlohmann/"
rm -rf temp_json

# Verify json.hpp exists
if [ ! -f "${EMSDK_INCLUDE}/nlohmann/json.hpp" ]; then
    echo "Error: json.hpp not found"
    exit 1
fi

# Compile WebClient.cpp to WebAssembly
emcc webclient.cpp \
    -o web/game.html \
    -s WEBSOCKET_URL="ws://localhost:5767" \
    -s ASYNCIFY=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s WASM=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s USE_GLFW=3 \
    -s "EXPORTED_RUNTIME_METHODS=['ccall']" \
    --preload-file assets@/assets \
    -I "${EMSDK_INCLUDE}" \
    -I /usr/local/include/raylib \
    -l raylib \
    -O2 \
    --shell-file shell.html

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
else
    echo "Compilation failed!"
    exit 1
fi