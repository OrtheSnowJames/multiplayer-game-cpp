#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include "raylib.h"

using namespace std;
using namespace boost::asio;
using ip::tcp;

int main() {
    try {
        boost::asio::io_context io_context;

        boost::asio::ip::tcp::socket socket(io_context);

        // Create endpoint for site
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 
            1234
        );

        socket.connect(endpoint);
        std::cout << "Connected to server" << std::endl;

        // Buffer for receiving data
        boost::asio::streambuf buffer;

        while (true) {
            // Read data until newline
            boost::asio::read_until(socket, buffer, "quitGame");

            // Process received data
            std::istream input_stream(&buffer);
            std::string message;
            std::getline(input_stream, message);

            if (message == "quitGame") {
                std::cout << "Received quit message, closing connection" << std::endl;
                break;
            }

            std::cout << "Received: " << message << std::endl;
        }

        // Clean up
        socket.close();

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}