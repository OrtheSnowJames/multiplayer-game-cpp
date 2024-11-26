#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "raylib.h"
#include <thread>
#include <random>
#include <vector>
#include <set>
#include <mutex>
#include "coolfunctions.hpp"
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <atomic>
#include <boost/asio/signal_set.hpp>

using json = nlohmann::json;
using boost::asio::ip::tcp;

#include <boost/asio/signal_set.hpp>

void setupSignalHandlers(boost::asio::io_context& io_context) {
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        std::cout << "Shutting down server..." << std::endl;
        io_context.stop();
    });
}

std::vector<std::shared_ptr<tcp::socket>> connected_sockets;
json game = {
    {"room1", {
        {"players", {}},
        {"objects", {}},
        {"enemies", {}}
    }},
    {"room2", {
        {"players", {}},
        {"objects", {}},
        {"enemies", {}}
    }}
};
boost::asio::io_context io_context;
std::mutex socket_mutex;

// Add a atomic boolean for coordinating shutdown
std::atomic<bool> shouldClose{false};

enum LogLevel { INFO, ERROR, DEBUG };

void logToFile(const std::string& message, LogLevel level = INFO) {
    static const std::map<LogLevel, std::string> levelNames = {
        {INFO, "INFO"}, {ERROR, "ERROR"}, {DEBUG, "DEBUG"}};

    std::ofstream logFile("err.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    logFile << "[" << levelNames.at(level) << "] " << std::ctime(&now_time) << message << std::endl;
}

void reportError(const std::string& message){
    logToFile(message, ERROR);
    std::cerr << message << std::endl;
    return;
}
std::mutex game_mutex; // Add a separate mutex for game

json createUser(const std::string& name, int id) {
    std::random_device rd;
    return {
        {"name", name},
        {"x", rd() % 800},
        {"y", rd() % 450},
        {"speed", 5},
        {"score", 0},
        {"inventory", {{"shields", 0}, {"bananas", 0}}},
        {"socket", id},
        {"spriteState", 0}, // Ensure spriteState is an int
        {"skin", 1},
        {"local", false},
        {"room", 1}
    };
}

void eraseUser(int id) {
    std::lock_guard<std::mutex> lock(game_mutex); // Lock game during modifications
    try {
        if (game.contains("room1") && game["room1"].contains("players")) {
            auto& players = game["room1"]["players"];
            players.erase(
                std::remove_if(players.begin(), players.end(),
                    [id](const json& p) { 
                        return p["socket"] == id; 
                    }
                ),
                players.end()
            );
        }
        
        if (game.contains("room2") && game["room2"].contains("players")) {
            auto& players = game["room2"]["players"];
            players.erase(
                std::remove_if(players.begin(), players.end(),
                    [id](const json& p) { 
                        return p["socket"] == id; 
                    }
                ),
                players.end()
            );
        }

        // Remove socket from connected_sockets
        auto it = std::remove_if(connected_sockets.begin(), connected_sockets.end(),
            [id](const std::shared_ptr<tcp::socket>& socket) {
                return socket->native_handle() == id;
            });
        connected_sockets.erase(it, connected_sockets.end());

    } catch (const std::exception& e) {
        std::cerr << "Error in eraseUser: " << e.what() << std::endl;
        logToFile("Error in eraseUser: " + std::string(e.what()), ERROR);
    }
}

void broadcastMessage(const json& message) {
    std::lock_guard<std::mutex> lock(game_mutex); // Ensure thread-safe access
    std::string compact = message.dump() + "\n";
    auto it = connected_sockets.begin();
    while (it != connected_sockets.end()) {
        try {
            if ((*it)->is_open()) {
                boost::asio::write(**it, boost::asio::buffer(compact));
                ++it;
            } else {
                eraseUser((*it)->native_handle());
                it = connected_sockets.erase(it);
            }
        } catch (...) {
            eraseUser((*it)->native_handle());
            it = connected_sockets.erase(it);
        }
    }
    game = game;
}

void handleRead(const boost::system::error_code& error, std::size_t bytes_transferred, boost::asio::streambuf& buffer, json& localPlayer, bool& initGameFully, bool& gameRunning, tcp::socket& socket) {
    if (!error) {
        std::istream input_stream(&buffer);
        std::string message;
        std::getline(input_stream, message);
        try {
            json messageJson = json::parse(message);

            if (messageJson.contains("quitGame") && messageJson["quitGame"].get<bool>()) {
                gameRunning = false;
                eraseUser(socket.native_handle());
            }
            if (messageJson.contains("getGame") && !initGameFully) {
                game = messageJson["getGame"];
                initGameFully = true;
            }
            if (messageJson.contains("name") && messageJson["local"].get<bool>()) {
                localPlayer = messageJson;
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            logToFile("JSON parse error: " + std::string(e.what()), ERROR);
            
        }

        buffer.consume(bytes_transferred); // Clear the buffer after reading

        boost::asio::async_read_until(socket, buffer, "\n", 
            [&buffer, &localPlayer, &initGameFully, &gameRunning, &socket](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                handleRead(ec, bytes_transferred, buffer, localPlayer, initGameFully, gameRunning, socket);
            });
    } else {
        std::cerr << "Read error: " << error.message() << std::endl;
        logToFile("Read error: " + error.message(), ERROR);
        eraseUser(socket.native_handle());
    }
}

void broadcastGameLocal(const json& game, tcp::socket& socket) {
    std::string compact = json({{"getGame", game}}).dump() + "\n";
    boost::asio::write(socket, boost::asio::buffer(compact));
}

void handleMessage(const std::string& message, tcp::socket& socket) {
    try {
        std::cout << "Server received: " << message << std::endl;
        json messageJson = json::parse(message);

        if (messageJson.contains("currentName")) {
            // Create new player
            std::string playerName = messageJson["currentName"];
            json newPlayer = createUser(playerName, socket.native_handle());
            game["room1"]["players"].push_back(newPlayer);
            
            // Send player data back to client
            json localPlayerData = newPlayer;
            localPlayerData["local"] = true;
            std::string response = localPlayerData.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(response));
            std::cout << "Sent local player data: " << response;
            
            // Broadcast updated game state to all clients
            json gameUpdate = {{"getGame", game}};
            broadcastMessage(gameUpdate);
        }
        else if (messageJson.contains("requestGame")) {
            // Send current game state
            json gameState = {{"getGame", game}};
            std::string response = gameState.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(response));
            std::cout << "Sent game state: " << response;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in handleMessage: " << e.what() << std::endl;
        logToFile("Error in handleMessage: " + std::string(e.what()), ERROR);
    }
    game = game;
}

void startReading(std::shared_ptr<tcp::socket> socket) {
    auto buffer = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(*socket, *buffer, "\n", [socket, buffer](boost::system::error_code ec, std::size_t) {
        if (!ec) {
            std::istream is(buffer.get());
            std::string message;
            std::getline(is, message);
            handleMessage(message, *socket);
            startReading(socket);
        } else {
            eraseUser(socket->native_handle());
            socket->close();
        }
    });
}

void acceptConnections(tcp::acceptor& acceptor) {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [socket, &acceptor](boost::system::error_code ec) {
        if (!ec) {
            std::lock_guard<std::mutex> lock(socket_mutex);
            connected_sockets.push_back(socket);
            std::cout << "New connection accepted!" << std::endl;
            startReading(socket);
            acceptConnections(acceptor);
        } else {
            logToFile("Error accepting connection: " + ec.message(), ERROR);
        }
    });
}

std::string getLocalIPAddress() {
    try {
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
        boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
        boost::asio::ip::tcp::resolver::iterator end;
        
        while (iter != end) {
            boost::asio::ip::tcp::endpoint ep = *iter++;
            if (ep.address().is_v4()) {  // Only get IPv4 addresses
                return ep.address().to_string();
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Could not get local IP: " << e.what() << std::endl;
        logToFile("Could not get local IP: " + std::string(e.what()), ERROR);
    }
    return "127.0.0.1";  // Fallback to localhost
}

void startServer(int port) {
    std::string localIP = getLocalIPAddress();
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    acceptConnections(acceptor);
    std::cout << "Server started on " << localIP << ":" << port << std::endl;
    logToFile("Server started on " + localIP + ":" + std::to_string(port), INFO);
    // Don't detach, let it run in the main thread
    io_context.run();
}

int main() {
    std::string currentWindow = "Server";
    int fps = getEnvVar<int>("FPS", 60);
    int screenWidth = getEnvVar<int>("SCREEN_WIDTH", 800);
    int screenHeight = getEnvVar<int>("SCREEN_HEIGHT", 450);
    int port = getEnvVar<int>("PORT", 5767);
    bool cli = getEnvVar<bool>("CLI", false);
    bool gameRunning = true;

    if (!cli) {
        InitWindow(screenWidth, screenHeight, "Server");
        SetTargetFPS(fps);
        
        // Run server in thread but don't detach
        std::thread serverThread(startServer, port);
        
        // Modify the input thread to check for window close
        boost::asio::io_context* io_context_ptr = &io_context;
        std::thread inputThread([&gameRunning, io_context_ptr] {
            std::string input;
            while (!shouldClose) {  // Check the atomic flag instead
                // Use non-blocking input checking or add a timeout
                if (WindowShouldClose()) {
                    shouldClose = true;
                    gameRunning = false;
                    io_context_ptr->stop();
                    break;
                }
                // ... rest of input handling ...
            }
        });
        Texture2D bg1Img = LoadTexture("/room1Bg.jpeg");
        Image bg1 = LoadImageFromTexture(bg1Img);
        std::string currentRoom = "room1";
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            if (currentWindow == "Server") {
                DrawText("This is the server window", 0, 0, 20, LIGHTGRAY);
                DrawText("Thank you for hosting a server", screenWidth / 2, screenHeight / 2 - 21, 15, BLACK);
                DrawText("Go to xterm window to use commands", screenWidth / 2, screenHeight / 2 + 21, 15, RED);
                DrawText("I don't know why but you have to close the xterm window to close", screenWidth / 2, screenHeight / 2 + 63, 15, RED);
                DrawRectangle(0, 100, screenWidth, 20, BLACK);
                DrawText("game view (if you press this you cannot go back to server window)", 0, 100, 20, WHITE);
                if (GetMousePosition().x > 0 && GetMousePosition().y > 100 && GetMousePosition().x < screenWidth && GetMousePosition().y < 120 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    InitWindow(screenWidth, screenHeight, "2D Viewport of Game");
                    currentWindow = "2D Viewport Of Game";
                }
            } else if (currentWindow == "2D Viewport Of Game") {
                if (currentRoom == "room1") {

                    for (auto& p : game["room1"]["players"]) {
                        DrawText(p["name"].get<std::string>().c_str(), p["x"].get<int>() + 10, p["y"].get<int>(), 20, BLACK);
                        DrawRectangle(p["x"].get<int>(), p["y"].get<int>(), 20, 20, RED);
                    }
                    DrawRectangle(0, 100, 50, 20, BLACK);
                    DrawText("room 2", 0, 100, 20, WHITE);
                    if (GetMousePosition().x > 0 && GetMousePosition().y > 100 && GetMousePosition().x < 50 && GetMousePosition().y < 120 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        currentRoom = "room2";
                    }
                } else if (currentRoom == "room2") {
                    for (auto& p : game["room2"]["players"]) {
                        DrawText(p["name"].get<std::string>().c_str(), p["x"].get<int>() + 10, p["y"].get<int>(), 20, BLACK);
                        DrawRectangle(p["x"].get<int>(), p["y"].get<int>(), 20, 20, RED);
                    }
                    DrawRectangle(0, 100, 50, 20, BLACK);
                    DrawText("room 1", 0, 100, 20, WHITE);
                    if (GetMousePosition().x > 0 && GetMousePosition().y > 100 && GetMousePosition().x < 50 && GetMousePosition().y < 120 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        currentRoom = "room1";
                    }
                }
            }
            EndDrawing();
            if (WindowShouldClose()) {
                shouldClose = true;
                gameRunning = false;
                io_context.stop();
                break;
            }
        }
        inputThread.join();
        
        // Join thread at end
        if (serverThread.joinable()) {
            serverThread.join();
        }
    } else {
        std::cout << "Starting server on port " << port << "...\n";
        std::thread serverThread(startServer, port);
        std::thread inputThread([&gameRunning] {
            std::string input;
            while (gameRunning) {
                std::getline(std::cin, input);
                if (input == "quit" || input == "^C") {
                    gameRunning = false;
                } else if (input == "kick") {
                    bool waitForKick = true;
                    std::cout << "Enter id to kick:";
                    for (auto& p : game["room1"]["players"]) {
                        std::cout << p["socket"].get<std::string>().c_str() << p["name"].get<std::string>().c_str() << std::endl;
                    }
                    for (auto& p : game["room2"]["players"]) {
                        std::cout << p["socket"].get<std::string>().c_str() << p["name"].get<std::string>().c_str() << std::endl;
                    }
                    std::thread kickThread([&waitForKick, &gameRunning] {
                        std::string input;
                        while (waitForKick) {
                            std::getline(std::cin, input);
                            if (input == "quit") {
                                gameRunning = false;
                                io_context.stop();
                            } else if (!input.empty()) {
                                for (auto& p : game["room1"]["players"]) {
                                    if (p["socket"].get<std::string>() == input) {
                                        p["quitGame"] = true;
                                        waitForKick = false;
                                    }
                                }
                                for (auto& p : game["room2"]["players"]) {
                                    if (p["socket"].get<std::string>() == input) {
                                        p["quitGame"] = true;
                                        waitForKick = false;
                                    }
                                }
                            }
                        }
                    });
                }
            }
        });
        inputThread.join();
        serverThread.join();
    }
    try{
        setupSignalHandlers(io_context); if (!cli) {CloseWindow();}
    } catch (const std::exception& e) {
        logToFile(std::string("ERROR: ") + e.what(), ERROR);
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
