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

template<typename T = float>
class num {
private:
    float value;

public:
    num() : value(0.0f) {}
    num(float v) : value(v) {}
    num(double v) : value(static_cast<float>(v)) {}
    num(int v) : value(static_cast<float>(v)) {}
    
    // String constructor
    num(const std::string& str) {
        std::stringstream ss(str);
        float v;
        ss >> v;
        value = v;
    }

    // Conversion operator
    template<typename U>
    explicit operator U() const {
        if constexpr (std::is_same_v<U, int>) {
            return static_cast<int>(value);
        } else if constexpr (std::is_same_v<U, float>) {
            return value;
        } else if constexpr (std::is_same_v<U, double>) {
            return static_cast<double>(value);
        }
        return static_cast<U>(value);
    }

    // Basic arithmetic operators
    num operator+(const num& other) const { return num(value + other.value); }
    num operator-(const num& other) const { return num(value - other.value); }
    num operator*(const num& other) const { return num(value * other.value); }
    num operator/(const num& other) const { return num(value / other.value); }

    // Comparison operators
    bool operator==(const num& other) const { return value == other.value; }
    bool operator!=(const num& other) const { return value != other.value; }
    bool operator<(const num& other) const { return value < other.value; }
    bool operator>(const num& other) const { return value > other.value; }
    bool operator<=(const num& other) const { return value <= other.value; }
    bool operator>=(const num& other) const { return value >= other.value; }

    // Get raw value
    float get() const { return value; }
};

#endif