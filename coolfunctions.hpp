// coolfunctions.hpp
#ifndef COOL_FUNCTIONS_HPP
#define COOL_FUNCTIONS_HPP

#include <string>
#include <cstdlib>
#include <sstream>
#include <type_traits>
#include <nlohmann/json.hpp>

//ex:
    //Find index of the element where "name" is "Bob"
    //int index = findIndex(jsonArray, "name", "Bob");

template <typename T>
int findIndex(const nlohmann::json& jsonArray, const std::string& key, const T& value) {
    for (size_t i = 0; i < jsonArray.size(); ++i) {
        const auto& item = jsonArray[i];

        // Check if the current element contains the key and if its value matches the given value
        if (item.contains(key)) {
            if constexpr (std::is_same<T, int>::value) {
                if (item[key].is_number_integer() && item[key].get<int>() == value) {
                    return i;
                }
            }
            else if constexpr (std::is_same<T, std::string>::value) {
                if (item[key].is_string() && item[key].get<std::string>() == value) {
                    return i;
                }
            }
            else if constexpr (std::is_same<T, bool>::value) {
                if (item[key].is_boolean() && item[key].get<bool>() == value) {
                    return i;
                }
            }
            // Add more type checks as needed (float, double, etc.)
        }
    }
    return -1; // Return -1 if the element is not found
}

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
        return (valueStr == "true" || valueStr == "1" || valueStr == "TRUE" || valueStr == "True");
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

class Timer {
public:
    Timer(double duration_seconds) 
        : duration_(std::chrono::duration<double>(duration_seconds)),
          start_time_(std::chrono::high_resolution_clock::now()) {}

    bool has_time_elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start_time_;
        return elapsed >= duration_;
    }

    void reset() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    double time_left() const {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start_time_;
        return (duration_ - elapsed).count();
    }

private:
    std::chrono::duration<double> duration_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

#endif