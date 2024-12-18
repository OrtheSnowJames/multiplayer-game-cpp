#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "../raylib.h"
#include <iostream>
#include "pathfinding.hpp"
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;
using namespace std;

static bool checkECollision(const json& enemy, const json& player) {
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

inline nlohmann::json updateEnemy(json& playersarray, json& enemy) {
    string direction = findPixelToGoTo(playersarray, enemy);

    int speed = enemy["speed"].get<int>();
    int ex = enemy["x"].get<int>();
    int ey = enemy["y"].get<int>();

    if (direction == "north") ey -= speed;
    else if (direction == "north-east") { ey -= speed; ex += speed; }
    else if (direction == "east") ex += speed;
    else if (direction == "south-east") { ey += speed; ex += speed; }
    else if (direction == "south") ey += speed;
    else if (direction == "south-west") { ey += speed; ex -= speed; }
    else if (direction == "west") ex -= speed;
    else if (direction == "north-west") { ey -= speed; ex -= speed; }
    else std::cerr << "Error: direction not found" << std::endl;

    enemy["x"] = ex;
    enemy["y"] = ey;

    int playerinarray = -1;

    for (int it = 0; it < (int)playersarray.size(); it++) {
        if (checkECollision(enemy, playersarray[it])) {
            playerinarray = it;
            break;
        }
    }

    json message;
    if (playerinarray < 0 || playerinarray >= static_cast<int>(playersarray.size())) {
        message = {{"noCollision", true}};
    } else if (playersarray[playerinarray]["inventory"]["shields"] <= 0) {
        message = {{"playerDead", playersarray[playerinarray]["id"]}};
    } else {
        message = {{"playerHit", playersarray[playerinarray]["id"]}};
    }

    return message;
}

#endif
