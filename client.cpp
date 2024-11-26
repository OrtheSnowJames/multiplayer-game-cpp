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
    {"currentPlayer", ""}
};

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

    return keyStates;
}

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
    std::string command = "bash ./run.sh";
    
    // Use exec to replace current process
    if (execl("/bin/bash", "bash", "./run.sh", nullptr) == -1) {
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



void handleRead(const boost::system::error_code& error, std::size_t bytes_transferred, boost::asio::streambuf& buffer, json& localPlayer, bool& initGameFully, bool& gameRunning, tcp::socket& socket, bool& localPlayerSet) {
    if (!error) {
        std::istream input_stream(&buffer);
        std::string message;
        std::getline(input_stream, message);
        
        std::cout << "Received message: " << message << std::endl;
        std::cout << "LocalPlayerSet: " << localPlayerSet << std::endl;
        std::cout << "InitGameFully: " << initGameFully << std::endl;

        try {
            std::cout << "Client received: " << message << std::endl;
            json messageJson = json::parse(message);
    
            if (messageJson.contains("local") && messageJson["local"].get<bool>()) {
                // Ensure spriteState is treated as an int
                try {
                    if (messageJson.contains("spriteState")) {
                        messageJson["spriteState"] = getSafeSpriteSate(messageJson, "spriteState");
                        checklist["x"] = messageJson["x"];
                        checklist["y"] = messageJson["y"];
                        //construct room to add to
                        std::string roomToAddTo = "room" + std::to_string(messageJson["room"].get<int>());
                        game[roomToAddTo]["players"].push_back(messageJson);
                    }
                } catch(const std::exception& e) {
                    std::cerr << "ERROR AT SPRITESTATE IN HANDLE READ: " << e.what() << std::endl;
                    logToFile("ERROR AT SPRITESTATE: " + std::string(e.what()), ERROR);
                }

                localPlayer = messageJson;
                localPlayerSet = true;
                std::cout << "Local player set: " << localPlayer.dump() << std::endl;
                
                // Request full game state only if not already initialized
                if (!initGameFully) {
                    json gameRequest = {{"requestGame", true}};
                    boost::asio::write(socket, boost::asio::buffer(gameRequest.dump() + "\n"));
                }
            }
            
            if (messageJson.contains("getGame")) {
                auto& players = messageJson["getGame"]["room1"]["players"];
                for (auto& player : players) {
                    if (player.contains("spriteState")) {
                        if (!player["spriteState"].is_number()) {
                            player["spriteState"] = 0; // Default value if not a number
                        }
                    } else {
                        player["spriteState"] = 0; // Default value if not present
                    }
                }
                
                game = messageJson["getGame"];
                initGameFully = true;
                std::cout << "Game state updated" << std::endl;
            }

            if (messageJson.contains("updatePosition")) {
                int socketId = messageJson["updatePosition"]["socket"].get<int>();
                int newX = messageJson["updatePosition"].value("x", -1);
                int newY = messageJson["updatePosition"].value("y", -1);

                for (auto& room : game.items()) {
                    for (auto& player : room.value()["players"]) {
                        if (player["socket"].get<int>() == socketId) {
                            if (newX != -1) player["x"] = newX;
                            if (newY != -1) player["y"] = newY;
                            break;
                        }
                    }
                }
            }

            // Clear the buffer before starting a new read
            buffer.consume(buffer.size());

            // Continue reading
            boost::asio::async_read_until(socket, buffer, "\n",
                [&](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                    handleRead(ec, bytes_transferred, buffer, localPlayer, initGameFully, gameRunning, socket, localPlayerSet);
                });
        }
        catch (const json::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << "\nMessage was: " << message << std::endl;
            logToFile("JSON parsing error: " + std::string(e.what()) + "\nMessage was: " + message);
        }
    }
}

int client_main() {
    int WindowsOpen = 0;
    int screenWidth = getEnvVar<int>("SCREEN_WIDTH", 800);
    int screenHeight = getEnvVar<int>("SCREEN_HEIGHT", 450);
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
        fs::path playerImgPath = root / "player.png";
        
        // Add error checking for background image
        fs::path bg1ImgPath = root / "room1Bg.png";
        if (!fs::exists(bg1ImgPath)) {
            std::string error = "Background image not found at: " + bg1ImgPath.string();
            logToFile(error, ERROR);
            throw std::runtime_error(error);
        }
        
        Texture2D playerTexture = LoadTexture(playerImgPath.string().c_str());
        if (playerTexture.id == 0) {
            std::string error = "Failed to load texture at: " + playerImgPath.string();
            logToFile(error, ERROR);
            std::cout << error << std::endl;
        }
        Texture2D room1BgT;
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
            throw std::runtime_error("Failed to load player texture");
        }

        Image playerImage = LoadImageFromTexture(playerTexture);
        if (playerImage.data == nullptr) {
            throw std::runtime_error("Failed to load player image");
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

        std::map<std::string, Texture2D> spriteSheet;
        spriteSheet["1"] = player1; spriteSheet["W"] = player1;
        spriteSheet["2"] = player2; spriteSheet["D"] = player2;
        spriteSheet["3"] = player3; spriteSheet["S"] = player3;
        spriteSheet["4"] = player4; spriteSheet["A"] = player4;
        UnloadImage(playerImage);
        UnloadImage(croppedImage1);

        json localPlayer;
        if (preferredLatency < 68 || preferredLatency > 1000) {
            preferredLatency = 150;
        }
        json previousChecklist = checklist; // Store the previous checklist
        std::map<std::string, bool> keys = DetectKeyPress();
        bool gameRunning = true;
        try {
            io_context io_context;
            tcp::socket socket(io_context);
            try {
                std::string ip = getEnvVar<std::string>("IP", "127.0.1.1");
                if (ip.find(":") != std::string::npos) {
                    // Strip port if accidentally included in IP
                    ip = ip.substr(0, ip.find(":"));
                }
                std::cout << "Trying to connect to " << ip << " on port " << port << std::endl;
                tcp::endpoint endpoint(ip::address::from_string(ip), port);
                // ... rest of connection code
                boost::system::error_code ec;
                int retryCount = 5;
                while (retryCount > 0) {
                    socket.connect(endpoint, ec);
                    if (!ec) {
                        break;
                    }
                    std::cerr << "Failed to connect: " << ec.message() << ". Retrying in 5 seconds..." << std::endl;
                    logToFile("Failed to connect: " + ec.message() + ". Retrying in 5 seconds...");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    retryCount--;
                }
                if (ec) {
                    std::cerr << "Failed to connect after retries: " << ec.message() << std::endl;
                    logToFile("Failed to connect after retries: " + ec.message());
                    return -1;
                }
                std::cout << "Connected to server" << std::endl;
            } catch (const std::exception& e) {
                logToFile(std::string("ERROR: ") + e.what());
                std::cerr << "Exception: " << e.what() << std::endl;
                CloseWindow();
                WindowsOpen = WindowsOpen - 1;
                return -1;
            }

            boost::asio::streambuf buffer;
            bool initGame = false;
            bool initGameFully = false;
            bool localPlayerSet = false;  // New flag to track if local player is set
            
            // Timer for sending updates
            auto lastSendTime = std::chrono::steady_clock::now();
            const std::chrono::milliseconds sendInterval(preferredLatency); // 255ms default interval; average human reaction time is 250ms but we want to save on aws container costs

            // Start asynchronous read
            boost::asio::async_read_until(socket, buffer, "\n", 
                [&](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                    handleRead(ec, bytes_transferred, buffer, localPlayer, initGameFully, gameRunning, socket, localPlayerSet);
                });
            // Main Game Loop
            while (!WindowShouldClose() && gameRunning) {
                // Run io_context to process async operations
                io_context.poll();

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

                // Show loading screen until fully initialized
                if (!localPlayerSet || !initGameFully) {
                    DrawText("Waiting for player initialization...", 10, 10, 20, BLACK);
                    DrawText("Try reconnecting if you've been here for a while", 40, 40, 20, BLACK);
                    EndDrawing();
                    continue;  // Skip rest of loop until initialized
                }
                
                // Only handle game logic after initialization
                if (localPlayerSet && initGameFully) {
                    keys = DetectKeyPress();
                    bool send = false;
                    int moveSpeed = 5; // Adjust movement speed

                    // Store previous position
                    int prevX = checklist["x"].get<int>();
                    int prevY = checklist["y"].get<int>();

                    if (keys["w"]) {
                        checklist["goingup"] = true;
                        checklist["y"] = prevY - moveSpeed;
                        checklist["spriteState"] = 1; // North facing
                        send = true;
                    } else {
                        checklist["goingup"] = false;
                    }

                    if (keys["s"]) {
                        checklist["goingdown"] = true; 
                        checklist["y"] = prevY + moveSpeed;
                        checklist["spriteState"] = 3; // South facing
                        send = true;
                    } else {
                        checklist["goingdown"] = false;
                    }

                    if (keys["a"]) {
                        checklist["goingleft"] = true;
                        checklist["x"] = prevX - moveSpeed;
                        checklist["spriteState"] = 4; // West facing
                        send = true;
                    } else {
                        checklist["goingleft"] = false;
                    }

                    if (keys["d"]) {
                        checklist["goingright"] = true;
                        checklist["x"] = prevX + moveSpeed;
                        checklist["spriteState"] = 2; // East facing
                        send = true;
                    } else {
                        checklist["goingright"] = false;
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

                if (localPlayerSet && initGameFully) {
                    BeginDrawing();
                    ClearBackground(RAYWHITE);

                    // Draw background if available
                    if (room1BgT.id != 0) {
                        DrawTexture(room1BgT, 0, 0, WHITE);
                    }

                    // Draw all players
                    for (auto& room : game.items()) {
                        for (auto& player : room.value()["players"]) {
                            int x = player["x"].get<int>();
                            int y = player["y"].get<int>();
                            
                            // Draw player sprite based on spriteState
                            int spriteState = player["spriteState"].get<int>();
                            std::string spriteKey = std::to_string(spriteState);
                            
                            if (spriteSheet.find(spriteKey) != spriteSheet.end()) {
                                DrawTexture(spriteSheet[spriteKey], x, y, WHITE);
                            } else {
                                // Fallback rectangle if sprite not found
                                DrawRectangle(x, y, 32, 32, RED);
                            }
                            
                            // Draw player name
                            DrawText(player["name"].get<std::string>().c_str(), 
                                    x - 10, y - 20, 20, BLACK);
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