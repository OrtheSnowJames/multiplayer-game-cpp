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
json checklist = 
{
    {"goingup", false},
    {"goingleft", false},
    {"goingright", false},
    {"goingdown", false},
    {"quitGame", false},
    {"requestGame", false},
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
        
        try {
            std::cout << "Client received: " << message << std::endl;
            json messageJson = json::parse(message);
            
            if (messageJson.contains("local") && messageJson["local"].get<bool>()) {
                // Convert spriteState to string if it's a number
                if (messageJson.contains("spriteState") && messageJson["spriteState"].is_number()) {
                    messageJson["spriteState"] = std::to_string(messageJson["spriteState"].get<int>());
                }
                
                localPlayer = messageJson;
                localPlayerSet = true;
                std::cout << "Local player set: " << localPlayer.dump() << std::endl;
                
                // Request full game state
                json gameRequest = {{"requestGame", true}};
                boost::asio::write(socket, boost::asio::buffer(gameRequest.dump() + "\n"));
            }
            
            if (messageJson.contains("getGame")) {
                auto& players = messageJson["getGame"]["room1"]["players"];
                for (auto& player : players) {
                    if (player.contains("spriteState")) {
                        if (player["spriteState"].is_number()) {
                            player["spriteState"] = std::to_string(player["spriteState"].get<int>());
                        } else if (!player["spriteState"].is_string()) {
                            player["spriteState"] = "";
                        }
                    } else {
                        player["spriteState"] = "";
                    }
                }
                
                game = messageJson["getGame"];
                initGameFully = true;
                std::cout << "Game state updated" << std::endl;
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

int main() {
    int WindowsOpen = 0;
    int screenWidth = getEnvVar<int>("SCREEN_WIDTH", 800);
    int screenHeight = getEnvVar<int>("SCREEN_HEIGHT", 450);
    int fps = getEnvVar<int>("FPS", 60);
    int port = getEnvVar<int>("PORT", 5767);
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
        fs::path bg1ImgPath = root / "room1Bg.jpeg";
        
        // Ensure paths are converted to strings
        std::string playerImgPathStr = playerImgPath.string();
        std::string bg1ImgPathStr = bg1ImgPath.string();
        
        Texture2D playerTexture = LoadTexture(playerImgPathStr.c_str());
        Texture2D room1BgT = LoadTexture(bg1ImgPathStr.c_str());

        Image room1Bg = LoadImageFromTexture(room1BgT);

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
            throw std::runtime_error("Failed to load player 1 texture");
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // Load player texture
    try {
        fs::path playerImgPath = root / "player.png";
        fs::path bg1ImgPath = root / "room1Bg.jpeg";
        Texture2D playerTexture = LoadTexture(playerImgPath.string().c_str());
        Texture2D room1BgT = LoadTexture(bg1ImgPath.string().c_str());

        Image room1Bg = LoadImageFromTexture(room1BgT);

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
            throw std::runtime_error("Failed to load player 1 texture");
        }

        // crop and load east (top right)
        Image croppedImage2 = ImageFromImage(playerImage, (Rectangle){static_cast<float>(playerTexture.width)/2, 0.0, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage2.data == nullptr) {
            throw std::runtime_error("Failed to crop image 2");
        }
        Texture2D player2 = LoadTextureFromImage(croppedImage2);
        if (player2.id == 0) {
            throw std::runtime_error("Failed to load player 2 texture");
        }

        // crop and load south (bottom left)
        Image croppedImage3 = ImageFromImage(playerImage, (Rectangle){0.0, static_cast<float>(playerTexture.height)/2, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage3.data == nullptr) {
            throw std::runtime_error("Failed to crop image 3");
        }
        Texture2D player3 = LoadTextureFromImage(croppedImage3);
        if (player3.id == 0) {
            throw std::runtime_error("Failed to load player 3 texture");
        }

        // crop and load west (top left)
        Image croppedImage4 = ImageFromImage(playerImage, (Rectangle){0.0, 0.0, static_cast<float>(playerTexture.width)/2, static_cast<float>(playerTexture.height)/2});
        if (croppedImage4.data == nullptr) {
            throw std::runtime_error("Failed to crop image 4");
        }
        Texture2D player4 = LoadTextureFromImage(croppedImage4);
        if (player4.id == 0) {
            throw std::runtime_error("Failed to load player 4 texture");
        }

        std::map<std::string, Texture2D> spriteSheet;
        spriteSheet["1"] = player1; spriteSheet["W"] = player1;
        UnloadImage(playerImage);
        UnloadImage(croppedImage1);

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

            boost::asio::streambuf buffer;
            bool initGame = false;
            bool initGameFully = false;
            bool localPlayerSet = false;  // New flag to track if local player is set
            
            // Start asynchronous read
            boost::asio::async_read_until(socket, buffer, "\n", 
                [&buffer, &localPlayer, &initGameFully, &gameRunning, &socket, &localPlayerSet](const boost::system::error_code& ec, std::size_t bytes_transferred) {
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
                    EndDrawing();
                    continue;  // Skip rest of loop until initialized
                }
                
                // Only handle game logic after initialization
                if (localPlayerSet && initGameFully) {
                    // Handle key presses
                    int keyCode = GetKeyPressed();
                    if (keyCode != 0) {
                        // ... key handling code ...
                    }
                    
                    // Draw game state
                    for (const auto p : game[localPlayer["room"].get<std::string>()]["players"]) {
                        DrawText(p["name"].get<std::string>().c_str(), 
                                p["x"].get<int>() + 10, 
                                p["y"].get<int>(), 
                                20, 
                                BLACK);
                        DrawTexture(spriteSheet[p["spriteState"].get<std::string>()], 
                                   p["x"].get<int>(), 
                                   p["y"].get<int>(), 
                                   WHITE);
                    }
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
