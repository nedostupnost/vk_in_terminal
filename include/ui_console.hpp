#pragma once
#include <notcurses/notcurses.h>
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
    void showTyping(const std::string& sender);
    void clearSystem();
    void printMedia(const std::string& sender, const std::string& filepath);
    void updateContacts(const std::vector<std::pair<std::string, bool>>& contacts);
    void clearChat();
    std::string getUserInput();

private:
    struct notcurses* nc = nullptr;
    struct ncplane* std_plane = nullptr;       
    struct ncplane* chat_bdr = nullptr;        
    struct ncplane* chat_plane = nullptr;      
    struct ncplane* contacts_bdr = nullptr;    
    struct ncplane* contacts_plane = nullptr;  
    struct ncplane* input_bdr = nullptr;       
    struct ncplane* input_plane = nullptr;     
    struct ncplane* status_plane = nullptr;    

    std::recursive_mutex ui_mutex;
    std::atomic<int> status_msg_id{0};
    std::ofstream log_file;
    
    std::vector<std::pair<std::string, bool>> cached_contacts;
    
    unsigned int term_height = 0;
    unsigned int term_width = 0;

    void drawBorders();
    void drawContactsText();
};