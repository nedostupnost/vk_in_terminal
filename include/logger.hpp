#pragma once
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <iostream>

inline void LOG(const std::string& msg) {
    std::ofstream log_file("debug_crash.log", std::ios::app);
    if (log_file.is_open())
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        log_file << "[" << std::put_time(std::localtime(&in_time_t), "%H:%M:%S") << "] " << msg << std::endl;
    }
}