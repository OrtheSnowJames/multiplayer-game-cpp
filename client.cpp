#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "raylib.h"

using namespace std;
using namespace boost::asio;
using ip::tcp;
using json = nlohmann::json;

/*
ATTENTION

Guide on how to do yes
Install apt packages in required packages.txt
Use linux c++ and boost
use gcc/g++ compiler
i dont know how to use make file so dont ask me 
compile manualy with compile.sh

messages sent to server are in json format like this:
json message = {
    type: "join"
};
std::string compact = message.dump();
boost::asio::write(socket, boost::asio::buffer(compact + "\n"));

these insructions are for ubuntu/debian/wsl ubuntu only, provided by the person who's name is not andy nor eli
bro that narrows it down to like infinite peopl- SHUT- 
*/
int main() {
    //init
    const int screenWidth = std::getenv("SCREEN_WIDTH") ? std::atoi(std::getenv("SCREEN_WIDTH")) : 800;
    const int screenHeight = std::getenv("SCREEN_HEIGHT") ? std::atoi(std::getenv("SCREEN_HEIGHT")) : 450;
    const int fps = std::getenv("FPS") ? std::atoi(std::getenv("FPS")) : 60;
    const int port = std::getenv("PORT") ? std::atoi(std::getenv("PORT")) : 1234;
    const string name = std::getenv("NAME") ? std::getenv("NAME") : "Player";
    InitWindow(screenWidth, screenHeight, "Game");
    bool gameRunning = true;
    try {
        boost::asio::io_context io_context;

        boost::asio::ip::tcp::socket socket(io_context);

        // Create endpoint for site
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 
            port
        );
        socket.connect(endpoint);
        std::cout << "Connected to server" << std::endl;

        // Buffer for receiving data
        boost::asio::streambuf buffer;
        //game loop
        while (gameRunning == true) {
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
            if (WindowShouldClose()) {
                json newMessage = {
                    {"quitGame", true}
                };
                gameRunning = false;
                break;
            }
            if (IsWindowResized()){
                SetWindowSize(screenWidth, screenHeight);
            }
            // Process received data
            std::istream input_stream(&buffer);
            std::string message;
            std::getline(input_stream, message);
            json messageJson = json::parse(message);
            if (messageJson.contains("quitGame")) {
                bool shouldQuit = messageJson["quitGame"];
                if (shouldQuit){ 
                    std::cout << "Received quit message, closing connection" << std::endl;
                    gameRunning = false;
                    break;
                }
            }
            //Other things than networking
            //Basic ediquite for games like this
            if (KEY_ESCAPE){
                ShowCursor();
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                HideCursor();
            }
            bool goingLeft = false;
            bool goingRight = false;
            bool goingUp = false;
            bool goingDown = false;
            if (KEY_W || KEY_UP){
                goingUp = true;
            }
            else {
                goingUp = false;
            }
            if (KEY_S || KEY_DOWN){
                goingDown = true;
            }
            else {
                goingDown = false;
            }
            if (KEY_A || KEY_LEFT){
                goingLeft = true;
            }
            else {
                goingLeft = false;
            }
            if (KEY_D || KEY_RIGHT){
                goingRight = true;
            }
            else {
                goingRight = false;
            }
            if (goingUp){
                //move up
            }
            if (goingDown){
                //move down
            }
            if (goingLeft){
                //move left
            }
            if (goingRight){
                //move right
            }
            //dont write code after this line
            try{
                std::string compact = checkList.dump();
                compact += "\n";
                boost::asio::write(socket, boost::asio::buffer(std::string(compact)));
            }catch(const std::exception& e){
                
            }
        }
        // Clean up
        socket.close();

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    CloseWindow();
    return 0;
}