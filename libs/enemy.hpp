#ifndef ENEMY_HPP
#define ENEMY_HPP
#include "../raylib.h"
#include <iostream>
#include "pathfinding.hpp"
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;
using namespace std;

inline bool checkECollision(const json& enemy, const json& player) {
    if (!enemy.contains("x") || !enemy.contains("y") || 
        !player.contains("x") || !player.contains("y") ||
        !enemy.contains("width") || !enemy.contains("height") ||
        !player.contains("width") || !player.contains("height")) {
        return false;
    }

    int left1 = enemy["x"].get<int>();
    int right1 = left1 + enemy["width"].get<int>();
    int top1 = enemy["y"].get<int>();
    int bottom1 = top1 + enemy["height"].get<int>();

    int left2 = player["x"].get<int>();
    int right2 = left2 + player["width"].get<int>();
    int top2 = player["y"].get<int>();
    int bottom2 = top2 + player["height"].get<int>();

    return !(left1 > right2 || right1 < left2 || top1 > bottom2 || bottom1 < top2);
}

template <typename T>
T updateEnemy(json& playersarray, json& enemy) {
    //first move towards if time inbetween was 1 second (5px)
    string direction = findPixelToGoTo(enemy, playersarray);
    if (direction == "north") enemy["y"] -= enemy["speed"];
    else if (direction == "north-east") {
        enemy["y"] -= enemy["speed"];
        enemy["x"] += enemy["speed"];
    }
    else if (direction == "east") enemy["x#include "libs/pathfinding.hpp""] += enemy["speed"];
    else if (direction == "south-east") {
        enemy["y"] += enemy["speed"];
        enemy["x"] += enemy["speed"];
    }
    else if (direction == "south") enemy["y"] += enemy["speed"];
    else if (direction == "south-west") {
        enemy["y"] += enemy["speed"];
        enemy["x"] -= enemy["speed"];
    }
    else if (direction == "west") enemy["x"] -= enemy["speed"];
    else if (direction == "north-west") {
        enemy["y"] -= enemy["speed"];
        enemy["x"] -= enemy["speed"];
    }
    int playerinarray;
    //then check if player is colliding
   for (int it = 0; it < playersarray.size(); it++) {
        if (checkECollision(enemy, playersarray[it])) {
            //player is hit
            playerinarray = it;
            break;
        }
    }
    json message;
    if (playersarray[playerinarray]["inventory"]["shields"] == 0 || playersarray[playerinarray]["inventory"]["shields"] < 0) {
        message = {{"playerDead", playersarray[playerinarray]["id"]}};
        return message;
    } else {
        //make sure server handles shield taking 
        message = {{"playerHit", playersarray[playerinarray]["id"]}};
        return message;
    }
}

#endif