#include <iostream>
#include <fstream>
#include <filesystem>
#include "app.hpp"

int main() {
    std::filesystem::path token_path = "token.txt";
    
    if (!std::filesystem::exists(token_path))
    {
        std::cerr << "ОШИБКА: Файл token.txt не найден в директории: " 
                  << std::filesystem::current_path() << "\n";
        return 1;
    }
    
    std::ifstream f(token_path);
    std::string token;
    std::getline(f, token);
    
    if (token.empty())
    {
        std::cerr << "ОШИБКА: Файл token.txt пуст!\n";
        return 1;
    }
    
    App app(std::move(token));
    app.run();
    
    return 0;
}