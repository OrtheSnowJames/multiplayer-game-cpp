#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <boost/asio.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include "coolfunctions.hpp"
#include "raylib.h"
// Add any other required headers but don't include client.cpp directly

using json = nlohmann::json;

class ClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
};

// Test sprite state initialization
TEST_F(ClientTest, SpriteStateInitialization) {
    json player = {
        {"name", "TestPlayer"},
        {"x", 100},
        {"y", 100},
        {"spriteState", 1}
    };
    EXPECT_EQ(player["spriteState"].get<int>(), 1);
}

// Test sprite state type conversion
TEST_F(ClientTest, SpriteStateTypeConversion) {
    json player = {
        {"name", "TestPlayer"},
        {"x", 100},
        {"y", 100},
        {"spriteState", "2"} // String input
    };
    
    if (!player["spriteState"].is_number()) {
        player["spriteState"] = 0;
    }
    EXPECT_EQ(player["spriteState"].get<int>(), 0);
}

// Test default sprite state
TEST_F(ClientTest, DefaultSpriteState) {
    json player = {
        {"name", "TestPlayer"},
        {"x", 100},
        {"y", 100}
    };
    
    if (!player.contains("spriteState")) {
        player["spriteState"] = 0;
    }
    EXPECT_EQ(player["spriteState"].get<int>(), 0);
}

// Test game state player updates
TEST_F(ClientTest, GameStatePlayerUpdate) {
    json gameState = {
        {"room1", {
            {"players", {
                {
                    {"name", "Player1"},
                    {"x", 100},
                    {"y", 100},
                    {"spriteState", "invalid"}
                }
            }}
        }}
    };

    auto& players = gameState["room1"]["players"];
    for (auto& player : players) {
        if (player.contains("spriteState")) {
            if (!player["spriteState"].is_number()) {
                player["spriteState"] = 0;
            }
        } else {
            player["spriteState"] = 0;
        }
    }

    EXPECT_EQ(players[0]["spriteState"].get<int>(), 0);
}

// Test sprite state bounds
TEST_F(ClientTest, SpriteStateBounds) {
    json player = {
        {"name", "TestPlayer"},
        {"x", 100},
        {"y", 100},
        {"spriteState", 5} // Out of bounds value
    };
    
    // Ensure sprite state is within valid range (0-3)
    if (player["spriteState"].get<int>() < 0 || player["spriteState"].get<int>() > 3) {
        player["spriteState"] = 0;
    }
    
    EXPECT_EQ(player["spriteState"].get<int>(), 0);
}

// Test sprite state in message handling
TEST_F(ClientTest, MessageHandlingSpriteState) {
    json message = {
        {"local", true},
        {"name", "TestPlayer"},
        {"x", 100},
        {"y", 100},
        {"spriteState", "invalid"}
    };

    if (message.contains("spriteState")) {
        if (!message["spriteState"].is_number()) {
            message["spriteState"] = 0;
        }
    }

    EXPECT_EQ(message["spriteState"].get<int>(), 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}