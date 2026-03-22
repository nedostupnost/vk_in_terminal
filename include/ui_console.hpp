#pragma once
#include <notcurses/notcurses.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

struct NcDeleter { 
    void operator()(notcurses* n) const { if (n) notcurses_stop(n); } 
};

class UIConsole {
public:
    UIConsole();
    ~UIConsole() = default;    
    void printMessage(std::string_view sender, std::string_view text);
    void printSystem(std::string_view text);
    void showTyping(std::string_view sender);
    void clearSystem();
    void printMedia(std::string_view sender, std::string_view filepath);
    void updateContacts(const std::vector<std::pair<std::string, bool>>& contacts);
    void clearChat();
    std::string getUserInput();

private:
    std::unique_ptr<notcurses, NcDeleter> nc;
    struct ncplane* std_plane = nullptr;       
    struct ncplane* chat_bdr = nullptr;        
    struct ncplane* chat_plane = nullptr;      
    struct ncplane* contacts_bdr = nullptr;    
    struct ncplane* contacts_plane = nullptr;  
    struct ncplane* input_bdr = nullptr;       
    struct ncplane* input_plane = nullptr;     
    struct ncplane* status_plane = nullptr;    

    std::vector<std::pair<std::string, bool>> cached_contacts;
    
    unsigned int term_height = 0;
    unsigned int term_width = 0;

    void drawBorders();
    void drawContactsText();
};