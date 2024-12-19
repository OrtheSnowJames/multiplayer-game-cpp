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
#include <condition_variable>
#include <future>

// Include WebSocketPP
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#ifdef _WIN32
#include <winsock2.h>
using SocketHandle = SOCKET;
#else
#include <unistd.h>
using SocketHandle = int;
#endif

using json = nlohmann::json;
using boost::asio::ip::tcp;

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;

// Global flags
std::atomic<bool> shouldClose{false};
std::atomic<bool> isShuttingDown{false};

// Mutexes
std::mutex socket_mutex;
std::mutex game_mutex;
std::mutex shutdownMutex;
std::mutex ws_mutex;

// Condition variable
std::condition_variable shutdownCV;

// Boost io_context for TCP
boost::asio::io_context io_context;

// Shared game state
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

// Tracking TCP connections
std::vector<std::shared_ptr<tcp::socket>> connected_sockets;

// Tracking WebSocket connections
std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> open_ws_connections;

// Forward declarations
void broadcastMessage(const json& message);

// Logging
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
        {"local", true},
        {"room", 1}
    };
    return newPlayer;
}

json createUser(const std::string& name, tcp::socket& socket) {
    int fid = castWinsock(socket);
    return createUserRaw(name, fid);
}

static std::atomic<int> ws_id_counter{100000}; // large number to avoid conflict with TCP socket IDs
std::map<websocketpp::connection_hdl, int, std::owner_less<websocketpp::connection_hdl>> ws_ids;

json createUserWS(const std::string& name, websocketpp::connection_hdl hdl) {
    int fid = ws_id_counter++;
    ws_ids[hdl] = fid;
    return createUserRaw(name, fid);
}

void eraseUser(int id) {
    std::lock_guard<std::mutex> lock(game_mutex);
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
    json gameUpdate = {{"getGame", game}};
    broadcastMessage(gameUpdate);
}

void broadcastMessage(const json& message) {
    std::lock_guard<std::mutex> lock(game_mutex);
    std::string compact = message.dump();

    {
        std::lock_guard<std::mutex> s_lock(socket_mutex);
        auto it = connected_sockets.begin();
        while (it != connected_sockets.end()) {
            try {
                if ((*it)->is_open()) {
                    boost::asio::write(**it, boost::asio::buffer(compact + "\n"));
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
    }

    // Send to WebSocket clients
    {
        std::lock_guard<std::mutex> ws_lock(ws_mutex);
        extern WebSocketServer* g_ws_server;
        if (!g_ws_server) return;

        for (auto& hdl : open_ws_connections) {
            try {
                g_ws_server->send(hdl, compact, websocketpp::frame::opcode::text);
            } catch (...) {
                // If send fails, we might want to close this connection
                // But just ignore for brevity
            }
        }
    }
}

// Handle a message from either TCP or WS
void handleMessage(const json& messageJson, int socketId) {
    try {
        if (messageJson.contains("currentName")) {
            std::string requestedName = messageJson["currentName"].get<std::string>();
            std::string name = requestedName;
            if (findPlayer(requestedName)) {
                for (int i = 1; i < 100; i++) {
                    std::string newName = requestedName + std::to_string(i);
                    if (!findPlayer(newName)) {
                        name = newName;
                        break;
                    }
                }
            }

            // Create user and insert into room1
            json newPlayer = createUserRaw(name, socketId);
            {
                std::lock_guard<std::mutex> lock(game_mutex);
                game["room1"]["players"].push_back(newPlayer);
            }

            json localResponse = newPlayer;
            if (socketId < 100000) {
                std::lock_guard<std::mutex> s_lock(socket_mutex);
                for (auto& s : connected_sockets) {
                    if (s->native_handle() == socketId) {
                        boost::asio::write(*s, boost::asio::buffer(localResponse.dump() + "\n"));
                        boost::asio::write(*s, boost::asio::buffer(json({{"getGame", game}}).dump() + "\n"));
                        break;
                    }
                }
            } else {
                // It's a WebSocket client
                std::lock_guard<std::mutex> ws_lock(ws_mutex);
                extern WebSocketServer* g_ws_server;
                if (g_ws_server) {
                    // Find hdl from ws_ids
                    for (auto& kv : ws_ids) {
                        if (kv.second == socketId) {
                            g_ws_server->send(kv.first, localResponse.dump(), websocketpp::frame::opcode::text);
                            g_ws_server->send(kv.first, json({{"getGame", game}}).dump(), websocketpp::frame::opcode::text);
                            break;
                        }
                    }
                }
            }
            return;
        }

        if (messageJson.contains("quitGame") && messageJson["quitGame"].get<bool>()) {
            eraseUser(socketId);
            return;
        }

        if (messageJson.contains("x") || messageJson.contains("y") || messageJson.contains("spriteState")) {
            // Update player in game state
            std::lock_guard<std::mutex> lock(game_mutex);
            for (auto& room : game.items()) {
                if (room.value().contains("players")) {
                    for (auto& p : room.value()["players"]) {
                        if (p["socket"].get<int>() == socketId) {
                            if (messageJson.contains("spriteState")) {
                                p["spriteState"] = messageJson["spriteState"].get<int>();
                            }
                            if (messageJson.contains("x")) {
                                p["x"] = messageJson["x"].get<int>();
                            }
                            if (messageJson.contains("y")) {
                                p["y"] = messageJson["y"].get<int>();
                            }
                            if (messageJson.contains("room")) {
                                int newRoom = messageJson["room"].get<int>();
                                if (newRoom != p["room"].get<int>()) {
                                    // Move player to new room
                                    p["room"] = newRoom;
                                }
                            }
                            break;
                        }
                    }
                }
            }

            json positionUpdate = {
                {"updatePosition", {
                    {"socket", socketId},
                    {"x", messageJson.value("x", 0)},
                    {"y", messageJson.value("y", 0)},
                    {"spriteState", messageJson.value("spriteState", 1)}
                }}
            };
            broadcastMessage(positionUpdate);
        }

        // Request full game
        if (messageJson.contains("requestGame")) {
            json gameUpdate = {{"getGame", game}};
            // Send directly to this player
            if (socketId < 100000) {
                std::lock_guard<std::mutex> s_lock(socket_mutex);
                for (auto& s : connected_sockets) {
                    if (s->native_handle() == socketId) {
                        boost::asio::write(*s, boost::asio::buffer(gameUpdate.dump() + "\n"));
                        break;
                    }
                }
            } else {
                std::lock_guard<std::mutex> ws_lock(ws_mutex);
                extern WebSocketServer* g_ws_server;
                if (g_ws_server) {
                    for (auto& kv : ws_ids) {
                        if (kv.second == socketId) {
                            g_ws_server->send(kv.first, gameUpdate.dump(), websocketpp::frame::opcode::text);
                            break;
                        }
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        logToFile("Error in handleMessage: " + std::string(e.what()), ERROR);
    }
}

void startReading(std::shared_ptr<tcp::socket> socket);
void acceptConnections(tcp::acceptor& acceptor);

void startReading(std::shared_ptr<tcp::socket> socket) {
    auto buffer = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(*socket, *buffer, "\n", [socket, buffer](boost::system::error_code ec, std::size_t) {
        if (!ec) {
            std::istream is(buffer.get());
            std::string message;
            std::getline(is, message);
            if (!message.empty()) {
                json messageJson = json::parse(message, nullptr, false);
                if (!messageJson.is_discarded()) {
                    handleMessage(messageJson, castWinsock(*socket));
                }
            }
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
            {
                std::lock_guard<std::mutex> lock(socket_mutex);
                connected_sockets.push_back(socket);
            }
            std::cout << "New TCP connection accepted!" << std::endl;
            startReading(socket);
            acceptConnections(acceptor);
        } else {
            logToFile("Error accepting connection: " + ec.message(), ERROR);
        }
    });
}

WebSocketServer* g_ws_server = nullptr;

class WSServer {
public:
    WSServer() {
        m_server.clear_access_channels(websocketpp::log::alevel::all);
        m_server.clear_error_channels(websocketpp::log::elevel::all);

        m_server.set_open_handler([this](websocketpp::connection_hdl hdl) {
            on_open(hdl);
        });

        m_server.set_close_handler([this](websocketpp::connection_hdl hdl) {
            on_close(hdl);
        });

        m_server.set_message_handler([this](websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
            on_message(hdl, msg);
        });
    }

    void run(int port) {
        boost::system::error_code ec;
        m_server.listen(port, ec);
        if (ec) {
            logToFile("Failed to bind to WS port: " + ec.message(), ERROR);
            return;
        }
        m_server.start_accept();
        m_server.run();
    }

    WebSocketServer& server() { return m_server; }

private:
    void on_open(websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(ws_mutex);
        open_ws_connections.insert(hdl);
        std::cout << "New WebSocket connection accepted!" << std::endl;
    }

    void on_close(websocketpp::connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(ws_mutex);
        open_ws_connections.erase(hdl);
        // If we assigned a user to this hdl, remove them
        if (ws_ids.find(hdl) != ws_ids.end()) {
            int socketId = ws_ids[hdl];
            ws_ids.erase(hdl);
            eraseUser(socketId);
        }
        std::cout << "WebSocket connection closed" << std::endl;
    }

    void on_message(websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
        try {
            auto payload = msg->get_payload();
            json messageJson = json::parse(payload, nullptr, false);
            if (!messageJson.is_discarded()) {
                // If no ID assigned yet and message is currentName, we create user
                int socketId;
                if (ws_ids.find(hdl) == ws_ids.end()) {
                    // If we are expecting "currentName" first
                    if (messageJson.contains("currentName")) {
                        //create user and assign ID
                        std::string requestedName = messageJson["currentName"].get<std::string>();
                        json newPlayer = createUserWS(requestedName, hdl);
                        {
                            std::lock_guard<std::mutex> lock(game_mutex);
                            game["room1"]["players"].push_back(newPlayer);
                        }

                        // Send back local player and game
                        std::string localResponse = newPlayer.dump();
                        m_server.send(hdl, localResponse, websocketpp::frame::opcode::text);
                        m_server.send(hdl, json({{"getGame", game}}).dump(), websocketpp::frame::opcode::text);
                        return;
                    } else {
                        return;
                    }
                }
                socketId = ws_ids[hdl];
                handleMessage(messageJson, socketId);
            }
        } catch (const std::exception& e) {
            logToFile("Error in on_message (WS): " + std::string(e.what()), ERROR);
        }
    }

    WebSocketServer m_server;
};

void cleanup() {
    {
        std::unique_lock<std::mutex> lock(shutdownMutex);
        isShuttingDown = true;
        shouldClose = true;
    }
    shutdownCV.notify_all();

    std::this_thread::sleep_for(std::chrono::seconds(2));

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

    {
        std::lock_guard<std::mutex> game_lock(game_mutex);
        game = json::object();
    }

    logToFile("Server cleanup completed", INFO);
}

void startTCPServer(int port) {
    tcp::acceptor acceptor(io_context);
    acceptor.open(tcp::v4());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.set_option(tcp::acceptor::keep_alive(true));

    // Try binding
    boost::system::error_code ec;
    acceptor.bind(tcp::endpoint(tcp::v4(), port), ec);
    if (ec) {
        throw std::runtime_error("Failed to bind TCP server: " + ec.message());
    }

    acceptor.listen();
    acceptConnections(acceptor);
}

int main() {
    try {
        int tcpPort = 5767;
        int wsPort = 9002;

        WSServer ws_server;
        g_ws_server = &ws_server.server();

        std::thread wsThread([&ws_server, wsPort]() {
            ws_server.run(wsPort);
        });

        // Start TCP server
        startTCPServer(tcpPort);

        logToFile("TCP server running on port " + std::to_string(tcpPort), INFO);
        logToFile("WebSocket server running on port " + std::to_string(wsPort), INFO);

        // Run io_context in the main thread
        while (!isShuttingDown) {
            io_context.run_for(std::chrono::seconds(1));
        }

        // Shutdown sequence
        logToFile("Starting server shutdown", INFO);
        cleanup();

        if (wsThread.joinable()) {
            wsThread.join();
        }

        logToFile("Server shutdown complete", INFO);
        return 0;

    } catch (const std::exception& e) {
        logToFile("Fatal server error: " + std::string(e.what()), ERROR);
        return 1;
    }
}
