#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "raylib.h"
#include <random>
#include <vector>
#include <set>
using json = nlohmann::json;
using boost::asio::ip::tcp;
std::vector<std::shared_ptr<tcp::socket>> connected_sockets;
std::string nowOn;
void logToFile (const std::string message){
    std::ofstream logFile("err.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
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
                ++it;  // Only increment on success
            } else {
                // Handle closed socket
                eraseUser(game, (*it)->native_handle());
                it = connected_sockets.erase(it);  // erase returns next iterator
            }
        } catch (const std::exception& e) {
            // Log error and remove bad socket
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

void broadcastGameLocal(const json& game, tcp::socket& socket){
    json newMessage = {
        {"getGame", game}
    };
    std::string compact = newMessage.dump();
    compact += "\n";
    boost::asio::write(socket, boost::asio::buffer(std::string(compact)));
}
void acceptConnection(tcp::acceptor& acceptor, boost::asio::io_context& io_context) {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.accept(*socket);
    connected_sockets.push_back(socket);
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
        {"speed", 5},
        {"score", 0},
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
            for (auto& p : game["room1"]["players"]){
                if (p["socket"] == socket.native_handle()){
                    game["room1"]["players"].erase(p);
                }
            }
            std::string compact = game.dump();
            compact += "\n";
            boost::asio::write(socket, boost::asio::buffer(std::string(compact)));
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
        boost::asio::write(socket, boost::asio::buffer(std::string(lnewNameJSON.dump())));
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
            std::string compact = game.dump();
            compact += "\n";
            boost::asio::write(socket, boost::asio::buffer(std::string(compact)));
        }
    }
    if (readableMessage.contains("currentGame")){
        std::string currentGame = readableMessage["currentGame"];
        if (currentGame != game){
            json newMessage = {
                {"game", game}
            };
            std::string compact = game.dump();
            compact += "\n";
            boost::asio::write(socket, boost::asio::buffer(std::string(compact)));
        }
    }
    //put things that would be on the checklist here
       if (readableMessage["goingup"]){
           for (auto& p : game["room1"]["players"]){
                if (p["socket"] == socket.native_handle()){
                        p["spriteState"] = 1;
                    }
                }
           }
        if (readableMessage["goingdown"]){
            for (auto& p : game["room1"]["players"]){
                if (p["socket"] == socket.native_handle()){
                    p["spriteState"] = 3;
                }
            }
        }
        if (readableMessage["goingleft"]){
            for (auto& p : game["room1"]["players"]){
                if (p["socket"] == socket.native_handle()){
                    p["spriteState"] = 2;
                }
            }
        }
        if (readableMessage["goingright"]){
            for (auto& p : game["room1"]["players"]){
                if (p["socket"] == socket.native_handle()){
                    p["spriteState"] = 4;
                }
            }
        }
        if (!readableMessage["goingright"] && !readableMessage["goingleft"] && !readableMessage["goingdown"] && !readableMessage["goingup"]){
            for (auto& p : game["room1"]["players"]){
                if (p["socket"] == socket.native_handle()){
                    p["spriteState"] = 3;
                }
            }
        }
        if (readableMessage.contains("currentPlayer")){
            if (readableMessage["currentPlayer"] != nullptr){
                for (auto& p : game["room1"]["players"]){
                    if (p["socket"] == socket.native_handle()){
                        if (p != readableMessage["currentPlayer"]){
                            p = readableMessage["currentPlayer"];
                            broadcastPlayer(p, game);
                        }
                    }
                }
            }
        }
        broadcastGame(game);
        return game;
    }
void broadcastPlayer(json& player, json& game){
    broadcastMessage(player, game);
}
int main(){
    const int screenWidth = std::getenv("SCREEN_WIDTH") ? std::atoi(std::getenv("SCREEN_WIDTH")) : 800;
    const int screenHeight = std::getenv("SCREEN_HEIGHT") ? std::atoi(std::getenv("SCREEN_HEIGHT")) : 450;
    int fps = std::getenv("FPS") ? std::atoi(std::getenv("FPS")) : 60;
    const int port = std::getenv("PORT") ? std::atoi(std::getenv("PORT")) : 1234;
    InitWindow(screenWidth, screenHeight, "Server");
    
    json game = {
        {"room1", {
            {"players", {
                
            }},
            {"objects", {
                
            }},
            {"enemies", {
                
            }}
        }}
    };
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    bool gameRunning = true;
    while(gameRunning){
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("This is the server window", 0, 0, 20, LIGHTGRAY);
        DrawText("Thank you for hosting a server", (screenWidth/2), (screenHeight/2-21), 15, BLACK);
        DrawText(nowOn.c_str(), (screenWidth/2), (screenHeight/2), 15, BLACK);
        EndDrawing();

        if (WindowShouldClose()){
            gameRunning = false;
            continue;
        }

        auto socket = std::make_shared<tcp::socket>(io_context);
        acceptor.accept(*socket);

        boost::asio::streambuf buffer;
        boost::asio::read_until(*socket, buffer, "\n");
        
        std::istream is(&buffer);
        std::string message;
        std::getline(is, message);

        game = handleMessage(message, *socket, game);

        std::string line;
        std::getline(is, line);
        
        std::string compact = game.dump();
        boost::asio::write(socket, boost::asio::buffer(compact));
    }
    
}
