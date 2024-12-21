#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP

#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <raylib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Calculate the Euclidean distance between two objects that have x,y.
 */
inline int calculateDistance(const json& object, const json& other_object) {
    int dx = object.at("x").get<int>() - other_object.at("x").get<int>();
    int dy = object.at("y").get<int>() - other_object.at("y").get<int>();
    return static_cast<int>(std::sqrt(dx * dx + dy * dy));
}

/**
 * Calculate the angle in degrees from 'object' to 'other_object'.
 * 
 * Here we use atan2(-dy, dx) so that:
 *   - angle = 0   => north (i.e., up)
 *   - angle = 90  => east
 *   - angle = 180 => south
 *   - angle = 270 => west
 */
inline float calculateDirectionDegrees(const json& object, const json& other_object) {
    float dx = static_cast<float>(other_object.at("x").get<int>() - object.at("x").get<int>());
    float dy = static_cast<float>(other_object.at("y").get<int>() - object.at("y").get<int>());
    
    // We negate `dy` so that 0 degrees is "north" (i.e. up).
    float angleRadians = std::atan2f(-dy, dx);
    float angleDegrees = angleRadians * RAD2DEG;
    if (angleDegrees < 0.0f) {
        angleDegrees += 360.0f;
    }
    return angleDegrees;
}

/**
 * Given a number (an angle) and a list of "standard angles," find the closest angle.
 */
inline int findClosestNumber(int number, const std::vector<int>& numbers) {
    return *std::min_element(numbers.begin(), numbers.end(),
        [number](int a, int b) {
            return std::abs(number - a) < std::abs(number - b);
        });
}

/**
 * Find which "direction label" to move toward so the enemy can go toward
 * the closest player, snapping angles to 45-degree increments.
 *
 * Returns one of:
 *   "north", "north-east", "east", "south-east",
 *   "south", "south-west", "west", "north-west"
 * or an empty string if playersArray is empty.
 */
inline std::string findPixelToGoTo(const json& playersArray, const json& enemy) {
    if (playersArray.empty()) {
        return "";
    }

    // Find the closest player to this enemy
    auto closestPlayerIt = std::min_element(playersArray.begin(), playersArray.end(),
        [&enemy](const json& a, const json& b) {
            return calculateDistance(enemy, a) < calculateDistance(enemy, b);
        });

    // Calculate direction from enemy -> closest player
    int degrees = static_cast<int>(calculateDirectionDegrees(enemy, *closestPlayerIt));

    // Snap to nearest 45-degree increment
    std::vector<int> standardDirections = {0, 45, 90, 135, 180, 225, 270, 315, 360};
    int dirToGo = findClosestNumber(degrees, standardDirections);

    // Map those angles to direction strings
    static const std::vector<std::pair<int, std::string>> directionMap = {
        {0,   "north"},      // 0 degrees => up
        {45,  "north-east"},
        {90,  "east"},
        {135, "south-east"},
        {180, "south"},
        {225, "south-west"},
        {270, "west"},
        {315, "north-west"},
        {360, "north"}  // Snap 360 back to "north"
    };

    auto it = std::find_if(directionMap.begin(), directionMap.end(),
        [dirToGo](const std::pair<int, std::string>& pair) {
            return pair.first == dirToGo;
        });

    return (it != directionMap.end()) ? it->second : "";
}

#endif // PATHFINDING_HPP
