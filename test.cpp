#include <iostream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

class Solution {
public:
    vector<vector<int>> validArrangement(vector<vector<int>>& pairs) {
        std::vector<std::vector<int>> result;
        std::vector<bool> used(pairs.size(), false);
        //get first pair
        result.push_back(pairs[0]);
        used[0] = true;

        //build chain 
        while (result.size() < pairs.size()) {
            int last = result.back()[1]; //last vlaue of current chain
            for (size_t i = 0; i < pairs.size(); ++i) {
                if (!used[i] && pairs[i][0] == last) {
                    result.push_back(pairs[i]);
                    used[i] = true;
                    break;
                }
            }
        }
        return result;
    }
};

int main() {
    std::cout << "Enter pairs in the format [x1,y1] [x2,y2] ... or type 'default' to use [[5,1],[4,5],[11,9],[9,4]]: ";
    std::string inputLine;
    std::getline(std::cin, inputLine);

    std::vector<std::vector<int>> pairs;

    if (inputLine == "default") {
        // Use default array
        pairs = {{5, 1}, {4, 5}, {11, 9}, {9, 4}};
    } else {
        // Parse input into a vector of pairs
        std::stringstream ss(inputLine);
        std::string pairStr;

        while (ss >> pairStr) {
            if (pairStr.front() == '[' && pairStr.back() == ']') {
                pairStr = pairStr.substr(1, pairStr.size() - 2); // Remove [ and ]
                std::stringstream pairStream(pairStr);
                std::string x, y;
                if (std::getline(pairStream, x, ',') && std::getline(pairStream, y)) {
                    pairs.push_back({std::stoi(x), std::stoi(y)});
                }
            }
        }

        // Check if input is valid
        if (pairs.empty()) {
            std::cout << "Invalid input. Please provide pairs in the correct format." << std::endl;
            return 1;
        }
    }

    // Create a Solution object and call the validArrangement method
    Solution solution;
    std::vector<std::vector<int>> result = solution.validArrangement(pairs);

    // Output the result
    std::cout << "Valid arrangement:" << std::endl;
    for (const auto& pair : result) {
        std::cout << "[" << pair[0] << "," << pair[1] << "] ";
    }
    std::cout << std::endl;

    return 0;
}