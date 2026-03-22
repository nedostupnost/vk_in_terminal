#include <fstream>
#include <iostream>
#include "app.hpp"
#include "logger.hpp"

int main() {
    LOG("=== ЗАПУСК ПРОГРАММЫ ===");
    LOG("Попытка открыть token.txt...");
    
    std::ifstream f("token.txt");
    std::string t;
    if (f.is_open()) {
        std::getline(f, t);
        LOG("Файл token.txt успешно прочитан.");
    } else {
        LOG("ОШИБКА: Не удалось открыть token.txt");
    }
    
    if (t.empty()) {
        std::cerr << "Файл token.txt не найден или пуст!\n";
        LOG("КРИТ: Токен пуст, завершение работы.");
        return 1;
    }
    
    LOG("Токен найден, создание объекта App...");
    App app(t);
    
    LOG("Объект App успешно создан, вызываем app.run()...");
    app.run();
    
    LOG("=== ШТАТНОЕ ЗАВЕРШЕНИЕ ПРОГРАММЫ ===");
    return 0;
}