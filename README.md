# CPP Multiplayer game

A game in C Plus Plus made by me and @AndrewGalvez.

Credits to [raysan5/raylib](https://github.com/raysan5/raylib), [boostorg/boost(asio, system, chrono etc.)](https://github.com/boostorg/boost), [nlohmann/json](https://github.com/nlohmann/json) and [zaphyod/websocketpp](https://github.com/zaphoyd/websocketpp) for providing libraries

INSTALLATION:
Linux: Install raylib, boost, and nlohmann json on your respective package manager; executables are already in the repo

MacOS: Same thing as Linux except install cmake, delete build folder, and run `cmake ..`, `make -j4` and `sudo make install`.

Windows: This is a little bit complex. Install vcpkg and install raylib, boost and nlohmann json on it. Install Microsoft Visual Studio if you haven't already. run `.\vcpkg integrate install` Opening up this repo in VS should build properly. If not, find a guide on how to use vcpkg with cmake on Visual Studio. This should make an executable in the build folder. Move the repo folder to Program Files (if it's not already there) and create shortcut which you move to desktop. Good job installing.
(prebuilt binaries coming as soon as finished with project and cmake working)
[![REUSE status](https://api.reuse.software/badge/github.com/nlohmann/json)](https://api.reuse.software/info/github.com/nlohmann/json)
