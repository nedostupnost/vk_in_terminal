#pragma once
#include "vk_api.hpp"
#include <functional>
#include <string>

class LongPoll {
public:
    LongPoll(VkApi& api, int group_id);
    
    void listen(std::function<void(int peer_id, const std::string& text, const std::string& photo_url)> on_message);

private:
    VkApi& vk;
    int group_id;
    std::string server;
    std::string key;
    std::string ts;

    bool updateServerInfo();
};