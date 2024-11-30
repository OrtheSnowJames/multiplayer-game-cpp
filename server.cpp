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
#include <atomic>
#include <boost/asio/signal_set.hpp>
#include <chrono>
#include <filesystem>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using json = nlohmann::json;
using boost::asio::ip::tcp;
namespace fs = std::filesystem;

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;
typedef std::map<websocketpp::connection_hdl, int, std::owner_less<websocketpp::connection_hdl>> ConnectionMap;

WebSocketServer server;
ConnectionMap connections;
std::mutex connection_mutex;

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
        {"objects", json::array({
            {{"x", 123}, {"y", 144}, {"width", 228}, {"height", 60}, {"objID", 1}},
            {{"x", 350}, {"y", 159}, {"width", 177}, {"height", 74}, {"objID", 2}},
            {{"x", 524}, {"y", 162}, {"width", 205}, {"height", 60}, {"objID", 3}}
        })},
        {"enemies", {}}
    }}
};

json lookForPlayer(tcp::socket& socket) {
    int sockID;
    if (typeid(socket.native_handle()) != typeid(int)) {
        sockID = static_cast<int>(socket.native_handle());
    } else {
        sockID = socket.native_handle();
    }
    
    for (auto& room : game.items()) {  // Iterate through rooms
        if (room.value().contains("players")) {
            for (auto& player : room.value()["players"]) {
                if (player["socket"].get<int>() == sockID) {
                    return player;
                }
            }
        }
    }
    json error = {{"error", "player not found"}};
    return error;
}

std::string lookForRoom(tcp::socket& socket) {
    int sockID;
    if (typeid(socket.native_handle()) != typeid(int)) {
        sockID = static_cast<int>(socket.native_handle());
    } else {
        sockID = socket.native_handle();
    }
    
    for (auto& room : game.items()) {  // Iterate through rooms
        if (room.value().contains("players")) {
            for (auto& player : room.value()["players"]) {
                if (player["socket"].get<int>() == sockID) {
                    return room.key();  // Return room name as string
                }
            }
        }
    }
    return "room1";  // Default room
}

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

json createUser(const std::string& name, int id) {
    int fid;
    if (typeid(id) != typeid(int)) {
        std::cout << "probably on windows" << std::endl;
        id = static_cast<int>(id);
        fid = id;
    }
    else {
        fid = id;
    }
    std::random_device rd;
    json newPlayer = {
        {"name", name},
        {"x", rd() % 600},
        {"y", rd() % 300},
        {"speed", 5},
        {"score", 0},
        {"width", 64}, 
        {"height", 64},
        {"inventory", {{"shields", 0}, {"bananas", 0}}},
        {"socket", fid},
        {"spriteState", 1},
        {"skin", 1},
        {"local", false},
        {"room", 1}
    };
    for (auto& o : game["room1"]["objects"]) {
        if (checkCollision(newPlayer, o)) {
            newPlayer = createUser(name, id);  // recreate player if colliding
        }
    }
    return newPlayer;
}

// Modify eraseUser function
void eraseUser(int id) {
    std::lock_guard<std::mutex> lock(game_mutex);
    try {
        // First remove from connected_sockets SAFELY AHEM AHEM
        {
            std::lock_guard<std::mutex> socket_lock(socket_mutex);
            auto it = std::remove_if(connected_sockets.begin(), connected_sockets.end(),
                [id](const std::shared_ptr<tcp::socket>& socket) {
                    return socket && socket->native_handle() == id;
                });
            connected_sockets.erase(it, connected_sockets.end());
        }

        // Then remove from game rooms
        for (auto& room : game.items()) {
            if (room.value().contains("players")) {
                auto& players = room.value()["players"];
                players.erase(
                    std::remove_if(players.begin(), players.end(),
                        [id](const json& p) { 
                            return p.contains("socket") && p["socket"].get<int>() == id; 
                        }
                    ),
                    players.end()
                );
            }
        }
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

void handleMessage(const std::string& message, tcp::socket& socket) {
    try {
        json messageJson = json::parse(message);
        int sockID;
        if (typeid(socket.native_handle()) != typeid(int)) {
            sockID = static_cast<int>(socket.native_handle());
        } else {
            sockID = socket.native_handle();
        }
        
        // Initial connection - only time we send full game state
        if (messageJson.contains("currentName")) {
            std::string name = messageJson["currentName"].get<std::string>();
            json newPlayer = createUser(name, sockID);
            newPlayer["local"] = true;
            
            game["room1"]["players"].push_back(newPlayer);
            
            // Send only local player data first
            json localResponse = newPlayer;
            boost::asio::write(socket, boost::asio::buffer(localResponse.dump() + "\n"));

            // Then send full game state once
            json gameUpdate = {{"getGame", game}};
            boost::asio::write(socket, boost::asio::buffer(gameUpdate.dump() + "\n"));
            return;
        }

        // Modify handleMessage for quit game
        if (messageJson.contains("quitGame") && messageJson["quitGame"].get<bool>()) {
            try {
                int socketId = socket.native_handle();
                eraseUser(socketId);
                json gameUpdate = {{"getGame", game}};
                broadcastMessage(gameUpdate);
            } catch (const std::exception& e) {
                std::cerr << "Error handling quit game: " << e.what() << std::endl;
                logToFile("Error handling quit game: " + std::string(e.what()), ERROR);
            }
            return;
        }

        if (messageJson.contains("x") || messageJson.contains("y") || messageJson.contains("spriteState")) {
            std::string roomName = lookForRoom(socket);
            bool changed = false;
            int newX = messageJson.value("x", -1);
            int newY = messageJson.value("y", -1);
            int spriteState = messageJson.value("spriteState", 1);  // Get sprite state with default
            
            for (auto& p : game[roomName]["players"]) {
                if (p["socket"].get<int>() == sockID) {
                    if (messageJson.contains("spriteState")) {
                        p["spriteState"] = spriteState;
                        changed = true;
                    }
                    if (messageJson.contains("x")) {
                        p["x"] = newX;
                        changed = true;
                    }
                    if (messageJson.contains("y")) {
                        p["y"] = newY;
                        changed = true;
                    }
                    break;
                }
            }

            if (changed) {
                bool crouched = (spriteState == 5);
                int widthToSet = crouched ? 48 : 32;  // Use consistent sizes
                int heightToSet = crouched ? 48 : 32;
                
                json positionUpdate = {
                    {"updatePosition", {
                        {"socket", socket.native_handle()},
                        {"x", newX},
                        {"y", newY},
                        {"width", widthToSet},
                        {"height", heightToSet},
                        {"spriteState", spriteState}
                    }}
                };
                broadcastMessage(positionUpdate);
            }
        }
        // Only send full game state when explicitly requested
        if (messageJson.contains("requestGame") && !messageJson.contains("x") && !messageJson.contains("y")) {
            json gameUpdate = {{"getGame", game}};
            boost::asio::write(socket, boost::asio::buffer(gameUpdate.dump() + "\n"));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in handleMessage: " << e.what() << std::endl;
        logToFile("Error in handleMessage: " + std::string(e.what()), ERROR);
    }
}

void handleRead(const boost::system::error_code& error, std::size_t bytes_transferred, boost::asio::streambuf& buffer, tcp::socket& socket) {
    if (!error) {
        std::istream input_stream(&buffer);
        std::string message;
        std::getline(input_stream, message);
        handleMessage(message, socket);
        buffer.consume(bytes_transferred); // Clear the buffer after reading

        boost::asio::async_read_until(socket, buffer, "\n", 
            [&buffer, &socket](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                handleRead(ec, bytes_transferred, buffer, socket);
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

int nextId = 0;
int getNextId() {
    return ++nextId;
}

void on_message(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
    try {
        json messageJson = json::parse(msg->get_payload());
        // Process message and create response
        json response = {{"status", "received"}};
        
        // Use WebSocket send instead of boost::asio::write
        server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        std::cerr << "Error in message handler: " << e.what() << std::endl;
    }
}

void on_open(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    int id = getNextId(); 
    connections[hdl] = id;
}

void on_close(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connection_mutex);
    int id = connections[hdl];
    connections.erase(hdl);
    eraseUser(id);
}

// Define WebSocket server types
typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;
WebSocketServer wsServer;
std::mutex wsConnectionMutex;
std::map<websocketpp::connection_hdl, int, std::owner_less<websocketpp::connection_hdl>> wsConnections;

// Add WebSocket handlers
void on_ws_message(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
    try {
        // Use existing handleMessage logic but adapt for WebSocket
        std::string payload = msg->get_payload();
        json messageJson = json::parse(payload);
        // Handle message same as TCP but use WebSocket send
        wsServer.send(hdl, messageJson.dump(), msg->get_opcode());
    } catch (const std::exception& e) {
        std::cerr << "WebSocket message error: " << e.what() << std::endl;
    }
}

int main() { 
    std::string currentWindow = "Server";
    int fps = getEnvVar<int>("FPS", 60);
    int screenWidth = getEnvVar<int>("SCREEN_WIDTH", 800);
    int screenHeight = getEnvVar<int>("SCREEN_HEIGHT", 450);
    int port = getEnvVar<int>("PORT", 5767);
    bool cli = getEnvVar<bool>("CLI", false);
    bool gameRunning = true;
    fs::path root = fs::current_path();
    fs::path assets = "assets"; // Define the assets path
    fs::path imgPath = root / assets;
    fs::path bg1ImgPath = imgPath / "room1Bg.png";
    std::cout << "cli var is: " << cli << std::endl;

    Texture2D bg1Img; // Declare bg1Img as Texture2D
    if (cli == false) {
        InitWindow(screenWidth, screenHeight, "Server");
        SetTargetFPS(fps);
        
        // Run server in thread but don't detach
        std::thread serverThread(startServer, port);
        
        // Modify the input thread to check for window close
        boost::asio::io_context* io_context_ptr = &io_context;
        std::thread inputThread([&gameRunning] {
            std::string input;
            while (gameRunning) {
                std::getline(std::cin, input);
                if (input == "quit" || input == "^C") {
                    gameRunning = false;
                } else if (input == "kick") {
                    bool waitForKick = true;
                    std::cout << "Enter id to kick:";
                    // Fix: Use get<int> for socket IDs
                    for (auto& p : game["room1"]["players"]) {
                        std::cout << p["socket"].get<int>() << " - " << p["name"].get<std::string>() << std::endl;
                    }
                    for (auto& p : game["room2"]["players"]) {
                        std::cout << p["socket"].get<int>() << " - " << p["name"].get<std::string>() << std::endl;
                    }
                    std::thread kickThread([&waitForKick, &gameRunning] {
                        std::string input;
                        while (waitForKick) {
                            std::getline(std::cin, input);
                            if (input == "quit") {
                                gameRunning = false;
                                io_context.stop();
                            } else if (!input.empty()) {
                                try {
                                    int kickId = std::stoi(input);
                                    for (auto& room : game.items()) {  // Iterate through all rooms
                                        if (room.value().contains("players")) {
                                            auto& players = room.value()["players"];
                                            players.erase(
                                                std::remove_if(players.begin(), players.end(),
                                                    [kickId](const json& p) {
                                                        return p["socket"].get<int>() == kickId;
                                                    }
                                                ),
                                                players.end()
                                            );
                                        }
                                    }
                                    waitForKick = false;
                                } catch (const std::exception& e) {
                                    std::cout << "Invalid ID format" << std::endl;
                                }
                            }
                        }
                    });
                }
            }
        });
            if (fs::exists(bg1ImgPath)) {
        bg1Img = LoadTexture(bg1ImgPath.string().c_str());
        if (bg1Img.id == 0) {
            std::cerr << "Failed to load background texture" << std::endl;
            logToFile("Failed to load background texture", ERROR);
        }
        } else {
            std::cerr << "Background image not found at: " << bg1ImgPath << std::endl;
            logToFile("Background image not found", ERROR);
        }
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
                    if (bg1Img.id != 0) DrawTexture(bg1Img, 0, 0, WHITE);
                    for (auto& p : game["room1"]["players"]) {
                        DrawText(p["name"].get<std::string>().c_str(), p["x"].get<int>() + 10, p["y"].get<int>(), 20, WHITE);
                        DrawRectangle(p["x"].get<int>(), p["y"].get<int>(), 20, 20, RED);
                    }
                    DrawRectangle(0, 100, 50, 20, BLACK);
                    DrawText("room 2", 0, 100, 20, WHITE);
                    if (GetMousePosition().x > 0 && GetMousePosition().y > 100 && GetMousePosition().x < 50 && GetMousePosition().y < 120 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        currentRoom = "room2";
                    }
                } else if (currentRoom == "room2") {
                    for (auto& p : game["room2"]["players"]) {
                        DrawText(p["name"].get<std::string>().c_str(), p["x"].get<int>() + 10, p["y"].get<int>(), 20, WHITE);
                        DrawRectangle(p["x"].get<int>(), p["y"].get<int>(), 20, 20, RED);
                    }
                    DrawRectangle(0, 100, 50, 20, BLACK);
                    DrawText("room 1", 0, 100, 20, WHITE);
                    if (GetMousePosition().x > 0 && GetMousePosition().y > 100 && GetMousePosition().y < 120 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        // CLI mode
        std::cout << "Starting server on port " << port << "...\n";
        std::thread serverThread(startServer, port);
        std::thread inputThread([&gameRunning] {
        std::string input;
        while (gameRunning) {
            std::getline(std::cin, input);
            if (input == "quit" || input == "^C") {
                gameRunning = false;
                json quitMessage = {{"quitGame", true}};
                broadcastMessage(quitMessage.dump());
                io_context.stop();
                break;
            } else if (input == "kick") {
                std::cout << "Current players:\n";
                {
                    std::lock_guard<std::mutex> lock(game_mutex);
                    // Show players from all rooms
                    for (const auto& room : game.items()) {
                        if (room.value().contains("players")) {
                            for (const auto& player : room.value()["players"]) {
                                std::cout << player["socket"].get<int>() << " - " 
                                        << player["name"].get<std::string>() << std::endl;
                            }
                        }
                        else {
                            std::cout << "No players in room " << room.key() << std::endl;
                        }
                    }
                }

                std::cout << "Enter player ID to kick: ";
                std::string kickInput;
                std::getline(std::cin, kickInput);

                try {
                    int kickId = std::stoi(kickInput);
                    {
                        std::lock_guard<std::mutex> lock(game_mutex);
                        // Remove player from all rooms
                        for (auto& room : game.items()) {
                            if (room.value().contains("players")) {
                                auto& players = room.value()["players"];
                                auto it = std::remove_if(players.begin(), players.end(),
                                    [kickId](const json& p) {
                                        return p["socket"].get<int>() == kickId;
                                    });
                                if (it != players.end()) {
                                    json kickedPlayer = *it;
                                    int sockid = kickedPlayer["socket"].get<int>();
                                    for (auto& p : connected_sockets) {
                                        if (p->native_handle() == sockid) {
                                            boost::asio::write(*p, boost::asio::buffer("quitGame\n"));
                                            break;
                                        }
                                    }
                                    players.erase(it, players.end());
                                    // Broadcast updated game state
                                    json gameUpdate = {{"getGame", game}};
                                    broadcastMessage(gameUpdate);
                                    std::cout << "Player kicked successfully\n";
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "Invalid player ID\n";
                }
            }
        }
    });
        inputThread.join();
        serverThread.join();
    }
    try {
        setupSignalHandlers(io_context);
        if (!cli) {
            CloseWindow();
        }
        
        
        // Create a local WebSocket server instance
        WebSocketServer* wsServer = new WebSocketServer();
        
        // Initialize WebSocket server
        wsServer->set_access_channels(websocketpp::log::alevel::all);
        wsServer->clear_access_channels(websocketpp::log::alevel::frame_payload);
        wsServer->init_asio();

        // Set handlers with proper namespace
        using websocketpp::lib::placeholders::_1;
        using websocketpp::lib::placeholders::_2;
        wsServer->set_message_handler(websocketpp::lib::bind(&on_message, _1, _2));
        wsServer->set_open_handler(websocketpp::lib::bind(&on_open, _1));
        wsServer->set_close_handler(websocketpp::lib::bind(&on_close, _1));

        // Start WebSocket server
        wsServer->listen(5767);
        wsServer->start_accept();

        // Run the server in its own thread
        std::thread wsThread([wsServer]() {
            wsServer->run();
        });

        // Wait for server thread to finish
        if (wsThread.joinable()) {
            wsThread.join();
        }
        
        delete wsServer;
        return 0;
        
    } catch (const std::exception& e) {
        logToFile(std::string("ERROR: ") + e.what(), ERROR);
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
}
