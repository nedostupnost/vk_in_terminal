#pragma once
#include <ncurses.h>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>

class UIConsole {
public:
    UIConsole();
    ~UIConsole();

    void printMessage(const std::string& sender, const std::string& text);
    void printSystem(const std::string& text);
    
    void updateContacts(const std::vector<std::string>& contacts);
    void clearChat();
    
    std::string getUserInput();

private:
    WINDOW* chat_bdr;
    WINDOW* chat_win;
    WINDOW* contacts_bdr;
    WINDOW* contacts_win;
    WINDOW* status_win;
    WINDOW* input_win;

    std::recursive_mutex ui_mutex;
    std::atomic<int> status_msg_id{0};

    void drawBorders();
};