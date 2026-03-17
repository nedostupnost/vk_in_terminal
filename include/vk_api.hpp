#pragma once
#include <string>
#include <vector>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

struct VkContact {
    int peer_id;
    std::string name;
};

struct VkMessage {
    int sender_id;
    std::string text;
};

class VkApi {
public:
    explicit VkApi(const std::string& token);
    
    nlohmann::json call(const std::string& method, cpr::Parameters params = cpr::Parameters{});

    int getGroupId();
    std::string getUserName(int user_id);
    bool sendMessage(int peer_id, const std::string& text);
    bool sendImage(int peer_id, const std::string& filepath, const std::string& text = "");
    bool downloadImage(const std::string& url, const std::string& filepath);

    std::vector<VkContact> getConversations(int count = 10);
    std::vector<VkMessage> getChatHistory(int peer_id, int count = 20);

    static std::string parseAttachments(const nlohmann::json& msg);

private:
    std::string access_token;
    std::string api_version = "5.199";
};