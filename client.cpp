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

#ifdef _WIN32
#include <winsock2.h>
using SocketHandle = SOCKET;
#else
using SocketHandle = int;
#endif
#if defined(__APPLE__) && defined(__MOBILE__)
#include "libs/KeyboardHelper.h"
#endif

using namespace std;
using namespace boost::asio;
using ip::tcp;
using json = nlohmann::json;
namespace fs = std::filesystem;
fs::path root = fs::current_path();

json game = {
    {"room1", {
        {"players", {}},
        {"objects", {}},
        {"enemies", {}}
    }}
};

json checklist = {
    {"goingup", false},
    {"goingleft", false},
    {"goingright", false},
    {"goingdown", false},
    {"quitGame", false},
    {"requestGame", false},
    {"x", 0},
    {"y", 0},
    {"currentGame", ""},
    {"currentPlayer", ""},
    {"spriteState", 1},
    {"room", 1},
    {"playerCount", 0}
};

json canMove = {{"w", true}, {"a", true}, {"s", true}, {"d", true}};

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

bool checkWallCollision(const json& object1, const json& object2, int& wall) {
    // Ensure all necessary properties are present
    if (!object1.contains("x") || !object1.contains("y") || 
        !object2.contains("x") || !object2.contains("y") ||
        !object1.contains("width") || !object1.contains("height") ||
        !object2.contains("width") || !object2.contains("height")) {
        return false;
    }

    // Get boundaries of object1
    int left1 = object1["x"].get<int>();
    int right1 = left1 + object1["width"].get<int>();
    int top1 = object1["y"].get<int>();
    int bottom1 = top1 + object1["height"].get<int>();

    // Get boundaries of object2 (the wall)
    int left2 = object2["x"].get<int>();
    int right2 = left2 + object2["width"].get<int>();
    int top2 = object2["y"].get<int>();
    int bottom2 = top2 + object2["height"].get<int>();

    // Check for overlap
    if (right1 <= left2 || left1 >= right2 || bottom1 <= top2 || top1 >= bottom2) {
        return false; // No collision
    }

    // Determine the side of the wall collided
    int overlapLeft = right1 - left2;  // Distance into the wall from the left
    int overlapRight = right2 - left1; // Distance into the wall from the right
    int overlapTop = bottom1 - top2;   // Distance into the wall from the top
    int overlapBottom = bottom2 - top1; // Distance into the wall from the bottom

    // Find the smallest overlap to determine the wall
    int minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});

    if (minOverlap == overlapLeft) {
        wall = 2; // Collided with left side of object2
    } else if (minOverlap == overlapRight) {
        wall = 4; // Collided with right side of object2
    } else if (minOverlap == overlapTop) {
        wall = 1; // Collided with top side of object2
    } else if (minOverlap == overlapBottom) {
        wall = 3; // Collided with bottom side of object2
    }

    // Adjust player position based on collision
    if (wall == 1) { // Top collision
        checklist["y"] = object2["y"].get<int>() - object1["height"].get<int>();
    } else if (wall == 3) { // Bottom collision
        checklist["y"] = object2["y"].get<int>() + object2["height"].get<int>();
    } else if (wall == 2) { // Left collision
        checklist["x"] = object2["x"].get<int>() - object1["width"].get<int>();
    } else if (wall == 4) { // Right collision
        checklist["x"] = object2["x"].get<int>() + object2["width"].get<int>();
    }

    return true; // Collision detected
}

// Personal Space Bubble Struct
struct personalSpaceBubble {
    //10 inch increments in all directions
    int x;
    int y;
    int width;
    int height;
    json playerslist;
    json construct_bubble() {
        return {
            {"x", x-10},
            {"y", y-10},
            {"width", width+10},
            {"height", height+10}
        };
    }
    json get(){ return construct_bubble(); }
    void set_bubble(int x_, int y_, int width_, int height_) {
        x = x_;
        y = y_;
        width = width_;
        height = height_;
        //you'll need to call constructBubble() to get the json object
    }
    void add_player(json player) {
        playerslist.push_back(player);
    }
    void clear_players() {
        playerslist.clear();
    }
    bool check_burst() {
        json bubbleJson = construct_bubble();
        for (auto& p : playerslist.items()) {
            if (checkCollision(bubbleJson, p)) {
                return true;
            }
        }
        return false;
    }
    bool check_specific_burst(json player) {
        json bubbleJson = construct_bubble();
        return checkCollision(bubbleJson, player);
    }
};
// Modify Button struct
struct Button {
    Rectangle bounds;
    const char* text;
    bool pressed;
    bool held;  // Add held state
};

// Update IsButtonPressed function
bool IsButtonPressed(Button& button, Vector2 mousePoint) {
    bool mouseOver = CheckCollisionPointRec(mousePoint, button.bounds);
    button.held = mouseOver && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    button.pressed = mouseOver && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    return button.held || button.pressed; // Return true for both initial press and hold
}

void DrawButton(const Button& button) {
    Color buttonColor = button.pressed ? DARKGRAY : LIGHTGRAY;
    DrawRectangleRec(button.bounds, buttonColor);
    DrawRectangleLinesEx(button.bounds, 2, BLACK);
    
    // Center text in button
    int textWidth = MeasureText(button.text, 30);
    float textX = button.bounds.x + (button.bounds.width - textWidth) / 2;
    float textY = button.bounds.y + (button.bounds.height - 30) / 2;
    
    DrawText(button.text, textX, textY, 30, BLACK);
}

int getSafeSpriteSate(const json& j, const std::string& key) {
    try {
        if (!j.contains(key)) {
            return 0;
        }
        if (j[key].is_number()) {
            return j[key].get<int>();
        }
        if (j[key].is_string()) {
            return std::stoi(j[key].get<std::string>());
        }
        return 0;
    } catch (...) {
        return 0;
    }
}

enum LogLevel { INFO, WARNING, ERROR };

void logToFile(const std::string& message, LogLevel level = ERROR, const std::string& filename = "err.log") {
    const std::map<LogLevel, std::string> levelStrings = {
        {INFO, "INFO"},
        {WARNING, "WARNING"},
        {ERROR, "ERROR"}
    };
    std::ofstream logFile(filename, std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        logFile << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                << " [" << levelStrings.at(level) << "] " << message << std::endl;
        logFile.close();
    }
}


std::map<std::string, bool> DetectKeyPress() {
    std::map<std::string, bool> keyStates;
    
    // Check for key states
    keyStates["w"] = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    keyStates["a"] = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT); 
    keyStates["s"] = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    keyStates["d"] = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    keyStates["q"] = IsKeyDown(KEY_Q);
    keyStates["shift"] = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    return keyStates;
}
std::string localRoomName;
Texture2D cropTextureFunc(Texture2D& sourceTexture, int x, int y, int width, int height) {
    Rectangle cropRect = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) };
    Image croppedImage = ImageFromImage(LoadImageFromTexture(sourceTexture), cropRect);
    if (croppedImage.data == nullptr) {
        throw std::runtime_error("Failed to crop texture");
    }
    Texture2D croppedTexture = LoadTextureFromImage(croppedImage);
    UnloadImage(croppedImage);
    return croppedTexture;
}


void restartApplication(int & WindowsOpenInt) {
    for (int i = 0; i < WindowsOpenInt; i++){
        CloseWindow();
    }
    
    // Execute the run script again
    std::string command = "bash ./runClient.sh";
    
    // Use exec to replace current process
    if (execl("/bin/bash", "bash", "./runClient.sh", nullptr) == -1) {
        std::cerr << "Failed to restart: " << strerror(errno) << std::endl;
        logToFile("Failed to restart: " + std::string(strerror(errno)), ERROR, "err.log");
        exit(1);
    }
}

json test = {
    {"thing", {
        {"name", "test"},
        {"x", 10},
        {"y", 10},
        {"width", 10},
        {"height", 10}
    }}
};

struct Position {
    float x;
    float y;
    float width;
    float height;
    Position(float x_ = 0, float y_ = 0, float width_ = 64, float height_ = 64)  // Always initialize with full size
        : x(x_), y(y_), width(width_), height(height_) {}
};

struct PlayerState {
    Position current;
    Position target;
    float interpolation = 0;
    int spriteState = 0;
    std::string name;
    int socketId;
    int room = 1;  // Add room tracking, default to room1
    
    void update(float dt) {
        if (interpolation < 1.0f) {
            interpolation += dt * 10.0f; // Adjust this multiplier to control smoothing speed
            if (interpolation > 1.0f) interpolation = 1.0f;
            
            current.x = current.x + (target.x - current.x) * interpolation;
            current.y = current.y + (target.y - current.y) * interpolation;
            current.width = current.width + (target.width - current.width) * interpolation;
            current.height = current.height + (target.height - current.height) * interpolation;
        }
    }
};

std::map<int, PlayerState> playerStates;

void handleRead(const boost::system::error_code& error, std::size_t bytes_transferred, 
                boost::asio::streambuf& buffer, json& localPlayer, bool& initGameFully, 
                bool& gameRunning, tcp::socket& socket, bool& localPlayerSet) {
    if (!error) {
        static std::string messageBuffer;
        std::string message;
        {
            std::istream input_stream(&buffer);
            std::string chunk;
            while (std::getline(input_stream, chunk)) {  // Read all available lines
                messageBuffer += chunk;
                
                // Try to find a complete JSON message
                size_t openBrace = messageBuffer.find('{');
                size_t closeBrace = messageBuffer.find_last_of('}');
                
                if (openBrace != std::string::npos && closeBrace != std::string::npos && openBrace < closeBrace) {
                    try {
                        message = messageBuffer.substr(openBrace, closeBrace - openBrace + 1);
                        json messageJson = json::parse(message); 
                        messageBuffer.erase(0, closeBrace + 1); // Remove processed message
                        
                        // Process valid JSON message
                        messageJson = json::parse(message);
                        std::cout << "Client received: " << message << std::endl;

                        if (messageJson.contains("local") && messageJson["local"].get<bool>()) {
                            try {
                                messageJson["spriteState"] = messageJson.value("spriteState", 1);
                                messageJson["x"] = messageJson.value("x", 0);
                                messageJson["y"] = messageJson.value("y", 0);
                                messageJson["room"] = messageJson.value("room", 1);
                                messageJson["width"] = messageJson.value("width", 32);
                                messageJson["height"] = messageJson.value("height", 32);

                                checklist["x"] = messageJson["x"].get<int>();
                                checklist["y"] = messageJson["y"].get<int>();
                                checklist["spriteState"] = messageJson["spriteState"].get<int>();
                                
                                std::string roomToAddTo = "room" + std::to_string(messageJson["room"].get<int>());
                                if (!game.contains(roomToAddTo)) {
                                    game[roomToAddTo] = {{"players", json::array()}};
                                }
                                game[roomToAddTo]["players"].push_back(messageJson);

                                localPlayer = messageJson;
                                localPlayerSet = true;
                                std::cout << "Local player set: " << localPlayer.dump() << std::endl;

                                if (!initGameFully) {
                                    json gameRequest = {{"requestGame", true}};
                                    boost::asio::write(socket, boost::asio::buffer(gameRequest.dump() + "\n"));
                                }
                            } catch(const std::exception& e) {
                                std::cerr << "Error processing local player: " << e.what() << std::endl;
                                logToFile("Local player processing error: " + std::string(e.what()), ERROR);
                            }
                        }

                        if (messageJson.contains("getGame")) {
                            game = messageJson["getGame"];
                            initGameFully = true;
                            std::cout << "Game state updated successfully" << std::endl;
                            for (auto& room : game.items()) {
                                for (auto& player : room.value()["players"]) {
                                    int socketId = player["socket"].get<int>();
                                    if (playerStates.find(socketId) == playerStates.end()) {
                                        playerStates[socketId] = PlayerState();
                                    }
                                    playerStates[socketId].target = Position(
                                        player["x"].get<float>(),
                                        player["y"].get<float>(),
                                        player.value("width", 32.0f),
                                        player.value("height", 32.0f)
                                    );
                                    playerStates[socketId].current = playerStates[socketId].target;
                                    playerStates[socketId].name = player["name"].get<std::string>();
                                    playerStates[socketId].socketId = socketId;
                                    playerStates[socketId].spriteState = player["spriteState"].get<int>();
                                    playerStates[socketId].room = player["room"].get<int>();  // Add room update
                                }
                            }
                        }
                        
                        if (messageJson.contains("getRoom")) {
                            std::string roomName = messageJson["room"].get<std::string>();
                            game[roomName] = messageJson["getRoom"];

                            if (localPlayer["socket"] == socket.native_handle()) {
                                localPlayer["room"] = std::stoi(roomName.substr(4));
                                localRoomName = roomName;

                                // Clear existing player states for the new room
                                playerStates.clear();

                                // Initialize player states for all players in the new room
                                for (const auto& player : game[roomName]["players"]) {
                                    int socketId = player["socket"].get<int>();
                                    playerStates[socketId] = PlayerState();
                                    playerStates[socketId].target = Position(
                                        player["x"].get<float>(),
                                        player["y"].get<float>(),
                                        64.0f,
                                        64.0f
                                    );
                                    playerStates[socketId].current = playerStates[socketId].target;
                                    playerStates[socketId].name = player["name"].get<std::string>();
                                    playerStates[socketId].socketId = socketId;
                                    playerStates[socketId].spriteState = player["spriteState"].get<int>();
                                    playerStates[socketId].room = player["room"].get<int>();
                                }

                                // Reset movement flags and ensure room has objects array
                                canMove = {{"w", true}, {"a", true}, {"s", true}, {"d", true}};
                                if (!game[roomName].contains("objects")) {
                                    game[roomName]["objects"] = json::array();
                                }
                                
                                std::cout << "Room transition complete. Movement enabled in " << roomName << std::endl;
                            }
                        }
                        if (messageJson.contains("quitGame")) {
                            gameRunning = false;
                            std::cout << "Game quit requested" << std::endl;
                        }
                        if (messageJson.contains("updatePosition")) {
                            auto& updateData = messageJson["updatePosition"];
                            int socketId = updateData["socket"].get<int>();

                            if (playerStates.find(socketId) == playerStates.end()) {
                                playerStates[socketId] = PlayerState();
                                playerStates[socketId].socketId = socketId;
                            }

                            // Keep dimensions constant - only scale in rendering
                            playerStates[socketId].target = Position(
                                updateData["x"].get<float>(),
                                updateData["y"].get<float>(),
                                64.0f,  // Keep constant size
                                64.0f   // Keep constant size
                            );
                            playerStates[socketId].interpolation = 0;

                            if (updateData.contains("spriteState")) {
                                playerStates[socketId].spriteState = updateData["spriteState"].get<int>();
                            }
                            if (updateData.contains("room")) {
                                playerStates[socketId].room = updateData["room"].get<int>();
                            }
                        }
                        break;  // Exit loop after processing valid message
                    } catch (const json::parse_error&) {
                        continue; // Keep reading if JSON is invalid
                    }
                }
            }
        }

        // Re-arm the read
        buffer.consume(buffer.size());
        boost::asio::async_read_until(socket, buffer, "\n",
            [&](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                handleRead(ec, bytes_transferred, buffer, localPlayer, initGameFully, 
                          gameRunning, socket, localPlayerSet);
            });
    }
}

void verifyImageFormat(const fs::path& imagePath) {
    std::cout << "Verifying image: " << imagePath.string() << std::endl;
    Image img = LoadImage(imagePath.string().c_str());
    if (img.data) {
        std::cout << "Image format: " << img.format << std::endl;
        std::cout << "Dimensions: " << img.width << "x" << img.height << std::endl;
        UnloadImage(img);
    } else {
        throw std::runtime_error("Failed to load image for verification");
    }
}

void debugImagePath(const fs::path& path, const std::string& imageName) {
    std::cout << "Checking " << imageName << " at: " << path.string() << std::endl;
    std::cout << "Path exists: " << (fs::exists(path) ? "Yes" : "No") << std::endl;
    std::cout << "Is regular file: " << (fs::is_regular_file(path) ? "Yes" : "No") << std::endl;
}

void debugTexture(const std::string& name, const Texture2D& texture, const fs::path& path) {
    std::cout << "Loading " << name << ":\n";
    std::cout << "Path: " << path.string() << "\n";
    std::cout << "Exists: " << (fs::exists(path) ? "Yes" : "No") << "\n";
    std::cout << "Texture ID: " << texture.id << "\n";
    std::cout << "Dimensions: " << texture.width << "x" << texture.height << "\n";
}

// Add these near the top with other global variables
std::chrono::steady_clock::time_point wKeyPressStart;
bool wKeyPressed = false;
bool wKeyStuck = false;

bool notsendingugh = false;

// Add this function before client_main()
void handleStuckState() {
    // This function is called when player is stuck pressing W for 1 second
    std::cout << "Player is stuck!" << std::endl;
    logToFile("Player attempted to move up but was stuck", INFO);
    notsendingugh = true;
}

int castWinsock(boost::asio::ip::tcp::socket& socket) {
    SocketHandle nativeHandle = socket.native_handle();
    int intHandle;
#ifdef _WIN32
    intHandle = static_cast<int>(reinterpret_cast<intptr_t>(nativeHandle));
#else
    intHandle = nativeHandle;
#endif
    return intHandle;
}
std::atomic<bool> shouldQuit{false};
std::atomic<bool> debugRequested{false};

void debugInputThread() {
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "game") {
            debugRequested = true;
        }
    }
}

// Add this struct with your other structs (near Button struct)
struct TextBox {
    Rectangle bounds;
    char* text;
    int textSize;
    bool isSelected;
    const char* label;
};

// Add these functions before client_main()
void DrawTextBox(TextBox& box) {
    DrawRectangleRec(box.bounds, box.isSelected ? LIGHTGRAY : WHITE);
    DrawRectangleLinesEx(box.bounds, 2, BLACK);
    DrawText(box.text, box.bounds.x + 5, box.bounds.y + 5, 20, BLACK);
    DrawText(box.label, box.bounds.x, box.bounds.y - 25, 20, BLACK);
}

bool HandleTextBoxInput(TextBox& box) {
    #if defined(__APPLE__) && defined(__MOBILE__)
        ShowKeyboard();
    #endif
    bool changed = false;
    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (strlen(box.text) < box.textSize - 1)) {
            box.text[strlen(box.text)] = (char)key;
            box.text[strlen(box.text) + 1] = '\0';
            changed = true;
        }
        key = GetCharPressed();
    }
    
    if (IsKeyPressed(KEY_BACKSPACE) && strlen(box.text) > 0) {
        box.text[strlen(box.text) - 1] = '\0';
        changed = true;
    }
    return changed;
}

int client_main() {
    int WindowsOpen = 0;
    int screenWidth = getEnvVar<int>("SCREEN_WIDTH", 800);
    int screenHeight = getEnvVar<int>("SCREEN_HEIGHT", 450);
    int skin = getEnvVar<int>("wharfs", 1);
    int fps = getEnvVar<int>("FPS", 60);
    int port = getEnvVar<int>("PORT", 5767);
    int preferredLatency = getEnvVar<int>("PREFERRED_LATENCY", 150);
    std::string ip = getEnvVar<std::string>("IP", "127.0.1.1");
    std::cout << "Trying to connect to " << ip << " on port " << port << std::endl;
    std::string LocalName = getEnvVar<std::string>("NAME", "Player");
    std::cout << "Starting game with width = " << screenWidth << " height = " << screenHeight << " fps = " << fps << " FPS" << std::endl;

    if (fps > 99) fps = 99;

    InitWindow(screenWidth, screenHeight, "Game");
    WindowsOpen = WindowsOpen + 1;
    SetTargetFPS(fps);

    // Load player texture
    try {
        // Debug paths
        fs::path playerImgPath = root / "assets" / "player.png";
        fs::path compressedPlayerImgPath = root / "assets" / "compressedPlayer.png";
        fs::path bg1ImgPath = root / "assets" / "room1Bg.png";
        fs::path bg2ImgPath = root / "assets" / "room2Bg.png";
        
        debugImagePath(playerImgPath, "Player Image");
        debugImagePath(compressedPlayerImgPath, "Compressed Player Image");
        debugImagePath(bg1ImgPath, "Background Image");

        // Check if files exist
        if (!fs::exists(playerImgPath)) {
            std::string error = "Player image not found at: " + playerImgPath.string();
            logToFile(error, ERROR);
            throw std::runtime_error(error);
        }

        if (!fs::exists(compressedPlayerImgPath)) {
            std::string error = "Compressed player image not found at: " + compressedPlayerImgPath.string();
            logToFile(error, WARNING);
            std::cout << error << std::endl;
        }

        if (!fs::exists(bg1ImgPath)) {
            std::string error = "Background image not found at: " + bg1ImgPath.string();
            logToFile(error, WARNING);
            std::cout << error << std::endl;
        }

        if (!fs::exists(bg2ImgPath)) {
            std::string error = "Background image not found at: " + bg2ImgPath.string();
            logToFile(error, WARNING);
            std::cout << error << std::endl;
        }

        // Load player texture with error checking
        Texture2D playerTexture = LoadTexture(playerImgPath.string().c_str());
        if (playerTexture.id == 0) {
            std::string error = "Failed to load player texture at: " + playerImgPath.string();
            logToFile(error, ERROR);
            throw std::runtime_error(error);
        }
        std::cout << "Successfully loaded player texture with ID: " << playerTexture.id << std::endl;

        // Load background with error checking
        Texture2D room1BgT = {0};
        Texture2D room2BgT = {0};
        if (fs::exists(bg1ImgPath)) {
            room1BgT = LoadTexture(bg1ImgPath.string().c_str());
            if (room1BgT.id == 0) {
                std::string error = "Failed to load background texture at: " + bg1ImgPath.string();
                logToFile(error, WARNING);
            } else {
                std::cout << "Successfully loaded background texture with ID: " << room1BgT.id << std::endl;
            }
        }
        if (fs::exists(bg2ImgPath)) {
            room2BgT = LoadTexture(bg2ImgPath.string().c_str());
            if (room2BgT.id == 0) {
                std::string error = "Failed to load background texture at: " + bg2ImgPath.string();
                logToFile(error, WARNING);
            } else {
                std::cout << "Successfully loaded background texture with ID: " << room2BgT.id << std::endl;
            }
        }
        debugTexture("Background", room1BgT, bg1ImgPath);

        // Load player image from texture
        Image playerImage = LoadImageFromTexture(playerTexture);
        if (playerImage.data == nullptr) {
            std::string error = "Failed to create image from player texture";
            logToFile(error, ERROR);
            throw std::runtime_error(error);
        }
        Image room1Bg;
        try {
            room1BgT = LoadTexture(bg1ImgPath.string().c_str());
            if (room1BgT.id != 0) {
                room1Bg = LoadImageFromTexture(room1BgT);
            } else {
                logToFile("Failed to load background texture, continuing without background", WARNING);
            }
        } catch (const std::exception& e) {
            logToFile("Background image loading failed: " + std::string(e.what()), WARNING);
            // Continue without background
        }

        if (playerTexture.id == 0) {
            throw std::runtime_error("Failed to load background texture");
        }
        Image room2Bg;
        try {
            room2BgT = LoadTexture(bg2ImgPath.string().c_str());
            if (room2BgT.id != 0) {
                room2Bg = LoadImageFromTexture(room2BgT);
            } else {
                logToFile("Failed to load background texture, continuing without background", WARNING);
            }
        } catch (const std::exception& e) {
            logToFile("Background image loading failed: " + std::string(e.what()), WARNING);
            // Continue without background
        }


        // crop and load north (bottom right)
        Image croppedImage1 = ImageFromImage(playerImage, (Rectangle){static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage1.data == nullptr) {
            throw std::runtime_error("Failed to crop image 1");
        }
        Texture2D player1 = LoadTextureFromImage(croppedImage1);
        if (player1.id == 0) {
            std::string error = "Failed to load texture at: " + playerImgPath.string();
            logToFile(error, ERROR);
            std::cout << error << std::endl;
        }

        // crop and load east (top right)
        Image croppedImage2 = ImageFromImage(playerImage, (Rectangle){static_cast<float>(playerTexture.width)/2, 0.0, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage2.data == nullptr) {
            throw std::runtime_error("Failed to crop image 2");
        }
        Texture2D player2 = LoadTextureFromImage(croppedImage2);
        if (player2.id == 0) {
            std::string error = "Failed to load texture at: " + playerImgPath.string();
            logToFile(error, ERROR);
            std::cout << error << std::endl;
        }

        // crop and load south (bottom left)
        Image croppedImage3 = ImageFromImage(playerImage, (Rectangle){0.0, static_cast<float>(playerTexture.height)/2, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage3.data == nullptr) {
            throw std::runtime_error("Failed to crop image 3");
        }
        Texture2D player3 = LoadTextureFromImage(croppedImage3);
        if (player3.id == 0) {
            std::string error = "Failed to load texture at: " + playerImgPath.string();
            logToFile(error, ERROR);
            std::cout << error << std::endl;
        }

        // crop and load west (top left)
        Image croppedImage4 = ImageFromImage(playerImage, (Rectangle){0.0, 0.0, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage4.data == nullptr) {
            throw std::runtime_error("Failed to crop image 4");
        }
        Texture2D player4 = LoadTextureFromImage(croppedImage4);
        if (player4.id == 0) {
            std::string error = "Failed to load texture at: " + playerImgPath.string();
            logToFile(error, ERROR);
            std::cout << error << std::endl;
        }

        // Load crouch texture
        Texture2D player5 = LoadTexture(compressedPlayerImgPath.string().c_str());
        if (player5.id == 0) {
            std::string error = "Failed to load compressed texture at: " + compressedPlayerImgPath.string();
            logToFile(error, WARNING);
            std::cout << error << std::endl;
        }

        debugTexture("Player 1", player1, playerImgPath);
        debugTexture("Player 2", player2, playerImgPath);
        debugTexture("Player 3", player3, playerImgPath);
        debugTexture("Player 4", player4, playerImgPath);

        std::map<std::string, Texture2D> spriteSheet;
        spriteSheet["1"] = player1; spriteSheet["W"] = player1;
        spriteSheet["2"] = player2; spriteSheet["D"] = player2;
        spriteSheet["3"] = player3; spriteSheet["S"] = player3;
        spriteSheet["4"] = player4; spriteSheet["A"] = player4;
        spriteSheet["5"] = player5; spriteSheet["shift"] = player5; 
        UnloadImage(playerImage);
        UnloadImage(croppedImage1);

        json localPlayer;
        if (preferredLatency < 68 || preferredLatency > 1000) {
            preferredLatency = 150;
        }
        json previousChecklist = checklist; // Store the previous checklist
        std::map<std::string, bool> keys = DetectKeyPress();
        bool gameRunning = true;
        int moveSpeed = 5; // Adjust movement speed

        try {
            io_context io_context;
            tcp::socket socket(io_context);
            bool reconnecting = false;
            
            TextBox ipBox = {
                {static_cast<float>(screenWidth)/2 - 100.0f, static_cast<float>(screenHeight)/2 - 50.0f, 200.0f, 30.0f},
                new char[256]{0},
                256,
                false,
                "Server IP:"
            };
            TextBox portBox = {
                {static_cast<float>(screenWidth)/2 - 100.0f, static_cast<float>(screenHeight)/2 + 50.0f, 200.0f, 30.0f},
                new char[256]{0},
                256,
                false,
                "Port:"
            };
            
            Button reconnectButton = {{static_cast<float>(screenWidth)/2 - 100.0f, static_cast<float>(screenHeight)/2 + 120.0f, 200.0f, 40.0f}, "Reconnect", false};

            // Get initial values
            std::string ip = getEnvVar<std::string>("IP", "127.0.1.1");
            if (ip.find(":") != std::string::npos) {
                ip = ip.substr(0, ip.find(":"));
            }
            strncpy(ipBox.text, ip.c_str(), 255);
            strncpy(portBox.text, std::to_string(port).c_str(), 255);

            auto attemptConnection = [&]() -> bool {
                try {
                    if (socket.is_open()) socket.close();
                    socket = tcp::socket(io_context);
                    ip = std::string(ipBox.text);
                    port = std::stoi(std::string(portBox.text));
                    
                    std::cout << "Trying to connect to " << ip << " on port " << port << std::endl;
                    tcp::endpoint endpoint(ip::address::from_string(ip), port);
                    
                    boost::system::error_code ec;
                    socket.connect(endpoint, ec);
                    if (ec) {
                        std::cerr << "Failed to connect: " << ec.message() << std::endl;
                        return false;
                    }
                    
                    std::cout << "Connected to server" << std::endl;
                    return true;
                } catch (const std::exception& e) {
                    std::cerr << "Connection attempt failed: " << e.what() << std::endl;
                    return false;
                }
            };

            // Initial connection attempt
            if (!attemptConnection()) {
                reconnecting = true;
            }

            if (!attemptConnection()) {
                reconnecting = true;
            }

            boost::asio::streambuf buffer;
            bool initGameFully = false;
            json localPlayer;
            bool localPlayerSet = false;

            boost::asio::async_read_until(socket, buffer, "\n",
                std::bind(&handleRead, std::placeholders::_1, std::placeholders::_2,
                          std::ref(buffer), std::ref(localPlayer), std::ref(initGameFully),
                          std::ref(gameRunning), std::ref(socket), std::ref(localPlayerSet))
            );

            // Run io_context in a separate thread
            std::thread ioThread([&]() {
                io_context.run();
            });

            bool initGame = false;
            initGameFully = false;
            localPlayerSet = false;  //new flag to track if local player is set
            
            // Timer for sending updates
            auto lastSendTime = std::chrono::steady_clock::now();
            const std::chrono::milliseconds sendInterval(preferredLatency); // 255ms default interval; average human reaction time is 250ms but we want to save on aws container costs

            // Start asynchronous read
            boost::asio::async_read_until(socket, buffer, "\n", 
                [&](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                    handleRead(ec, bytes_transferred, buffer, localPlayer, initGameFully, gameRunning, socket, localPlayerSet);
                });
            // Main Game Loop
            json localPlayerInterpolatedPos = {};
            personalSpaceBubble bubble;
            while (!WindowShouldClose() && gameRunning) {
                if (!socket.is_open() || reconnecting) {
                    BeginDrawing();
                    ClearBackground(RAYWHITE);
                    
                    Vector2 mousePoint = GetMousePosition();
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        ipBox.isSelected = CheckCollisionPointRec(mousePoint, ipBox.bounds);
                        portBox.isSelected = CheckCollisionPointRec(mousePoint, portBox.bounds);
                        if (ipBox.isSelected == false && portBox.isSelected == false) {
                            #if defined(__APPLE__) && defined(__MOBILE__)
                            HideKeyboard();
                            #endif
                        }
                    }
                    
                    if (ipBox.isSelected) HandleTextBoxInput(ipBox);
                    #if defined(__APPLE__) && defined(__MOBILE__)
                    else HideKeyboard();
                    #endif
                    if (portBox.isSelected) HandleTextBoxInput(portBox);
                    #if defined(__APPLE__) && defined(__MOBILE__)
                    else HideKeyboard();
                    #endif
                    
                    DrawTextBox(ipBox);
                    DrawTextBox(portBox);
                    DrawButton(reconnectButton);
                    
                    DrawText("Connection lost. Please enter server details:", 
                            screenWidth/2 - 200, screenHeight/2 - 100, 20, BLACK);
                    
                    if (IsButtonPressed(reconnectButton, mousePoint)) {
                        if (attemptConnection()) {
                            reconnecting = false;
                            // Reset game state
                            initGame = false;
                            initGameFully = false;
                            localPlayerSet = false;
                            
                            // Restart async read
                            boost::asio::async_read_until(socket, buffer, "\n",
                                [&](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                                    handleRead(ec, bytes_transferred, buffer, localPlayer, 
                                             initGameFully, gameRunning, socket, localPlayerSet);
                                });
                        }
                    }
                    
                    EndDrawing();
                    continue;
                }

                // Run io_context to process async operations
                boost::system::error_code ec;
                io_context.poll_one(ec);
                if (ec) {
                    std::cerr << "IO Context error: " << ec.message() << std::endl;
                    logToFile("IO Context error: " + ec.message(), ERROR);
                    gameRunning = false;
                    break;
                };

                // Send initialization message only once at start
                if (!initGame) {
                    json newMessage = {
                        {"currentName", LocalName}
                    };
                    std::string messageStr = newMessage.dump() + "\n";
                    boost::asio::write(socket, boost::asio::buffer(messageStr));
                    std::cout << "Sent player creation request" << std::endl;
                    initGame = true;
                }

                BeginDrawing();
                ClearBackground(RAYWHITE);
                Button buttonW = {{static_cast<float>(screenWidth) - 180, static_cast<float>(screenHeight) - 240, 60, 60}, "W", false};
                Button buttonA = {{static_cast<float>(screenWidth) - 240, static_cast<float>(screenHeight) - 180, 60, 60}, "A", false};
                Button buttonS = {{static_cast<float>(screenWidth) - 180, static_cast<float>(screenHeight) - 180, 60, 60}, "S", false};
                Button buttonD = {{static_cast<float>(screenWidth) - 120, static_cast<float>(screenHeight) - 180, 60, 60}, "D", false};
                Button buttonShift = {{static_cast<float>(screenWidth) - 180, static_cast<float>(screenHeight) - 120, 60, 60}, "Shift", false};
                Button buttonQuit = {{static_cast<float>(screenWidth) - 180, static_cast<float>(screenHeight) - 60, 60, 60}, "Quit", false};
                // Show loading screen until fully initialized
                if (!localPlayerSet || !initGameFully) {
                    DrawText("Waiting for player initialization...", 10, 10, 20, BLACK);
                    DrawText("Try reconnecting if you've been here for a while", 40, 40, 20, BLACK);
                    DrawText("If you're stuck, you probably got kicked, just tell the server owner to restart the server", 40, 70, 20, BLACK);
                    DrawText("If you're still stuck, just report on github issues", 40, 100, 20, BLACK);
                    EndDrawing();
                    continue;  // Skip rest of loop until initialized
                }
                int localSocketId = localPlayer["socket"].get<int>();
                localPlayerInterpolatedPos = {
                    {"x", playerStates[localSocketId].current.x},
                    {"y", playerStates[localSocketId].current.y},
                    {"width", playerStates[localSocketId].current.width},
                    {"height", playerStates[localSocketId].current.height}
                };
                // Only handle game logic after initialization
                if (localPlayerSet && initGameFully) {
                    localRoomName = "room" + std::to_string(localPlayer["room"].get<int>());
                    int socketHandle = castWinsock(socket);

                    for (const auto& room : game.items()) {
                        if (room.value().contains("players")) {
                            for (const auto& player : room.value()["players"]) {
                                if (player["socket"] == socketHandle) {
                                    localRoomName = "room" + std::to_string(room.value()["roomID"].get<int>());
                                    break;
                                }
                            }
                        }
                    }
                    bool switchr = false;
                    //Get player count
                    int playerCount = 0;
                    for (const auto& player : game[localRoomName]["players"]) {
                        playerCount++;
                    }
                    checklist["playerCount"] = playerCount;
                   // Iterate over objects in the current room
                    if (game.contains(localRoomName) && game[localRoomName].contains("objects")) {
                        // Reset movement flags at the start of each frame
                        canMove["w"] = true;
                        canMove["a"] = true;
                        canMove["s"] = true;
                        canMove["d"] = true;                    

                        //get players in bubble
                        bubble.clear_players();
                        bubble.set_bubble(localPlayerInterpolatedPos["x"].get<float>(), localPlayerInterpolatedPos["y"].get<float>(), localPlayerInterpolatedPos["width"].get<float>(), localPlayerInterpolatedPos["height"].get<float>());
                        for (const auto& player : game[localRoomName]["players"]) {
                            if (player["socket"] != socketHandle) {
                                bubble.add_player(player);
                            }
                            if (bubble.check_burst() && bubble.check_specific_burst(player)) {
                                //add player to objects so no going through
                                if (player["room"] == localRoomName) {
                                    json pushbackable = player;
                                    pushbackable["objID"] = 0;
                                    game[localRoomName]["objects"].push_back(pushbackable);
                                }
                            }
                        }
                        for (const auto& object : game[localRoomName]["objects"]) {
                            json predictedPos = localPlayerInterpolatedPos;
                            int wall = 0;                   

                            // Check for collisions in all directions
                            // Up
                            predictedPos["y"] = localPlayerInterpolatedPos["y"].get<float>() - moveSpeed;
                            if (checkWallCollision(predictedPos, object, wall)) {
                                if (wall == 1) {
                                    canMove["w"] = false;
                                    checklist["y"] = object["y"].get<float>() + object["height"].get<float>(); // Stop at top boundary
                                }
                            }                   

                            // Down
                            predictedPos["y"] = localPlayerInterpolatedPos["y"].get<float>() + moveSpeed;
                            if (checkWallCollision(predictedPos, object, wall)) {
                                if (wall == 3) {
                                    canMove["s"] = false;
                                    checklist["y"] = object["y"].get<float>() - localPlayerInterpolatedPos["height"].get<float>(); // Stop at bottom boundary
                                }
                            }                   

                            // Left
                            predictedPos["x"] = localPlayerInterpolatedPos["x"].get<float>() - moveSpeed;
                            if (checkWallCollision(predictedPos, object, wall)) {
                                if (wall == 4) {
                                    canMove["a"] = false;
                                    checklist["x"] = object["x"].get<float>() + object["width"].get<float>(); // Stop at left boundary
                                }
                            }                   

                            // Right
                            predictedPos["x"] = localPlayerInterpolatedPos["x"].get<float>() + moveSpeed;
                            if (checkWallCollision(predictedPos, object, wall)) {
                                if (wall == 2) {
                                    canMove["d"] = false;
                                    checklist["x"] = object["x"].get<float>() - localPlayerInterpolatedPos["width"].get<float>(); // Stop at right boundary
                                }
                            }
                            //special collisions
                            if ((object["objID"] == 2 || object["objID"] == 4) && checkCollision(localPlayerInterpolatedPos, object)) {
                                int newRoom = (object["objID"] == 2) ? 2 : ((object["objID"] == 4) ? 1 : 1);                                
                                checklist["room"] = newRoom;
                                checklist["x"] = (newRoom == 1) ? 700 : 100;  // Different spawn points per room
                                checklist["y"] = (newRoom == 1) ? 300 : 100;
                                
                                localPlayer["room"] = newRoom;
                                localPlayer["x"] = checklist["x"];
                                localPlayer["y"] = checklist["y"];
                                
                                // Force reset movement flags
                                canMove = {{"w", true}, {"a", true}, {"s", true}, {"d", true}};
                                notsendingugh = false;
                                
                                // Send room change and position update
                                json roomChangeMessage = {
                                    {"room", newRoom},
                                    {"updatePosition", {
                                        {"x", checklist["x"]},
                                        {"y", checklist["y"]},
                                        {"room", newRoom},
                                        {"socket", localPlayer["socket"]},
                                        {"spriteState", checklist["spriteState"]}
                                    }}
                                };
                                
                                boost::asio::write(socket, boost::asio::buffer(roomChangeMessage.dump() + "\n"));
                                bool send = false;
                                lastSendTime = std::chrono::steady_clock::now() - sendInterval;
                            }
                        }
                    }

                    //player state goes back to if not moving
                    int backPoint = 3;
                    Vector2 mousePoint;
                    mousePoint = GetMousePosition();

                    keys = DetectKeyPress();
                    bool send = false;

                    // Store previous position
                    int prevX = checklist["x"].get<int>();
                    int prevY = checklist["y"].get<int>();
                    if (switchr) send = true; switchr = false;
                    
                    if (keys["shift"] || IsButtonPressed(buttonShift, mousePoint)) {
                        if (checklist["spriteState"].get<int>() != 5) {
                            checklist["prevState"] = checklist["spriteState"]; 
                            checklist["spriteState"] = 5;  
                            send = true;
                        }
                        moveSpeed = 2;  // Slower while crouched
                    } else if (checklist["spriteState"].get<int>() == 5) {

                        if (checklist.contains("prevState")) {
                            checklist["spriteState"] = checklist["prevState"];
                            moveSpeed = 5; 
                        } else {
                            checklist["spriteState"] = 3; 
                            moveSpeed = 5;
                        }
                        moveSpeed = 5; 
                        send = true;
                    }

                    if ((keys["w"] || IsButtonPressed(buttonW, mousePoint)) && canMove["w"]) {
                        std::cout << "W pressed, canMove[w]: " << canMove["w"] << std::endl;
                        checklist["goingup"] = true;
                        checklist["y"] = prevY - moveSpeed;
                        checklist["spriteState"] = 1; // North facing
                        send = true;
                        wKeyStuck = false;
                        wKeyPressed = true;
                        wKeyPressStart = std::chrono::steady_clock::now();
                    } else if (keys["w"] || IsButtonPressed(buttonW, mousePoint)) {
                        // W is pressed but can't move
                        if (!wKeyPressed) {
                            wKeyPressed = true;
                            wKeyPressStart = std::chrono::steady_clock::now();
                        } else if (!wKeyStuck) {
                            auto now = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - wKeyPressStart);
                            if (duration.count() >= 1000) {  // 1 second
                                handleStuckState();
                                wKeyStuck = true;
                            }
                        }
                    } else {
                        checklist["goingup"] = false;
                        wKeyPressed = false;
                        wKeyStuck = false;
                    }

                    if ((keys["s"] || IsButtonPressed(buttonS, mousePoint)) && canMove["s"]) {
                        checklist["goingdown"] = true; 
                        checklist["y"] = prevY + moveSpeed;
                        checklist["spriteState"] = 3; // South facing
                        send = true;
                    } else {
                        checklist["goingdown"] = false;
                    }

                    if ((keys["a"] || IsButtonPressed(buttonA, mousePoint)) && canMove["a"]) {
                        checklist["goingleft"] = true;
                        checklist["x"] = prevX - moveSpeed;
                        checklist["spriteState"] = 4; // West facing
                        send = true;
                    } else {
                        checklist["goingleft"] = false;
                    }

                    if ((keys["d"] || IsButtonPressed(buttonD, mousePoint)) && canMove["d"]) {
                        checklist["goingright"] = true;
                        checklist["x"] = prevX + moveSpeed;
                        checklist["spriteState"] = 2; // East facing
                        send = true;
                    } else {
                        checklist["goingright"] = false;
                    }

                    if (keys["q"] || IsButtonPressed(buttonQuit, mousePoint)) {
                        gameRunning = false;
                        shouldQuit = true;
                        json quitMessage = {{"quitGame", true}};
                        boost::asio::write(socket, boost::asio::buffer(quitMessage.dump() + "\n"));
                        socket.close();
                        break;
                    }

                    // Add bounds checking
                    int screenWidth = GetScreenWidth();
                    int screenHeight = GetScreenHeight();
                    
                    checklist["x"] = std::max(0, std::min(screenWidth - 32, checklist["x"].get<int>()));
                    checklist["y"] = std::max(0, std::min(screenHeight - 32, checklist["y"].get<int>()));
                    
                    // Only send if checklist has changed and interval has passed
                    auto now = std::chrono::steady_clock::now();
                    if (send && (now - lastSendTime) >= sendInterval && checklist != previousChecklist) {
                        std::string messageStr = checklist.dump() + "\n";
                        boost::asio::write(socket, boost::asio::buffer(messageStr));
                        lastSendTime = now;
                        previousChecklist = checklist; // Update previous checklist
                    }
                }
                std::string localRoomName = "room" + std::to_string(localPlayer["room"].get<int>());
                if (localPlayerSet && initGameFully) {
                    std::string currentRoom = "room" + std::to_string(localPlayer["room"].get<int>());
                    BeginDrawing();
                    ClearBackground(RAYWHITE);
                    localRoomName = "room" + std::to_string(localPlayer["room"].get<int>());
                    // Draw background if available
                    if (currentRoom == "room1" && room1BgT.id != 0) {
                        DrawTexture(room1BgT, 0, 0, WHITE);
                    } else if (currentRoom == "room2" && room2BgT.id != 0) {
                        DrawTexture(room2BgT, 0, 0, WHITE);
                    }
                    DrawButton(buttonW);DrawButton(buttonA);DrawButton(buttonS);DrawButton(buttonD); DrawButton(buttonShift); DrawButton(buttonQuit); if (notsendingugh) {DrawText("You are stuck! You probably got kicked though...", 10, 10, 20, BLACK);}

                    float deltaTime = GetFrameTime();
                    
                    // Update all player states
                    // Update and draw all players
                    
                    for (auto& [socketId, state] : playerStates) {
                        if (state.room == localPlayer["room"].get<int>()) {  // Only draw players in same room
                            state.update(deltaTime);  // Updates interpolation for smooth movement

                            // Draw player sprite based on interpolated position
                            if (spriteSheet.find(std::to_string(state.spriteState)) != spriteSheet.end()) {
                                Texture2D currentSprite = spriteSheet[std::to_string(state.spriteState)];
                                Rectangle sourceRect;
                                
                                // Handle crouching sprite differently
                                if (state.spriteState == 5) {
                                    sourceRect = (Rectangle){ 0, 0, 48, 47 }; // Compressed sprite dimensions
                                } else {
                                    sourceRect = (Rectangle){ 0, 0, 64, 64 }; // Normal sprite dimensions
                                }

                                Rectangle destRect = {
                                    state.current.x, 
                                    state.current.y, 
                                    state.spriteState == 5 ? 48.0f : 64.0f,  // Scale only in rendering
                                    state.spriteState == 5 ? 47.0f : 64.0f   // Scale only in rendering
                                };
                                if (state.room == localPlayer["room"].get<int>()) {
                                DrawTexturePro(
                                    currentSprite,
                                    sourceRect,    // Source rectangle from sprite sheet
                                    destRect,      // Destination rectangle with current dimensions
                                    (Vector2){ 0, 0 },
                                    0.0f,
                                    WHITE
                                );}
                            } else {
                                DrawRectangle(
                                    state.current.x, 
                                    state.current.y, 
                                    state.current.width, 
                                    state.current.height, 
                                    RED
                                );
                            }

                            // Draw player name
                            DrawText(state.name.c_str(), 
                                    state.current.x - 10, 
                                    state.current.y - 20, 
                                    20, BLACK);
                        }
                    }

                    EndDrawing();
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
        try {
            UnloadTexture(playerTexture);
            UnloadTexture(player1);
            UnloadTexture(player2);
            UnloadTexture(player3);
            UnloadTexture(player4);
        } catch (const std::exception& e) {
            std::cout << "May be a problem with unloading textures: " << e.what() << std::endl;
        }

        CloseWindow();
        WindowsOpen = WindowsOpen - 1;
        return 0;
    } catch (const std::exception& e) {
        logToFile(std::string("ERROR: ") + e.what());
        std::cerr << "Exception: " << e.what() << std::endl;
        CloseWindow();
        WindowsOpen = WindowsOpen - 1;
        return -1;
    }
}
int main() {
    return client_main();
}