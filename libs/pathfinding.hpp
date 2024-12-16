#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP
#include <cmath>
#include <vector>
#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;
#ifndef DEG2RAD
#define DEG2RAD (M_PI / 180.0f)
#endif

inline int calculateDistance(json& object, json& other_object) {
    int dx = abs(object["x"].get<int>() - other_object["x"].get<int>());
    int dy = abs(object["y"].get<int>() - other_object["y"].get<int>());
    return static_cast<int>(sqrt(dx * dx + dy * dy));
}

inline int findClosestNumber(int number, const std::vector<int>& numbers) {
    int min_diff = std::abs(number - numbers[0]);
    int closest_num = numbers[0];

    for (int i = 1; i < numbers.size(); ++i) {
        int diff = std::abs(number - numbers[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest_num = numbers[i];
        }
    }

    return closest_num;
}

inline int calculateXDistance(json& object, json& other_object) {
    int returnableinpixels = abs(object["x"].get<int>() - other_object["x"].get<int>());
    return returnableinpixels;
}

inline int calculateYDistance(json& object, json& other_object) {
    int returnableinpixels = abs(object["y"].get<int>() - other_object["y"].get<int>());
    return returnableinpixels;
}
/*
0 to 90 degrees: North-East
90 to 180 degrees: South-East
180 to 270 degrees: South-West
270 to 360 degrees: North-West
*/
inline float calculateDirectionDegrees(json& object, json& other_object) {
    float distX = static_cast<float>(calculateXDistance(object, other_object));
    float distY = static_cast<float>(calculateYDistance(object, other_object));
    float angleRadians = atan2f(distY, distX);
    float angleDegrees = DEG2RAD * angleRadians;
    if (angleDegrees < 0) {
        angleDegrees += 360.0f;
    }
    return angleDegrees;
}
//use timer to do it so less wasting resources btw
template <typename T>
T findPixelToGoTo(nlohmann::json& playersarray, nlohmann::json& enemy) {
    int smallestyet = 0;
    int smallestDistance = calculateDistance(playersarray[0], enemy);
    for (int it = 1; it < playersarray.size(); it++) {
        int dist = calculateDistance(playersarray[it], enemy);
        if (dist < smallestDistance) {
            smallestDistance = dist;
            smallestyet = it;
        }
    }
    int degrees = static_cast<int>(calculateDirectionDegrees(playersarray[smallestyet], enemy));
    
    vector<int> nums = {0, 45, 90, 135, 180, 225, 270, 315, 360};
    int dirtogo = findClosestNumber(degrees, nums);

    string dirnametogo;
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
