#include "ascii_converter.hpp"
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.hpp"

std::string AsciiConverter::convert(const std::string& filepath, int max_width)
{
    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 3);
    
    if (!data) return "[ОШИБКА] Не удалось открыть файл изображения для ASCII.";

    const std::string ramp = " .:-=+*#%@";
    std::stringstream ss;

    float scale_factor = (float)max_width / (float)width;
    int target_width = max_width;
    int target_height = (int)(height * scale_factor * 0.5f);

    if (width < max_width)
    {
        target_width = width;
        target_height = (int)(height * 0.5f);
        scale_factor = 1.0f;
    }

    for (int y = 0; y < target_height; ++y)
    {
        for (int x = 0; x < target_width; ++x)
        {
            int src_x = (int)(x / scale_factor);
            int src_y = (int)(y / (scale_factor * 0.5f));

            if (src_x >= width) src_x = width - 1;
            if (src_y >= height) src_y = height - 1;

            int index = (src_y * width + src_x) * 3;
            int r = data[index];
            int g = data[index + 1];
            int b = data[index + 2];

            int brightness = (int)(0.2126f * r + 0.7152f * g + 0.0722f * b);
            int char_index = (brightness * ramp.length()) / 256;
            
            if (char_index >= ramp.length()) char_index = ramp.length() - 1; 
            
            char ascii_char = ramp[char_index];
            ss << "\033[38;2;" << r << ";" << g << ";" << b << "m" << ascii_char;
        }
        ss << "\033[0m\n"; 
    }

    stbi_image_free(data);
    return ss.str();
}