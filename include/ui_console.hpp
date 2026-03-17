#pragma once
#include <ncurses.h>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>
#include <fstream>

class UIConsole {
public:
    UIConsole();
    ~UIConsole();

    void printMessage(const std::string& sender, const std::string& text);
    void printSystem(const std::string& text);
    
    void updateContacts(const std::vector<std::pair<std::string, bool>>& contacts);
    void clearChat();
    
    std::string getUserInput();

private:
    WINDOW* chat_bdr;
    WINDOW* chat_pad;
    WINDOW* contacts_bdr;
    WINDOW* contacts_win;
    WINDOW* status_win;
    WINDOW* input_win;

    std::recursive_mutex ui_mutex;
    std::atomic<int> status_msg_id{0};
    std::ofstream log_file;

    std::vector<std::pair<std::string, bool>> cached_contacts;
    int chat_view_h;
    int chat_view_w;
    int scroll_offset;
    int current_pad_y;

    void drawBorders();
    void drawContactsText();
    void refreshChat();
    void scrollChat(int direction);
    std::string sanitizeText(const std::string& text);
};