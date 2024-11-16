#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "raylib.h"
#include <thread>
#include <random>
#include <vector>
#include <set>

using json = nlohmann::json;
using boost::asio::ip::tcp;

std::vector<std::shared_ptr<tcp::socket>> connected_sockets;
std::string nowOn;
json game = {
    {"room1", {
        {"players", {}},
        {"objects", {}},
        {"enemies", {}}
    }}
};

boost::asio::io_context io_context;

void logToFile(const std::string& message) {
    std::ofstream logFile("err.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    logFile << std::ctime(&now_time) << message << std::endl;
    logFile.close();
}

void eraseUser(json& game, int id){
    auto& players = game["room1"]["players"];
    players.erase(std::remove_if(players.begin(), players.end(), [id](const json& p) {
        return p["socket"] == id;
    }), players.end());
}

json broadcastMessage(json& message, json& game) {
    std::string compact = message.dump() + "\n";
    
    auto it = connected_sockets.begin();
    while (it != connected_sockets.end()) {
        try {
            if ((*it)->is_open()) {
                boost::asio::write(**it, boost::asio::buffer(compact));
                ++it;
            } else {
                eraseUser(game, (*it)->native_handle());
                it = connected_sockets.erase(it);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error broadcasting: " << e.what() << std::endl;
            logToFile("Broadcast error: " + std::string(e.what()));
            eraseUser(game, (*it)->native_handle());
            it = connected_sockets.erase(it);
        }
    }
    
    return game;
}

void broadcastGame(const json& game) {
    json newMessage = {
        {"getGame", game}
    };
    json gameCopy = game;
    broadcastMessage(newMessage, gameCopy);
}

void broadcastPlayer(json& player, json& game){
    broadcastMessage(player, game);
}

void broadcastGameLocal(const json& game, tcp::socket& socket){
    json newMessage = {
        {"getGame", game}
    };
    std::string compact = newMessage.dump() + "\n";
    boost::asio::write(socket, boost::asio::buffer(compact));
}

void acceptConnections(tcp::acceptor& acceptor) {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [socket, &acceptor](boost::system::error_code ec) {
        if (!ec) {
            connected_sockets.push_back(socket);
            std::cout << "New connection accepted!" << std::endl;
            acceptConnections(acceptor);
        } else {
            logToFile("Accept error: " + ec.message());
        }
    });
}

json createUser(std::string name, int id){
    std::random_device rd;
    json newUsr = {
        {"name", name},
        {"x", rd() % 800},
        {"y", rd() % 450},
        {"dead", false},
        {"speed", 5},
        {"score", 0},
        {"inventory", {
            {"shields", 0},
            {"bananas", 0}
        }},
        {"socket", id},
        {"spriteState", 0},
        {"skin", 1},
        {"local", false},
        {"room", 1}
    };
    return newUsr;
}

json handleMessage(const std::string& message, tcp::socket& socket, json& game){
    json readableMessage = json::parse(message);
    if (readableMessage.contains("quitGame")){
        bool shouldQuit = readableMessage["quitGame"];
        if (shouldQuit){
            for (auto it = game["room1"]["players"].begin(); it != game["room1"]["players"].end(); ++it){
                if ((*it)["socket"] == socket.native_handle()){
                    game["room1"]["players"].erase(it);
                    break;
                }
            }
            std::string compact = game.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(compact));
        }
    }
    if (readableMessage.contains("currentName")){
        int howManyDuplicates = 0;
        for (auto& p : game["room1"]["players"]){
            if (p["name"] == readableMessage["currentName"]){
                howManyDuplicates++;
            }
        }
        std::string howManyDuplicatesString = std::to_string(howManyDuplicates);
        std::string newName = readableMessage["currentName"].get<std::string>() + howManyDuplicatesString;
        json newNameJSON = createUser(newName, socket.native_handle());
        json lnewNameJSON = createUser(newName, socket.native_handle());
        lnewNameJSON["local"] = true;
        boost::asio::write(socket, boost::asio::buffer(lnewNameJSON.dump() + "\n"));
        game = broadcastMessage(newNameJSON, game);
        std::cout << "New player: " << newName << " is very cool for joining our game" << std::endl;
        nowOn = "New player: " + newName + " is very cool for joining our game";
    }
    if (readableMessage.contains("requestGame")){
        bool shouldGetGame = readableMessage["requestGame"];
        if (shouldGetGame){
            json newMessage = {
                {"getGame", game}
            };
            std::string compact = newMessage.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(compact));
        }
    }
    if (readableMessage.contains("currentGame")){
        std::string currentGame = readableMessage["currentGame"];
        if (currentGame != game.dump()){
            json newMessage = {
                {"game", game}
            };
            std::string compact = newMessage.dump() + "\n";
            boost::asio::write(socket, boost::asio::buffer(compact));
        }
    }
    if (readableMessage.contains("goingup") && readableMessage["goingup"].get<bool>()){
        for (auto& p : game["room1"]["players"]){
            if (p["socket"] == socket.native_handle()){
                p["spriteState"] = 1;
            }
        }
    }
    if (readableMessage.contains("goingdown") && readableMessage["goingdown"].get<bool>()){
        for (auto& p : game["room1"]["players"]){
            if (p["socket"] == socket.native_handle()){
                p["spriteState"] = 3;
            }
        }
    }
    if (readableMessage.contains("goingleft") && readableMessage["goingleft"].get<bool>()){
        for (auto& p : game["room1"]["players"]){
            if (p["socket"] == socket.native_handle()){
                p["spriteState"] = 2;
            }
        }
    }
    if (readableMessage.contains("goingright") && readableMessage["goingright"].get<bool>()){
        for (auto& p : game["room1"]["players"]){
            if (p["socket"] == socket.native_handle()){
                p["spriteState"] = 4;
            }
        }
    }
    if (!readableMessage.contains("goingright") && !readableMessage.contains("goingleft") && !readableMessage.contains("goingdown") && !readableMessage.contains("goingup")){
        for (auto& p : game["room1"]["players"]){
            if (p["socket"] == socket.native_handle()){
                p["spriteState"] = 3;
            }
        }
    }
    if (readableMessage.contains("currentPlayer") && !readableMessage["currentPlayer"].is_null()){
        for (auto& p : game["room1"]["players"]){
            if (p["socket"] == socket.native_handle()){
                if (p != readableMessage["currentPlayer"]){
                    p = readableMessage["currentPlayer"];
                    broadcastPlayer(p, game);
                }
            }
        }
    }
    broadcastGame(game);
    return game;
}

void startServer(int port) {
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    acceptConnections(acceptor);
    io_context.run();
}

int main() {
    const int screenWidth = std::getenv("SCREEN_WIDTH") ? std::atoi(std::getenv("SCREEN_WIDTH")) : 800;
    const int screenHeight = std::getenv("SCREEN_HEIGHT") ? std::atoi(std::getenv("SCREEN_HEIGHT")) : 450;
    int fps = std::getenv("FPS") ? std::atoi(std::getenv("FPS")) : 60;
    const int port = std::getenv("PORT") ? std::atoi(std::getenv("PORT")) : 1234;

    InitWindow(screenWidth, screenHeight, "Server");

    // Start the server in a separate thread
    std::thread serverThread(startServer, port);

    SetTargetFPS(fps);
    bool gameRunning = true;
    while (gameRunning) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("This is the server window", 0, 0, 20, LIGHTGRAY);
        DrawText("Thank you for hosting a server", (screenWidth / 2), (screenHeight / 2 - 21), 15, BLACK);
        DrawText(nowOn.c_str(), (screenWidth / 2), (screenHeight / 2), 15, BLACK);
        EndDrawing();

        if (WindowShouldClose()) {
            gameRunning = false;
            io_context.stop(); // Stop the io_context to end the server thread gracefully
        }
    }

    // Join the server thread before exiting
    if (serverThread.joinable()) {
        serverThread.join();
    }

    CloseWindow();
    return 0;
}
