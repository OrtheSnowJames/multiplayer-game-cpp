g++ -g server.cpp -lboost_system -lboost_thread -lboosCXX = g++
CXXFLAGS = -std=c++11 -Wall -I/path/to/boost
LDFLAGS = -L/path/to/boost/lib 

TARGET = my_program
SOURCES = main.cpp

OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
    $(CXX) $(OBJECTS) $(LDFLAGS) -lboost_filesystem -o $(TARGET)

%.o: %.cpp
    $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
    rm -f $(OBJECTS) $(TARGET)t_filesystem -lboost_regex -lboost_chrono -lboost_date_time -lboost_asio
g++ -g client.cpp -lboost_system -lboost_thread -lboost_filesystem -lboost_regex -lboost_chrono -lboost_date_time -lboost_asio
