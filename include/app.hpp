#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <variant>
#include "vk_api.hpp"
#include "longpoll.hpp"
#include "ui_console.hpp"

struct MessageEvent { int peer_id; std::string text; std::string att_type; std::string att_url; };
struct TypingEvent { int peer_id; };
using AppEvent = std::variant<MessageEvent, TypingEvent>;

class App {
public:
    explicit App(std::string token);
    void run();

private:
    VkApi api;
    std::optional<int> group_id;
    LongPoll lp;
    UIConsole ui;
    
    std::atomic<int> active_chat{0};
    
    std::vector<VkContact> conversations;
    std::unordered_map<int, std::string> user_names;
    std::shared_mutex names_mutex;

    std::queue<AppEvent> event_queue;
    std::mutex queue_mutex;
    
    std::set<int> unread_chats; 
    std::string last_media_url;
    std::string last_media_type;

    std::string getUserNameSafe(int peer_id);
    void refreshContactsUI();
    void processCommand(std::string_view input);
    void loadChatHistory(int peer_id);
};