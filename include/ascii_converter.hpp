#pragma once
#include <string>

class AsciiConverter {
public:
    static std::string convert(const std::string& filepath, int max_width = 80);
};