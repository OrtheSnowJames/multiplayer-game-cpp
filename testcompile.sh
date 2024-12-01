#!/bin/bash

# Path to nlohmann json library
JSON_LIB_PATH="/usr/include/nlohmann"

# Compile the test.cpp file using clang and include the nlohmann json library
clang++ -std=c++17 -I${JSON_LIB_PATH} -o test test.cpp

# Check if the compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful. Executable created: ./test"
else
    echo "Compilation failed."
fi