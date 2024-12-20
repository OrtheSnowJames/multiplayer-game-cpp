#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <random>
#include <vector>
#include <set>
#include <mutex>
#include <cstdlib>
#include <sstream>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <algorithm>
#include <string> // For std::to_string

#include "libs/enemy.hpp"
#include "coolfunctions.hpp"
#include <websocketpp/server.hpp>
#include <boost/asio/signal_set.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#ifdef _WIN32
#include <winsock2.h>
using SocketHandle = SOCKET;
#else
using SocketHandle = int;
#endif

using json = nlohmann::json;
using boost::asio::ip::tcp;

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;

std::atomic<bool> shouldClose{false};
std::mutex socket_mutex;
std::mutex game_mutex;
boost::asio::io_context io_context;

std::vector<std::shared_ptr<tcp::socket>> connected_sockets;

json game = {
    {"room1", {
        {"roomID", 1},
        {"players", json::array()},
        {"objects", json::array({
            {{"x", 123}, {"y", 144}, {"width", 228}, {"height", 60}, {"objID", 1}},
            {{"x", 350}, {"y", 159}, {"width", 177}, {"height", 74}, {"objID", 2}},
            {{"x", 524}, {"y", 162}, {"width", 205}, {"height", 60}, {"objID", 3}}
        })},
        {"enemies", json::array()}
    }},
    {"room2", {
        {"roomID", 2},
        {"players", json::array()},
        {"objects", json::array({
            {{"x", 410}, {"y", 0}, {"width", 93}, {"height", 286}, {"objID", 4}}
        })},
        {"enemies", json::array()}
    }}
};

int enemyNewId = 0;

int castWinsock(tcp::socket& socket) {
    auto nativeHandle = socket.native_handle();
    int intHandle;
#ifdef _WIN32
    intHandle = static_cast<int>(reinterpret_cast<intptr_t>(nativeHandle));
#else
    intHandle = nativeHandle;
#endif
    return intHandle;
}

enum LogLevel { INFO, ERROR, DEBUG };
void logToFile(const std::string& message, LogLevel level = INFO) {
    static const std::map<LogLevel, std::string> levelNames = {
        {INFO, "INFO"}, {ERROR, "ERROR"}, {DEBUG, "DEBUG"}};

    std::ofstream logFile("err.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::string timeStr = std::ctime(&now_time);
    timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());

    logFile << "[" << levelNames.at(level) << "] " << timeStr << " " << message << std::endl;
}

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

bool findPlayer(const std::string& name) {
    for (auto& room : game.items()) {
        if (room.value().contains("players")) {
            for (auto& player : room.value()["players"]) {
                if (player["name"] == name) {
                    return true;
                }
            }
        }
    }
    return false;
}

WebSocketServer wss;
std::vector<websocketpp::connection_hdl> ws_connections;
std::mutex ws_mutex;

// Forward declarations
void eraseUser(int id);

// Function to send messages over WebSockets
void sendWebSocketMessage(websocketpp::connection_hdl hdl, const std::string& message) {
    try {
        wss.send(hdl, message, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        logToFile("WebSocket send error: " + std::string(e.what()), ERROR);
    }
}


void broadcastMessage(const json& message) {
    std::string compact = message.dump() + "\n";
    std::vector<int> invalidSockets;

        std::lock_guard<std::mutex> lock(socket_mutex);
        for (const auto& socket : connected_sockets) {
            try {
                if (socket && socket->is_open()) {
                    boost::system::error_code ec;
                    boost::asio::write(*socket, boost::asio::buffer(compact), ec);
                    if (ec) {
                        invalidSockets.push_back(castWinsock(*socket));
                    }
                } else if (socket) {
                    invalidSockets.push_back(castWinsock(*socket));
                }
            } catch (const std::exception& e) {
                if (socket) {
                    invalidSockets.push_back(castWinsock(*socket));
                }
                logToFile("Error broadcasting TCP message: " + std::string(e.what()), ERROR);
            }
    }

    // WebSocket broadcast
    {
        std::lock_guard<std::mutex> lock(ws_mutex);
        auto it = ws_connections.begin();
        while (it != ws_connections.end()) {
            try {
                if (wss.get_con_from_hdl(*it)->get_state() == websocketpp::session::state::open) {
                    sendWebSocketMessage(*it, compact);
                    ++it;
                } else {
                    it = ws_connections.erase(it);
                }
            } catch (...) {
                it = ws_connections.erase(it);
            }
        }
    }

    // Clean up invalid TCP sockets
    for (int socketId : invalidSockets) {
        eraseUser(socketId);
    }
}

json createUserRaw(const std::string& name, int fid) {
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
            return createUserRaw(name, fid); // recreate if colliding
        }
    }
    return newPlayer;
}

json createUser(const std::string& name, tcp::socket& socket) {
    int fid = castWinsock(socket);
    return createUserRaw(name, fid);
}

json createEnemy(int room) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> x_dis(50, 550);
    std::uniform_int_distribution<> y_dis(50, 250);
    
    json newEnemy = {
        {"x", x_dis(gen)},
        {"y", y_dis(gen)},
        {"width", 64},
        {"height", 64},
        {"room", room},
        {"speed", 5},
        {"id", enemyNewId++}
    };
    for (auto& e : game["room" + std::to_string(room)]["enemies"]) {
        if (checkCollision(newEnemy, e)) {
            return createEnemy(room);
        }
    }
    return newEnemy;
}

void eraseUser(int id) {
    try {
        // First send quit message to the disconnecting player
        {
            std::lock_guard<std::mutex> socket_lock(socket_mutex);
            auto it = std::find_if(connected_sockets.begin(), connected_sockets.end(),
                [id](const std::shared_ptr<tcp::socket>& socket) {
                    return socket && castWinsock(*socket) == id;
                });
                
            if (it != connected_sockets.end() && (*it)->is_open()) {
                try {
                    json quitMessage = {{"quitGame", true}};
                    boost::asio::write(**it, boost::asio::buffer(quitMessage.dump() + "\n"));
                } catch (...) {
                    // Ignore write errors during disconnect
                }
            }
        }

        // Remove from game state
        {
            std::lock_guard<std::mutex> lock(game_mutex);
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
        }

        // Clean up socket and remove from connected_sockets
        {
            std::lock_guard<std::mutex> socket_lock(socket_mutex);
            auto it = std::find_if(connected_sockets.begin(), connected_sockets.end(),
                [id](const std::shared_ptr<tcp::socket>& socket) {
                    return socket && castWinsock(*socket) == id;
                });
                
            if (it != connected_sockets.end()) {
                if ((*it)->is_open()) {
                    boost::system::error_code ec;
                    (*it)->shutdown(tcp::socket::shutdown_both, ec);
                    (*it)->close(ec);
                }
                connected_sockets.erase(it);
            }
        }

        // Notify remaining players about the disconnection
        json playerLeftMessage = {{"playerLeft", id}};
        broadcastMessage(playerLeftMessage);
        
        logToFile("User " + std::to_string(id) + " disconnected and removed successfully", INFO);
    } catch (const std::exception& e) {
        logToFile("Error in eraseUser: " + std::string(e.what()), ERROR);
    }
}

std::string lookForRoom(tcp::socket& socket) {
    int sockID = castWinsock(socket);
    for (auto& room : game.items()) {
        if (room.value().contains("players")) {
            for (auto& player : room.value()["players"]) {
                if (player["socket"].get<int>() == sockID) {
                    return room.key();
                }
            }
        }
    }
    return "room1";
}

json lookForPlayer(tcp::socket& socket) {
    int sockID = castWinsock(socket);
    for (auto& room : game.items()) {
        if (room.value().contains("players")) {
            for (auto& player : room.value()["players"]) {
                if (player["socket"].get<int>() == sockID) {
                    return player;
                }
            }
        }
    }
    return {{"error", "player not found"}};
}

void printCurrentPlayers() {
    std::cout << "Current players:\n";
    for (auto& room : game.items()) {
        if (room.value().contains("players")) {
            for (auto& player : room.value()["players"]) {
                // Print player's name and socket (instead of id which doesn't exist)
                std::cout << player["name"].get<std::string>() << " - " 
                          << player["socket"].get<int>() << std::endl;
            }
        }
    }
}

void sendWebSocketMessage(websocketpp::connection_hdl, const std::string&); // forward declaration if needed

void onWebSocketMessage(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg);

void onWebSocketOpen(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(ws_mutex);
    ws_connections.push_back(hdl);
    std::cout << "New WebSocket connection!" << std::endl;
}

void onWebSocketClose(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(ws_mutex);
    ws_connections.erase(
        std::remove_if(ws_connections.begin(), ws_connections.end(),
            [hdl](websocketpp::connection_hdl h) { return !h.owner_before(hdl) && !hdl.owner_before(h); }),
        ws_connections.end()
    );
}

void switchRoom(json& player, const std::string& newRoom) {
    std::lock_guard<std::mutex> lock(game_mutex);
    for (auto& room : game.items()) {
        if (room.value().contains("players")) {
            auto& players = room.value()["players"];
            auto it = std::remove_if(players.begin(), players.end(),
                [&player](const json& p) {
                    return p["socket"] == player["socket"];
                });
            players.erase(it, players.end());
        }
    }
    game[newRoom]["players"].push_back(player);
    json updateMessage = {{"switchRoom", {{"socket", player["socket"]}, {"room", std::stoi(newRoom.substr(4))}}}};
    broadcastMessage(updateMessage);
}

bool playersInRoom(const std::string& roomName) {
    if (!game.contains(roomName)) {
        return false;
    }
    return game[roomName].contains("players") && !game[roomName]["players"].empty();
}

// Shield logic
json createShield() {
    std::random_device rd;
    return {
        {"x", rd() % 600},
        {"y", rd() % 300},
        {"width", 32},
        {"height", 32},
        {"objID", 5}
    };
}

bool shieldExists() {
    for (auto& o : game["room2"]["objects"]) {
        if (o["objID"] == 5) {
            return true;
        }
    }
    return false;
}

void handleMessage(const std::string& message, tcp::socket& socket);

void shieldThread() {
    while (!shouldClose) {
        if (playersInRoom("room2") && !shieldExists()) {
            {
                std::lock_guard<std::mutex> lock(game_mutex);
                game["room2"]["objects"].push_back(createShield());
                json gameUpdate = {{"getGame", game}};
                broadcastMessage(gameUpdate);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void onWebSocketMessage(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
    try {
        // Create a dummy TCP socket for compatibility with existing code
        tcp::socket dummy_socket(io_context);
        handleMessage(msg->get_payload(), dummy_socket);
    } catch (const std::exception& e) {
        logToFile("WebSocket message handling error: " + std::string(e.what()), ERROR);
    }
}

void handleMessage(const std::string& message, tcp::socket& socket) {
    try {
        json messageJson = json::parse(message);
        int sockID = castWinsock(socket);
        
        // Initial connection
        if (messageJson.contains("currentName")) {
            std::string requestedName = messageJson["currentName"].get<std::string>();
            std::string name = requestedName;
            if (findPlayer(requestedName)) {
                // If name exists, find a unique variant
                for (int i = 1; i < 100; i++) {
                    std::string newName = requestedName + std::to_string(i);
                    if (!findPlayer(newName)) {
                        name = newName;
                        break;
                    }
                }
            }

            json newPlayer = createUser(name, socket);
            newPlayer["local"] = true;
            {
                std::lock_guard<std::mutex> lock(game_mutex);
                game["room1"]["players"].push_back(newPlayer);
            }

            json localResponse = newPlayer;
            boost::asio::write(socket, boost::asio::buffer(localResponse.dump() + "\n"));

            json gameUpdate = {{"getGame", game}};
            boost::asio::write(socket, boost::asio::buffer(gameUpdate.dump() + "\n"));
            return;
        }

        if (messageJson.contains("quitGame") && messageJson["quitGame"].get<bool>()) {
            eraseUser(socket.native_handle());
            json gameUpdate = {{"getGame", game}};
            broadcastMessage(gameUpdate);
            return;
        }

        if (messageJson.contains("x") || messageJson.contains("y") || messageJson.contains("spriteState")) {
            std::string roomName = lookForRoom(socket);
            bool changed = false;
            int newX = messageJson.value("x", -1);
            int newY = messageJson.value("y", -1);
            int spriteState = messageJson.value("spriteState", 1);

            {
                std::lock_guard<std::mutex> lock(game_mutex);
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
            }

            if (messageJson.contains("room") && lookForPlayer(socket)["room"].get<int>() != messageJson["room"].get<int>()) {
                json player = lookForPlayer(socket);
                std::string newRoomName = "room" + std::to_string(messageJson["room"].get<int>());
                {
                    std::lock_guard<std::mutex> lock(game_mutex);
                    player["room"] = messageJson["room"].get<int>();
                    if (player["room"].get<int>() == 1) {player["x"] = 90; player["y"] = 90;}
                    else {player["x"] = 100; player["y"] = 100;}

                    if (!game.contains(newRoomName)) {
                        game[newRoomName] = {
                            {"players", json::array()},
                            {"objects", json::array()},
                            {"enemies", json::array()}
                        };
                    }

                    // Remove from old room
                    std::string oldRoomName = roomName;
                    auto& oldRoomPlayers = game[oldRoomName]["players"];
                    oldRoomPlayers.erase(
                        std::remove_if(oldRoomPlayers.begin(), oldRoomPlayers.end(),
                            [&socket](const json& p) { 
                                return p["socket"] == castWinsock(socket); 
                            }
                        ),
                        oldRoomPlayers.end()
                    );

                    // Add to new room
                    game[newRoomName]["players"].push_back(player);
                }

                json gameUpdate = {{"getGame", game}};
                broadcastMessage(gameUpdate);
                changed = true;
            }

            if (changed) {
                bool crouched = (spriteState == 5);
                int widthToSet = crouched ? 48 : 32;
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

        if (messageJson.contains("requestGame") && !messageJson.contains("x") && !messageJson.contains("y")) {
            json gameUpdate = {{"getGame", game}};
            boost::asio::write(socket, boost::asio::buffer(gameUpdate.dump() + "\n"));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in handleMessage: " << e.what() << std::endl;
        logToFile(std::string("Error in handleMessage: ") + e.what(), ERROR);
    }
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
            eraseUser(castWinsock(*socket));
            socket->close();
        }
    });
}

void acceptConnections(tcp::acceptor& acceptor) {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [socket, &acceptor](boost::system::error_code ec) {
        if (!ec) {
            {
                std::lock_guard<std::mutex> lock(socket_mutex);
                connected_sockets.push_back(socket);
            }
            std::cout << "New connection accepted!" << std::endl;
            startReading(socket);
            acceptConnections(acceptor);
        } else {
            logToFile("Error accepting connection: " + ec.message(), ERROR);
        }
    });
}

std::atomic<bool> isShuttingDown{false};
std::condition_variable shutdownCV;
std::mutex shutdownMutex;

void cleanup() {
    {
        std::unique_lock<std::mutex> lock(shutdownMutex);
        isShuttingDown = true;
        shouldClose = true;
    }
    shutdownCV.notify_all();
    
    // Give threads time to cleanup
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Close all sockets
    {
        std::lock_guard<std::mutex> lock(socket_mutex);
        for (auto& socket : connected_sockets) {
            try {
                if (socket && socket->is_open()) {
                    json quitMessage = {{"quitGame", true}};
                    boost::asio::write(*socket, boost::asio::buffer(quitMessage.dump() + "\n"));
                    socket->shutdown(tcp::socket::shutdown_both);
                    socket->close();
                }
            } catch (const std::exception& e) {
                logToFile("Socket cleanup error: " + std::string(e.what()), ERROR);
            }
        }
        connected_sockets.clear();
    }
    
    // Clear game state
    {
        std::lock_guard<std::mutex> game_lock(game_mutex);
        game = json::object();
    }
    
    logToFile("Server cleanup completed", INFO);
}

void heartbeatThread() {
    const int HEARTBEAT_INTERVAL = 5000; // 5 seconds
    
    while (!shouldClose) {
        std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL));
        
        std::lock_guard<std::mutex> lock(socket_mutex);
        auto it = connected_sockets.begin();
        
        while (it != connected_sockets.end()) {
            try {
                if ((*it)->is_open()) {
                    json heartbeat = {{"type", "heartbeat"}};
                    boost::asio::write(**it, boost::asio::buffer(heartbeat.dump() + "\n"));
                    ++it;
                } else {
                    eraseUser(castWinsock(**it));
                    it = connected_sockets.erase(it);
                }
            } catch (...) {
                eraseUser(castWinsock(**it));
                it = connected_sockets.erase(it);
            }
        }
    }
}

void enemyThread() {
    std::cout << "Enemy thread started" << std::endl;
    logToFile("Enemy thread started", INFO);
    while (!shouldClose) { 
        try {
            for (int i = 0; i < 10 && !shouldClose; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            {
                std::lock_guard<std::mutex> lock(game_mutex);
                if (!game["room2"]["players"].empty()) {
                    if (game["room2"]["enemies"].size() < 4) {
                        json newEnemy = createEnemy(2);
                        game["room2"]["enemies"].push_back(newEnemy);

                        json message = {{"getEnemy", newEnemy}};
                        broadcastMessage(message);
                    }
                }
                //update enemies
                for (auto& enemy : game["room2"]["enemies"]) {
                    json beforeenemy = enemy;
                    updateEnemy(game["room2"]["players"], enemy);
                    json afterenemy = enemy;
                    json message = {{"updateEPosition", true},
                    {"x", enemy["x"]},
                    {"y", enemy["y"]},
                    {"width", enemy["width"]},
                    {"height", enemy["height"]},
                    {"enemyId", enemy["id"]}};
                    broadcastMessage(message);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in enemy thread: " << e.what() << std::endl;
            logToFile(std::string("Error in enemy thread: ") + e.what(), ERROR);
        }
    }
    std::cout << "Enemy thread stopping" << std::endl;
    logToFile("Enemy thread stopping", INFO);
}

std::shared_ptr<tcp::socket> getSocketFromId(int socketId) {
    std::lock_guard<std::mutex> lock(socket_mutex);
    auto it = std::find_if(connected_sockets.begin(), connected_sockets.end(),
        [socketId](const std::shared_ptr<tcp::socket>& s) {
            return s && s->is_open() && castWinsock(*s) == socketId;
        });
    if (it != connected_sockets.end()) {
        return *it;
    }
    return nullptr;
}

void startServer(int port) {
    tcp::acceptor acceptor(io_context);
    
    try {
        acceptor.open(tcp::v4());
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        acceptor.set_option(tcp::acceptor::keep_alive(true));
        
        // Bind to any address (0.0.0.0)
        tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), port);
        boost::system::error_code ec;
        
        std::cout << "Attempting to bind to 0.0.0.0:" << port << std::endl;
        acceptor.bind(endpoint, ec);
        
        if (ec) {
            std::cerr << "Bind error: " << ec.message() << std::endl;
            throw std::runtime_error("Failed to bind to port " + std::to_string(port) + ": " + ec.message());
        }
        
        acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            throw std::runtime_error("Failed to listen: " + ec.message());
        }

        auto localEndpoint = acceptor.local_endpoint(ec);
        std::cout << "Server is listening on " << localEndpoint.address().to_string() 
                  << ":" << localEndpoint.port() << std::endl;
        
        // Start accepting connections
        acceptConnections(acceptor);
        
        // Keep io_context running
        io_context.run();
        
    } catch (const std::exception& e) {
        logToFile("Server startup failed: " + std::string(e.what()), ERROR);
        throw;
    }
}

void kickPlayer(int playerId) {
    std::shared_ptr<tcp::socket> socket = getSocketFromId(playerId);
    if (socket) {
        json leaveMessage = {{"quitGame", true}};
        json othermessage = {{"playerLeft", playerId}};
        boost::asio::write(*socket, boost::asio::buffer(leaveMessage.dump() + "\n"));
        socket->close();
        broadcastMessage(othermessage);
        eraseUser(playerId);
    } else {
        std::cout << "Player with ID " << playerId << " not found" << std::endl;
    }
}

void cliThread() {
    bool expectingKickId = false;  // tracks if we're waiting for a second input for 'kick' command

    while (!isShuttingDown && !shouldClose) {
        // Print main prompt only if not expecting a second response
        if (!expectingKickId) {
            std::cout << "> " << std::flush;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout);
        if (ready <= 0) {
            if (isShuttingDown || shouldClose) break;
            continue; 
        }

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            expectingKickId = false;
            continue;
        }

        if (expectingKickId) {
            if (input.empty()) {
                std::cout << "No input provided. Kick cancelled.\n";
            } else {
                try {
                    int kickId = std::stoi(input);
                    kickPlayer(kickId);
                    std::cout << "Attempted to kick player with ID: " << kickId << "\n";
                } catch (...) {
                    std::cout << "Invalid player ID. Kick cancelled.\n";
                }
            }
            expectingKickId = false; 
            continue;
        }

        if (input == "quit" || input == "^C") {
            {
                std::lock_guard<std::mutex> lock(shutdownMutex);
                isShuttingDown = true;
                shouldClose = true;
            }
            shutdownCV.notify_all();
            broadcastMessage({{"quitGame", true}});
            break;

        } else if (input == "kick") {
            printCurrentPlayers();
            std::cout << "Enter player ID to kick: " << std::flush;
            expectingKickId = true; 

        } else if (input == "game") {
            std::lock_guard<std::mutex> lock(game_mutex);
            std::cout << "\n=== CURRENT GAME STATE ===\n";
            std::cout << game.dump(2) << std::endl; 
            std::cout << "========================\n\n";

        } else if (!input.empty()) {
            std::cout << "Unknown command: " << input << "\n";
        }
    }
}

void setupSignalHandlers() {
    static boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    
    signals.async_wait([](const boost::system::error_code& error, int signal_number) {
        if (!error) {
            std::cout << "\nReceived signal " << signal_number << ", initiating graceful shutdown..." << std::endl;
            logToFile("Received shutdown signal " + std::to_string(signal_number), INFO);
            
            json shutdownMsg = {{"quitGame", true}};
            broadcastMessage(shutdownMsg);
            
            {
                std::lock_guard<std::mutex> lock(shutdownMutex);
                isShuttingDown = true;
                shouldClose = true;
            }
            
            shutdownCV.notify_all();
            
            io_context.stop();
        }
    });
    
    // Re-register for future signals
    signals.async_wait([](const boost::system::error_code& error, int signal_number) {
        if (!error) {
            std::cout << "\nForce shutdown initiated..." << std::endl;
            logToFile("Force shutdown triggered", ERROR);
            std::quick_exit(1);
        }
    });
}

int main() {
    try {
        int port = getEnvVar<int>("PORT", 5766);
        std::cout << "Starting server on port " + std::to_string(port) << std::endl;
        logToFile("Initializing server on port " + std::to_string(port), INFO);

        wss.clear_access_channels(websocketpp::log::alevel::all);
        wss.set_access_channels(websocketpp::log::alevel::connect);
        wss.set_access_channels(websocketpp::log::alevel::disconnect);
        wss.set_access_channels(websocketpp::log::alevel::app);

        wss.init_asio(&io_context);
        wss.set_message_handler(std::bind(&onWebSocketMessage, std::placeholders::_1, std::placeholders::_2));
        wss.set_open_handler(std::bind(&onWebSocketOpen, std::placeholders::_1));
        wss.set_close_handler(std::bind(&onWebSocketClose, std::placeholders::_1));

        wss.listen(port + 1); // WebSocket port is HTTP port + 1
        wss.start_accept();

        setupSignalHandlers();

        std::vector<std::thread> threads;
        
        // CLI thread
        threads.emplace_back(cliThread);
        
        // Server thread
        threads.emplace_back([port]() {
            try {
                startServer(port);
            } catch (const std::exception& e) {
                logToFile("Server thread error: " + std::string(e.what()), ERROR);
            }
        });

        threads.emplace_back(enemyThread);
        threads.emplace_back(shieldThread);
        threads.emplace_back(heartbeatThread);

        while (!isShuttingDown) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        cleanup();
        
        for (auto& thread : threads) {
            if (thread.joinable()) {
                std::future<void> future = std::async(std::launch::async, [&thread]() {
                    thread.join();
                });
                
                if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
                    logToFile("Thread join timeout - forcing shutdown", INFO);
                    break;
                }
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal server error: " << e.what() << std::endl;
        logToFile("Fatal server error: " + std::string(e.what()), ERROR);
        return 1;
    }
}
