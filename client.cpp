#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include "coolfunctions.hpp"
#include "raylib.h"
using namespace std;
using namespace boost::asio;
using ip::tcp;
using json = nlohmann::json;

bool checkCollision(const json& object1, const json& object2) {
    if (!object1.contains("x") || !object1.contains("y") || 
        !object2.contains("x") || !object2.contains("y") ||
        !object1.contains("width") || !object1.contains("height") ||
        !object2.contains("width") || !object2.contains("height")) {
        return false;
    }

    int left1 = object1["x"].get<int>();
    int right1 = left1 + object1["width"].get<int>();
    int top1 = object1["y"].get<int>();
    int bottom1 = top1 + object1["height"].get<int>();

    int left2 = object2["x"].get<int>();
    int right2 = left2 + object2["width"].get<int>();
    int top2 = object2["y"].get<int>();
    int bottom2 = top2 + object2["height"].get<int>();

    return !(left1 > right2 || right1 < left2 || top1 > bottom2 || bottom1 < top2);
}

void logToFile(const std::string& message, const std::string& filename = "err.log", bool append = true) {
    try {
        std::ofstream logFile(filename, std::ios::app);
        if (append) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            logFile << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") 
                    << " - " << message << std::endl;
        } else {
            logFile << message << std::endl;
        }
        logFile.close();
    }
    catch(const std::exception& e) {
        std::cerr << "Logging failed: " << e.what() << std::endl;
    }
}

void restartApplication(int & WindowsOpenInt) {
    for (int i = 0; i < WindowsOpenInt; i++){
        CloseWindow();
    }
    
    // Execute the run script again
    std::string command = "sudo bash ./run.sh";
    
    // Use exec to replace current process
    if (execl("/bin/bash", "bash", "./run.sh", nullptr) == -1) {
        std::cerr << "Failed to restart: " << strerror(errno) << std::endl;
        logToFile("Failed to restart: " + std::string(strerror(errno)), "err.log", true);
        exit(1);
    }
}

int main() {
    int WindowsOpen = 0;
    int screenWidth = getEnvVar<int>("SCREEN_WIDTH", 800);
    int screenHeight = getEnvVar<int>("SCREEN_HEIGHT", 450);
    int fps = getEnvVar<int>("FPS", 60);
    int port = getEnvVar<int>("PORT", 1234);
    std::string ip = getEnvVar<std::string>("IP", "127.0.0.1");
    std::string LocalName = getEnvVar<std::string>("NAME", "Player");
    std::cout << "Starting game with width = " << screenWidth << "height = " << screenHeight << " fps = " << fps << " FPS" << std::endl;

    if (fps > 99) fps = 99;

    // Initialize window
    InitWindow(screenWidth, screenHeight, "Game");
    WindowsOpen = WindowsOpen + 1;
    SetTargetFPS(fps);

    // Load player texture
    Texture2D playerTexture = LoadTexture("/home/james/Documents/vSCProjects/multiplayer-game-cpp/player.png");
    
    Image playerImage = LoadImageFromTexture(playerTexture);
    Image croppedImage1 = ImageFromImage(playerImage, (Rectangle){0, 0, playerTexture.width/2, playerTexture.height/2});
    Texture2D player1 = LoadTextureFromImage(croppedImage1);
    Image croppedImage2 = ImageFromImage(playerImage, (Rectangle){playerTexture.width/2, 0, playerTexture.width/2, playerTexture.height/2});
    Texture2D player2 = LoadTextureFromImage(croppedImage2);
    Image croppedImage3 = ImageFromImage(playerImage, (Rectangle){0, playerTexture.height/2, playerTexture.width/2, playerTexture.height/2});
    Texture2D player3 = LoadTextureFromImage(croppedImage3);
    Image croppedImage4 = ImageFromImage(playerImage, (Rectangle){playerTexture.width/2, playerTexture.height/2, playerTexture.width/2, playerTexture.height/2});
    Texture2D player4 = LoadTextureFromImage(croppedImage4);

    std::map<std::string, Texture2D> spriteSheet;
    spriteSheet["1"] = player1; spriteSheet["2"] = player2; spriteSheet["3"] = player3; spriteSheet["4"] = player4;
    spriteSheet["W"] = player1; spriteSheet["A"] = player4; spriteSheet["S"] = player3; spriteSheet["D"] = player2;
    UnloadImage(playerImage);
    UnloadImage(croppedImage1);
    UnloadImage(croppedImage2);
    UnloadImage(croppedImage3);
    UnloadImage(croppedImage4);

    json game = {
        {"room1", {
            {"players", {}},
            {"objects", {}},
            {"enemies", {}}
        }}
    };
    json localPlayer;
    json keys = {
        {"keymap", {}},  // Store key mappings
        {"state", {}}    // Store key states
    };
    bool gameRunning = true;
    
    try {
        io_context io_context;
        tcp::socket socket(io_context);
        tcp::endpoint endpoint(ip::address::from_string(ip), port);
        socket.connect(endpoint);
        std::cout << "Connected to server" << std::endl;

        boost::asio::streambuf buffer;
        bool initGame = false;
        bool initGameFully = false;

        // Main Game Loop
        while (!WindowShouldClose()) {
            std::cout << "1" << std::endl;
            // Handle key presses
            int keyCode = GetKeyPressed();
            if (keyCode != 0) {
                // Store both representations
                std::string keyName = TextFormat("%d", keyCode);
                std::string keyChar = (keyCode >= 32 && keyCode <= 126) ? 
                    std::string(1, static_cast<char>(keyCode)) : "";
                
                // Store mappings
                keys["keymap"][keyName] = keyCode;
                if (!keyChar.empty()) {
                    keys["keymap"][keyChar] = keyCode;
                }
                
                // Set state
                keys["state"][keyName] = true;
                if (!keyChar.empty()) {
                    keys["state"][keyChar] = true;
                }
            }
            
            // Check key releases
            for (auto& k : keys["state"].items()) {
                if (k.value().get<bool>()) {
                    int keyCode = keys["keymap"][k.key()].get<int>();
                    if (!IsKeyDown(keyCode)) {
                        keys["state"][k.key()] = false;
                    }
                }
            }
            std::cout << "2" << std::endl;
            std::string CurrentWindow = "game";
            if (!initGame) {
                json newMessage = {
                    {"requestGame", true},
                    {"currentGame", "game1"},
                    {"currentPlayer", LocalName}
                };
                boost::asio::write(socket, boost::asio::buffer(newMessage.dump() + "\n"));
                initGame = true;
            }

            if (IsWindowResized()) SetWindowSize(screenWidth, screenHeight);
                        std::cout << "3" << std::endl;
            std::string message;
            std::cout << "4" << std::endl;
            boost::system::error_code error;
            std::cout << "5" << std::endl;
            boost::asio::read_until(socket, buffer, "\n", error);
            std::cout << "6" << std::endl;
            if (!error) {
                std::istream input_stream(&buffer);
                std::getline(input_stream, message);
                json messageJson = json::parse(message);

                if (messageJson.contains("quitGame") && messageJson["quitGame"].get<bool>()) {
                    if (messageJson.contains("socket")){
                        for (auto it = game["room1"]["players"].begin(); it != game["room1"]["players"].end(); ++it){
                            if ((*it)["socket"] == messageJson["socket"]){
                                game["room1"]["players"].erase(it);
                                break;
                            }
                        }
                    }
                    if (messageJson["socket"] == socket.native_handle()){gameRunning = false; CloseWindow(); break;}
                }
                if (messageJson.contains("getGame")) {
                    game = messageJson["getGame"];
                    initGameFully = true;
                }
                if (messageJson.contains("name") && messageJson["local"].get<bool>()) {
                    localPlayer = messageJson;
                }
            }

            BeginDrawing();
            ClearBackground(RAYWHITE);

            // Game Input Processing
            bool goingLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
            bool goingRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
            bool goingUp = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
            bool goingDown = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
            std::cout << "4" << std::endl;

            if (localPlayer.contains("y") && localPlayer.contains("speed")) {
                if (goingUp) localPlayer["y"] = localPlayer["y"].get<int>() - localPlayer["speed"].get<int>();
                if (goingDown) localPlayer["y"] = localPlayer["y"].get<int>() + localPlayer["speed"].get<int>();
            }

            if (localPlayer.contains("x") && localPlayer.contains("speed")) {
                if (goingLeft) localPlayer["x"] = localPlayer["x"].get<int>() - localPlayer["speed"].get<int>();
                if (goingRight) localPlayer["x"] = localPlayer["x"].get<int>() + localPlayer["speed"].get<int>();
            }

            //init ui
            json fpsField = {{"x", 20}, {"y", 20}, {"width", 100}, {"height", 20}, {"text", "FPS: " + std::to_string(fps)}, {"editable", false}};
            json ipField = {{"x", 20}, {"y", 60}, {"width", 100}, {"height", 20}, {"text", "IP: " + ip}};
            json portField = {{"x", 20}, {"y", 100}, {"width", 100}, {"height", 20}, {"text", "Port: " + std::to_string(port)}, {"editable", false}};
            json nameField = {{"x", 20}, {"y", 140}, {"width", 100}, {"height", 20}, {"text", "Name: " + LocalName}, {"editable", false}};
            json screenWidthField = {{"x", 20}, {"y", 180}, {"width", 100}, {"height", 20}, {"text", "Screen Width: " + std::to_string(screenWidth)}, {"editable", false}};
            json screenHeightField = {{"x", 20}, {"y", 200}, {"width", 100}, {"height", 20}, {"text", "Screen Height: " + std::to_string(screenHeight)}, {"editable", false}};
            // Draw Game Elements
            if (CurrentWindow == "game"){
                //draw game
                //draw players

                for (auto& p : game[localPlayer["room"]]["players"]) {
                    DrawText(p["name"].get<std::string>().c_str(), p["x"].get<int>() + 10, p["y"].get<int>(), 20, BLACK);
                    //sprite state uses cardinal directions in order: n, e, s, w
                    DrawTexture(spriteSheet[p["spriteState"].get<std::string>()], p["x"].get<int>(), p["y"].get<int>(), WHITE);
                    DrawRectangle(p["x"].get<int>(), p["y"].get<int>() + 20, 20, 20, RED);
                }
                for (auto& p : game[localPlayer["room"]]["enemies"]) {
                    DrawRectangle(p["x"].get<int>(), p["y"].get<int>(), 20, 20, BLUE);
                }
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    json mouseRect = {{"x", static_cast<int>(mousePos.x)}, {"y", static_cast<int>(mousePos.y)}, {"width", 1}, {"height", 1}};
                    json settingsButton = {{"x", 10}, {"y", 10}, {"width", 30}, {"height", 20}};
                    if (checkCollision(mouseRect, settingsButton)) {
                        CurrentWindow = "settings";
                    }
                }
                DrawText("Settings", 15, 15, 20, WHITE);
                DrawRectangle(10, 10, 30, 20, BLACK);
                std::cout << "passs" << std::endl;
            }
            else if (CurrentWindow == "settings"){
                ClearBackground(GRAY);
                DrawText("Settings:", 10, 0, 10, WHITE);

                DrawText("FPS: ", 10, 10, 10, BLACK);
                DrawRectangle(fpsField["x"].get<int>(), fpsField["y"].get<int>(), fpsField["width"].get<int>(), fpsField["height"].get<int>(), BLACK);
                DrawText(fpsField["text"].get<std::string>().c_str(), fpsField["x"].get<int>(), fpsField["y"].get<int>(), 20, WHITE);

                DrawText("IP: ", 10, 40, 20, BLACK);
                DrawRectangle(ipField["x"].get<int>(), ipField["y"].get<int>(), ipField["width"].get<int>(), ipField["height"].get<int>(), BLACK);
                DrawText(ipField["text"].get<std::string>().c_str(), ipField["x"].get<int>(), ipField["y"].get<int>(), 20, WHITE);

                DrawText("Port: ", 10, 80, 20, BLACK);
                DrawRectangle(portField["x"].get<int>(), portField["y"].get<int>(), portField["width"].get<int>(), portField["height"].get<int>(), BLACK);
                DrawText(portField["text"].get<std::string>().c_str(), portField["x"].get<int>(), portField["y"].get<int>(), 20, WHITE);

                DrawText("Name: ", 10, 120, 20, BLACK);
                DrawRectangle(nameField["x"].get<int>(), nameField["y"].get<int>(), nameField["width"].get<int>(), nameField["height"].get<int>(), BLACK);
                DrawText(nameField["text"].get<std::string>().c_str(), nameField["x"].get<int>(), nameField["y"].get<int>(), 20, WHITE);

                DrawText("Screen Width: ", 10, 160, 20, BLACK);
                DrawRectangle(screenWidthField["x"].get<int>(), screenWidthField["y"].get<int>(), screenWidthField["width"].get<int>(), screenWidthField["height"].get<int>(), BLACK);
                DrawText(screenWidthField["text"].get<std::string>().c_str(), screenWidthField["x"].get<int>(), screenWidthField["y"].get<int>(), 20, WHITE);

                DrawText("Screen Height: ", 10, 200, 20, BLACK);
                DrawRectangle(screenHeightField["x"].get<int>(), screenHeightField["y"].get<int>(), screenHeightField["width"].get<int>(), screenHeightField["height"].get<int>(), BLACK);
                DrawText(screenHeightField["text"].get<std::string>().c_str(), screenHeightField["x"].get<int>(), screenHeightField["y"].get<int>(), 20, WHITE);

                DrawText("Press Right Mouse Button To Stop Editing", 10, 240, 20, BLACK);
                DrawText("Press Enter To Save Changes", 10, 260, 20, BLACK);
                DrawText("Press Escape To Close", 10, 280, 20, BLACK);

                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)){
                    fpsField["editable"] = false;
                    nameField["editable"] = false;
                    screenWidthField["editable"] = false;
                    screenHeightField["editable"] = false;
                    portField["editable"] = false;
                    ipField["editable"] = false;
                    fps = std::stoi(fpsField["text"].get<std::string>().substr(5));
                    screenWidth = std::stoi(screenWidthField["text"].get<std::string>().substr(14));
                    screenHeight = std::stoi(screenHeightField["text"].get<std::string>().substr(15));
                    port = std::stoi(portField["text"].get<std::string>().substr(6));
                    ip = ipField["text"].get<std::string>().substr(4);
                    LocalName = nameField["text"].get<std::string>().substr(6);
                    std::string whatToWrite = "export FPS=" + std::to_string(fps) + "\n" + 
                                            "export SCREEN_WIDTH=" + std::to_string(screenWidth) + "\n" + 
                                            "export SCREEN_HEIGHT=" + std::to_string(screenHeight) + "\n" + 
                                            "export PORT=" + std::to_string(port) + "\n" + 
                                            "export IP=" + ip + "\n" + 
                                            "export NAME=" + LocalName + "\n";
                    std::ofstream envFile("environmentVars.sh");
                    if (envFile.is_open()) {
                        envFile << whatToWrite;
                        envFile.close();
                    } else {
                        logToFile("Failed to open environmentVars.sh for writing", "err.log", true);
                    }
                    InitWindow(screenWidth, screenHeight, "restarting");
                    WindowsOpen = WindowsOpen + 1; 
                    BeginDrawing(); 
                    ClearBackground(RAYWHITE); 
                    DrawText("Restarting...", 0, 0, 50, BLACK); 
                    EndDrawing();
                    
                }
                else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    json mouseRect = {{"x", static_cast<int>(mousePos.x)}, {"y", static_cast<int>(mousePos.y)}, {"width", 1}, {"height", 1}};
                    if (checkCollision(mouseRect, fpsField)) { fpsField["editable"] = true; }
                    else if (checkCollision(mouseRect, nameField)) { nameField["editable"] = true; }
                    else if (checkCollision(mouseRect, screenWidthField)) { screenWidthField["editable"] = true; }
                    else if (checkCollision(mouseRect, screenHeightField)) { screenHeightField["editable"] = true; }
                    else if (checkCollision(mouseRect, portField)) { portField["editable"] = true; }
                    else if (checkCollision(mouseRect, ipField)) { ipField["editable"] = true; }
                }
                if (fpsField["editable"] && GetKeyPressed() != 0) { fpsField["text"] = "FPS: " + std::to_string(GetKeyPressed()); }
                if (nameField["editable"] && GetKeyPressed() != 0) { nameField["text"] = "Name: " + std::string(1, static_cast<char>(GetKeyPressed())); }
                if (screenWidthField["editable"] && GetKeyPressed() != 0) { screenWidthField["text"] = "Screen Width: " + std::to_string(GetKeyPressed()); }
                if (screenHeightField["editable"] && GetKeyPressed() != 0) { screenHeightField["text"] = "Screen Height: " + std::to_string(GetKeyPressed()); }
                if (portField["editable"] && GetKeyPressed() != 0) { portField["text"] = "Port: " + std::to_string(GetKeyPressed()); }
                if (ipField["editable"] && GetKeyPressed() != 0) { ipField["text"] = "IP: " + std::string(1, static_cast<char>(GetKeyPressed())); }
            }
            EndDrawing();
        }
        gameRunning = false;
        json quitMessage = {{"quitGame", true}};
        boost::asio::write(socket, boost::asio::buffer(quitMessage.dump() + "\n"));
        socket.close();
    } catch (const std::exception& e) {
        logToFile(std::string("ERROR: ") + e.what());
        std::cerr << "Exception: " << e.what() << std::endl;
        CloseWindow();
        WindowsOpen = WindowsOpen - 1;
        return -1;
    }

    // Properly unload textures
    UnloadTexture(playerTexture);
    UnloadTexture(player1);
    UnloadTexture(player2);
    UnloadTexture(player3);
    UnloadTexture(player4);

    CloseWindow();
    WindowsOpen = WindowsOpen - 1;
    return 0;
}
