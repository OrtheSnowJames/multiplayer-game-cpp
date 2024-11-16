#include <iostream>

int main(){
    bool testenv = std::getenv("TEXTENV");
    if (testenv == NULL){
        std::cout << "TEXTENV is not set" << std::endl;
    } else {
        std::cout << "TEXTENV is set to " << testenv << std::endl;
    }
    return 0;
}