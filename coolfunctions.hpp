// coolfunctions.hpp
#ifndef COOL_FUNCTIONS_HPP
#define COOL_FUNCTIONS_HPP

#include <string>
#include <cstdlib>
#include <sstream>
#include <type_traits>

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

// Function to check if a string contains a decimal point
bool hasDecimalPoint(const std::string& str) {
    return str.find('.') != std::string::npos;
}

// Function to convert string to int or float
template <typename T>
T convertString(const std::string& str) {
    std::stringstream ss(str);
    T result;
    ss >> result;
    return result;
}

class Number {
public:
    enum class Type {
        Integer,
        Float
    };

    Number(int value) : type(Type::Integer), intValue(value), floatValue(static_cast<float>(value)) {}
    Number(float value) : type(Type::Float), intValue(static_cast<int>(value)), floatValue(value) {}
    Number(const std::string& str) {
        if (hasDecimalPoint(str)) {
            type = Type::Float;
            floatValue = convertString<float>(str);
            intValue = static_cast<int>(floatValue);
        } else {
            type = Type::Integer;
            intValue = convertString<int>(str);
            floatValue = static_cast<float>(intValue);
        }
    }

    Type getType() const {
        return type;
    }

    int toInt() const {
        return intValue;
    }

    float toFloat() const {
        return floatValue;
    }

    std::string toString() const {
        if (type == Type::Integer) {
            return std::to_string(intValue);
        } else {
            return std::to_string(floatValue);
        }
    }

private:
    Type type;
    int intValue;
    float floatValue;
};

#endif