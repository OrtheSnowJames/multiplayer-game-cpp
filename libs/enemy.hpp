#ifndef ENEMY_HPP
#define ENEMY_HPP

#include <raylib.h>
#include <iostream>
#include "pathfinding.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <cmath>

using json = nlohmann::json;

/**
 * Collision check for enemy vs. player.
 */
inline bool CheckECollision(const json& enemy, const json& player) {
    if (!enemy.contains("x") || !enemy.contains("y") || 
        !player.contains("x") || !player.contains("y") ||
        !enemy.contains("width") || !enemy.contains("height") ||
        !player.contains("width") || !player.contains("height")) {
        return false;
    }

    int left1   = enemy["x"].get<int>();
    int right1  = left1 + enemy["width"].get<int>();
    int top1    = enemy["y"].get<int>();
    int bottom1 = top1 + enemy["height"].get<int>();

    int left2   = player["x"].get<int>();
    int right2  = left2 + player["width"].get<int>();
    int top2    = player["y"].get<int>();
    int bottom2 = top2 + player["height"].get<int>();

    return !(left1 > right2 || right1 < left2 || top1 > bottom2 || bottom1 < top2);
}

inline json updateEnemy(json& playersArray, json& enemy) {
    int speed = enemy["speed"].get<int>();
    float ex = static_cast<float>(enemy["x"].get<int>());
    float ey = static_cast<float>(enemy["y"].get<int>());

    //find the closest player:
    if (playersArray.empty()) {
        // No players: no movement, no collisions
        return {{"noCollision", true}};
    }

    json closestPlayer = playersArray[0];
    float minDistance = static_cast<float>(calculateDistance(enemy, playersArray[0]));
    for (int i = 1; i < static_cast<int>(playersArray.size()); ++i) {
        float dist = static_cast<float>(calculateDistance(enemy, playersArray[i]));
        if (dist < minDistance) {
            minDistance = dist;
            closestPlayer = playersArray[i];
        }
    }

    // If within 30 pixels of player, snap to their position
    if (minDistance <= 30.0f) {
        ex = static_cast<float>(closestPlayer["x"].get<int>());
        ey = static_cast<float>(closestPlayer["y"].get<int>());
    } else {
        // 2) Vector from enemy to closest player
        float dx = static_cast<float>(closestPlayer["x"].get<int>()) - ex;
        float dy = static_cast<float>(closestPlayer["y"].get<int>()) - ey;

        // 3) Normalize if distance > 0 (avoid divide by zero)
        float length = std::sqrt(dx * dx + dy * dy);
        if (length > 0.0001f) {
            dx /= length; 
            dy /= length; 
            ex += dx * speed;
            ey += dy * speed;
        }
    }

    if (ex < 0.0f) ex = 0.0f;
    if (ey < 0.0f) ey = 0.0f;

    // Update JSON with new position
    enemy["x"] = static_cast<int>(ex);
    enemy["y"] = static_cast<int>(ey);

    int collidedPlayerIndex = -1;
    for (int i = 0; i < static_cast<int>(playersArray.size()); ++i) {
        if (CheckECollision(enemy, playersArray[i])) {
            collidedPlayerIndex = i;
            break;
        }
    }

    if (collidedPlayerIndex < 0) {
        return {{"noCollision", true}};
    }
    else {
        auto& p = playersArray[collidedPlayerIndex];
        if (p["inventory"]["shields"].get<int>() <= 0) {
            return {{"playerDead", p["id"]}};
        } else {
            return {{"playerHit", p["id"]}};
        }
    }
}

#endif // ENEMY_HPP
