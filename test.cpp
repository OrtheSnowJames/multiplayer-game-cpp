//test
#include <iostream>

int main() {
    int maybebool = 1;
    bool actualbool = true;
    while (maybebool) {
        std::cout << "Hello, World!" << std::endl;
        std::cout << std::to_string(actualbool) << std::endl;
        maybebool = 0;
    }
    return 0;
}