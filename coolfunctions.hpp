// coolfunctions.h
#ifndef COOL_FUNCTIONS_H
#define COOL_FUNCTIONS_H

#include <string>
#include <cstdlib>
#include <sstream>

template <typename T>
T getEnvVar(const std::string& varName, const T& defaultValue) {
    const char* envValue = std::getenv(varName.c_str());
    if (envValue == nullptr) {
        return defaultValue;
    }

    std::stringstream ss(envValue);
    T result;
    if constexpr (std::is_same<T, bool>::value) {
        std::string valueStr = envValue;
        return (valueStr == "true" || valueStr == "1");
    } else if constexpr (std::is_same<T, std::string>::value) {
        return std::string(envValue);
    } else {
        ss >> result;
        if (ss.fail()) {
            return defaultValue;
        }
        return result;
    }
}

#endif