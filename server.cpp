#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
int main(){
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 1234));
    while(true){
        tcp::socket socket(io_context);
        //wait
        acceptor.accept(socket);
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");
    }
}