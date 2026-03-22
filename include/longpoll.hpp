#pragma once
#include "vk_api.hpp"
#include <functional>
#include <string>

class LongPoll {
public:
    LongPoll(VkApi& api, int group_id);
    void listen(
        std::function<void(int peer_id, const std::string& text, const std::string& att_type, const std::string& att_url)> on_message,
        std::function<void(int peer_id)> on_typing
    );

private:
    VkApi& vk;
    int group_id;
    std::string server, key, ts;
    bool updateServerInfo();
};