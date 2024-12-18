#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP
#include <cmath>
#include <vector>
#include <iostream>
#include "../raylib.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;


inline int calculateDistance(json& object, json& other_object) {
    int dx = object["x"].get<int>() - other_object["x"].get<int>();
    int dy = object["y"].get<int>() - other_object["y"].get<int>();
    return static_cast<int>(sqrt(dx*dx + dy*dy));
}

inline float calculateDirectionDegrees(json& object, json& other_object) {
    float dx = other_object["x"].get<int>() - object["x"].get<int>();
    float dy = other_object["y"].get<int>() - object["y"].get<int>();

    float angleRadians = atan2f(dy, dx);
    float angleDegrees = angleRadians * RAD2DEG;
    if (angleDegrees < 0) angleDegrees += 360.0f;
    return angleDegrees;
}

inline int findClosestNumber(int number, const std::vector<int>& numbers) {
    int min_diff = std::abs(number - numbers[0]);
    int closest_num = numbers[0];

    for (size_t i = 1; i < numbers.size(); ++i) {
        int diff = std::abs(number - numbers[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest_num = numbers[i];
        }
    }

    return closest_num;
}

inline std::string findPixelToGoTo(nlohmann::json& playersarray, nlohmann::json& enemy) {
    if (playersarray.empty()) {
        return ""; // :(
    }

    int smallestyet = 0;
    int smallestDistance = calculateDistance(enemy, playersarray[0]);
    for (int it = 1; it < (int)playersarray.size(); it++) {
        int dist = calculateDistance(enemy, playersarray[it]);
        if (dist < smallestDistance) {
            smallestDistance = dist;
            smallestyet = it;
        }
    }

    int degrees = static_cast<int>(calculateDirectionDegrees(enemy, playersarray[smallestyet]));

    std::vector<int> nums = {0, 45, 90, 135, 180, 225, 270, 315, 360};
    int dirtogo = findClosestNumber(degrees, nums);

    std::string dirnametogo;
    if (dirtogo == 0 || dirtogo == 360) dirnametogo = "north";
    else if (dirtogo == 45) dirnametogo = "north-east";
    else if (dirtogo == 90) dirnametogo = "east";
    else if (dirtogo == 135) dirnametogo = "south-east";
    else if (dirtogo == 180) dirnametogo = "south";
    else if (dirtogo == 225) dirnametogo = "south-west";
    else if (dirtogo == 270) dirnametogo = "west";
    else if (dirtogo == 315) dirnametogo = "north-west";

    return dirnametogo;
}

#endif
