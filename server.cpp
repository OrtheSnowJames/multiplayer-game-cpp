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

// Logging and error handling
enum LogLevel { INFO, ERROR, DEBUG };
void logToFile(const std::string& message, LogLevel level = INFO) {
    static const std::map<LogLevel, std::string> levelNames = {
        {INFO, "INFO"}, {ERROR, "ERROR"}, {DEBUG, "DEBUG"}};

    std::ofstream logFile("err.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    // Remove trailing newline from ctime
    std::string timeStr = std::ctime(&now_time);
    timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());

    logFile << "[" << levelNames.at(level) << "] " << timeStr << " " << message << std::endl;
}

void reportError(const std::string& message){
    logToFile(message, ERROR);
    std::cerr << message << std::endl;
    return;
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

bool findPlayer(std::string name) {
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

json createUser(const std::string& name, int fid) {
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
            return createUser(name, fid); // recreate if colliding
        }
    }
    return newPlayer;
}

json createUser(const std::string& name, tcp::socket& socket) {
    int fid = castWinsock(socket);
    return createUser(name, fid);
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
    std::lock_guard<std::mutex> lock(game_mutex);
    try {
        {
            std::lock_guard<std::mutex> socket_lock(socket_mutex);
            auto it = std::remove_if(connected_sockets.begin(), connected_sockets.end(),
                [id](const std::shared_ptr<tcp::socket>& socket) {
                    return socket && socket->native_handle() == id;
                });
            connected_sockets.erase(it, connected_sockets.end());
        }

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

void broadcastMessage(const json& message) {
    std::lock_guard<std::mutex> lock(game_mutex);
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
    json updateMessage = {{"getGame", game}};
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

void shieldThread() {
    while (!shouldClose) {
        if (playersInRoom("room2") && !shieldExists()) {
            std::lock_guard<std::mutex> lock(game_mutex);
            game["room2"]["objects"].push_back(createShield());
            json gameUpdate = {{"getGame", game}};
            broadcastMessage(gameUpdate);
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void handleMessage(const std::string& message, tcp::socket& socket) {
    try {
        json messageJson = json::parse(message);
        int sockID = castWinsock(socket);
        
        // Initial connection
        if (messageJson.contains("currentName")) {
            std::string name;
            std::string requestedName = messageJson["currentName"].get<std::string>();
            if (findPlayer(requestedName)) {
                for (int i = 1; i < 100; i++) {
                    std::string newName = requestedName + std::to_string(i);
                    if (!findPlayer(newName)) {
                        name = newName;
                        break;
                    }
                }
            } else {
                name = requestedName;
            }
            json newPlayer = createUser(name, sockID);
            newPlayer["local"] = true;
            game["room1"]["players"].push_back(newPlayer);
            
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

            if (messageJson.contains("room") && lookForPlayer(socket)["room"].get<int>() != messageJson["room"].get<int>()) {
                json player = lookForPlayer(socket);
                std::string newRoomName = "room" + std::to_string(messageJson["room"].get<int>());

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
                {
                    std::string oldRoomName = roomName; // previously looked up
                    auto& oldRoomPlayers = game[oldRoomName]["players"];
                    oldRoomPlayers.erase(
                        std::remove_if(oldRoomPlayers.begin(), oldRoomPlayers.end(),
                            [&socket](const json& p) { 
                                return p["socket"] == castWinsock(socket); 
                            }
                        ),
                        oldRoomPlayers.end()
                    );
                }

                // Add to new room
                game[newRoomName]["players"].push_back(player);

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
        logToFile("Error in handleMessage: " + std::string(e.what()), ERROR);
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
            eraseUser(socket->native_handle());
            socket->close();
        }
    });
}

void acceptConnections(tcp::acceptor& acceptor);

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

void setupSignalHandlers() {
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        std::cout << "Shutting down server..." << std::endl;
        shouldClose = true;
        io_context.stop();
    });
}

bool shieldThreadStarted = false;
bool enemyThreadStarted = false;

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
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in enemy thread: " << e.what() << std::endl;
            logToFile("Error in enemy thread: " + std::string(e.what()), ERROR);
        }
    }
    std::cout << "Enemy thread stopping" << std::endl;
    logToFile("Enemy thread stopping", INFO);
}

void startServer(int port) {
    tcp::acceptor acceptor(io_context);
    acceptor.open(tcp::v4());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    
    // Try multiple ports if initial port is in use
    const int MAX_PORT_ATTEMPTS = 10;
    int currentPort = port;
    bool bound = false;
    
    for(int attempt = 0; attempt < MAX_PORT_ATTEMPTS; attempt++) {
        boost::system::error_code ec;
        acceptor.bind(tcp::endpoint(tcp::v4(), currentPort), ec);
        
        if (!ec) {
            bound = true;
            std::cout << "Successfully bound to port " << currentPort << std::endl;
            break;
        }
        
        std::cerr << "> Port " << currentPort << " is in use, trying " << currentPort + 1 << std::endl;
        currentPort++;
        
        acceptor.close();
        acceptor.open(tcp::v4());
        acceptor.set_option(tcp::acceptor::reuse_address(true));
    }
    
    if (!bound) {
        std::cerr << "Failed to find available port after " << MAX_PORT_ATTEMPTS << " attempts\n";
        shouldClose = true;
        return;
    }
    
    acceptor.listen();
    acceptConnections(acceptor);
}

int main() {
    int port = getEnvVar<int>("PORT", 5767);
    std::cout << "Starting server on port " << port << "..." << std::endl;
    setupSignalHandlers();
    startServer(port);

    // Start background threads
    if (!enemyThreadStarted) {
        std::thread(enemyThread).detach();
        enemyThreadStarted = true;
    }
    if (!shieldThreadStarted) {
        std::thread(shieldThread).detach();
        shieldThreadStarted = true;
    }

    // Run until stopped by signal
    io_context.run();

    // Clean up
    // Additional cleanup if needed

    return 0;
}
