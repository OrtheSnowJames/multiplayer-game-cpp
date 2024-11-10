#include <iostream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using boost::asio::ip::tcp;
void handleMessage(const std::string& message, tcp::socket& socket, json& game){
        
}
int main(){
    const int port = std::getenv("PORT") ? std::atoi(std::getenv("PORT")) : 1234;
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
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 1234));
    while(true){
        tcp::socket socket(io_context);
        //wait
        acceptor.accept(socket);
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");
        std::istream is(&buffer);
        std::string line;
        std::getline(is, line);
        std::string compact = game.dump();
        boost::asio::write(socket, boost::asio::buffer(compact));
    }
    
}

