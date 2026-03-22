#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>
#include "vk_api.hpp"
#include "longpoll.hpp"
#include "ui_console.hpp"

class App {
public:
    explicit App(const std::string& token);
    void run();

private:
    VkApi api;
    int group_id;
    LongPoll lp;
    UIConsole ui;
    
    std::atomic<int> active_chat{0};
    
    std::vector<VkContact> conversations;
    std::unordered_map<int, std::string> user_names;
    std::set<int> unread_chats; 
    std::mutex names_mutex; 
    
    std::string last_media_url;
    std::string last_media_type;

    std::string getUserNameSafe(int peer_id);
    void refreshContactsUI();
    void processCommand(const std::string& input);
    void loadChatHistory(int peer_id);
};