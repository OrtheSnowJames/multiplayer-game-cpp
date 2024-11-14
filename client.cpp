#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include "raylib.h"

using namespace std;
using namespace boost::asio;
using ip::tcp;
using json = nlohmann::json;

void logToFile(const std::string& message) {
    try {
        std::ofstream logFile("game.log", std::ios::app);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        logFile << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") 
                << " - " << message << std::endl;
        logFile.close();
    }
    catch(const std::exception& e) {
        std::cerr << "Logging failed: " << e.what() << std::endl;
    }
}

int main() {
    // Initialization
    Texture2D playerTexture = LoadTexture("player.png");
    const int screenWidth = std::getenv("SCREEN_WIDTH") ? std::atoi(std::getenv("SCREEN_WIDTH")) : 800;
    const int screenHeight = std::getenv("SCREEN_HEIGHT") ? std::atoi(std::getenv("SCREEN_HEIGHT")) : 450;
    int fps = std::getenv("FPS") ? std::atoi(std::getenv("FPS")) : 60;
    const int port = std::getenv("PORT") ? std::atoi(std::getenv("PORT")) : 1234;
    const std::string ip = std::getenv("IP") ? std::getenv("IP") : "127.0.0.1";
    const std::string LocalName = std::getenv("NAME") ? std::getenv("NAME") : "Player";

    if (fps > 99) fps = 99;

    json game = {
        {"room1", {
            {"players", {}},
            {"objects", {}},
            {"enemies", {}}
        }}
    };
    json localPlayer;
    bool gameRunning = true;

    InitWindow(screenWidth, screenHeight, "Game");
    SetTargetFPS(fps);

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
            if (!initGame) {
                json newMessage = {
                    {"requestGame", true},
                    {"currentGame", "game1"},
                    {"currentPlayer", LocalName}
                };
                boost::asio::write(socket, boost::asio::buffer(newMessage.dump() + "\n"));
                initGame = true;
            }

            json checkList = {
                {"goingup", false},
                {"goingleft", false},
                {"goingright", false},
                {"goingdown", false},
                {"quitGame", false},
                {"requestGame", false},
                {"currentGame", ""},
                {"currentPlayer", ""}
            };

            

            if (IsWindowResized()) SetWindowSize(screenWidth, screenHeight);

            std::string message;
            boost::system::error_code error;
            boost::asio::read_until(socket, buffer, "\n", error);
            if (!error) {
                std::istream input_stream(&buffer);
                std::getline(input_stream, message);
                json messageJson = json::parse(message);

                if (messageJson.contains("quitGame") && messageJson["quitGame"].get<bool>()) {
                    gameRunning = false;
                    break;
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

            if (goingUp) localPlayer["y"] = localPlayer["y"].get<int>() - localPlayer["speed"].get<int>();
            if (goingDown) localPlayer["y"] = localPlayer["y"].get<int>() + localPlayer["speed"].get<int>();
            if (goingLeft) localPlayer["x"] = localPlayer["x"].get<int>() - localPlayer["speed"].get<int>();
            if (goingRight) localPlayer["x"] = localPlayer["x"].get<int>() + localPlayer["speed"].get<int>();

            // Draw Game Elements
            for (auto& p : game[localPlayer["room"]]["players"]) {
                DrawText(p["name"].get<std::string>().c_str(), p["x"].get<int>() + 10, p["y"].get<int>(), 20, BLACK);
                DrawRectangle(p["x"].get<int>(), p["y"].get<int>() + 20, 20, 20, RED);
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
        return -1;
    }
    CloseWindow();
    return 0;
}
