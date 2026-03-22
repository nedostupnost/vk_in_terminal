#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

struct VkContact { int peer_id; std::string name; };
struct VkMessage { int sender_id; std::string text; };

class VkApi {
public:
    explicit VkApi(std::string token);
    nlohmann::json call(std::string_view method, cpr::Parameters params = cpr::Parameters{});
    
    std::optional<int> getGroupId();
    std::string getUserName(int user_id);
    
    bool sendMessage(int peer_id, std::string_view text);
    bool sendImage(int peer_id, std::string_view filepath, std::string_view text = "");
    bool downloadImage(std::string_view url, std::string_view filepath);
    
    std::vector<VkContact> getConversations(int count = 10);
    std::vector<VkMessage> getChatHistory(int peer_id, int count = 20);
    static std::string parseAttachments(const nlohmann::json& msg);

private:
    std::string access_token;
    std::string api_version = "5.199";
};