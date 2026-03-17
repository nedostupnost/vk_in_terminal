#include "vk_api.hpp"
#include <iostream>
#include <fstream>
#include <random>

using json = nlohmann::json;

VkApi::VkApi(const std::string& token) : access_token(token) {}

std::string VkApi::parseAttachments(const nlohmann::json& msg) {
    std::string res = "";
    if (msg.contains("attachments")) {
        for (const auto& att : msg["attachments"]) {
            std::string type = att["type"];
            if (type == "photo") res += " [Фото]";
            else if (type == "video") res += " [Видео]";
            else if (type == "audio") {
                std::string artist = att["audio"].value("artist", "Неизвестный");
                std::string title = att["audio"].value("title", "Трек");
                res += " [Аудио: " + artist + " - " + title + "]";
            }
            else if (type == "doc") {
                std::string title = att["doc"].value("title", "Файл");
                res += " [Документ: " + title + "]";
            }
            else if (type == "sticker") res += " [Стикер]";
            else res += " [Вложение: " + type + "]";
        }
    }
    return res;
}

json VkApi::call(const std::string& method, cpr::Parameters params) {
    std::string url = "https://api.vk.com/method/" + method;
    params.Add({"access_token", access_token});
    params.Add({"v", api_version});

    cpr::Response r = cpr::Post(cpr::Url{url}, params);

    if (r.status_code == 200) {
        return json::parse(r.text);
    }
    return json({});
}

int VkApi::getGroupId() {
    json response = call("groups.getById");
    if (response.contains("response")) {
        return response["response"]["groups"][0]["id"];
    }
    return 0;
}

std::string VkApi::getUserName(int user_id) {
    json response = call("users.get", cpr::Parameters{
        {"user_ids", std::to_string(user_id)}
    });

    if (response.contains("response") && !response["response"].empty()) {
        std::string first_name = response["response"][0]["first_name"];
        std::string last_name = response["response"][0]["last_name"];
        return first_name + " " + last_name;
    }
    return "Неизвестный ID " + std::to_string(user_id);
}

bool VkApi::sendMessage(int peer_id, const std::string& text) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 2147483647);

    json response = call("messages.send", cpr::Parameters{
        {"peer_id", std::to_string(peer_id)},
        {"message", text},
        {"random_id", std::to_string(dist(gen))}
    });

    return !response.contains("error");
}

bool VkApi::sendImage(int peer_id, const std::string& filepath, const std::string& text) {
    json server_response = call("photos.getMessagesUploadServer", cpr::Parameters{
        {"peer_id", std::to_string(peer_id)}
    });
    
    if (!server_response.contains("response")) return false;
    std::string upload_url = server_response["response"]["upload_url"];

    cpr::Response upload_resp = cpr::Post(cpr::Url{upload_url},
                                          cpr::Multipart{{"photo", cpr::File{filepath}}});
                                          
    if (upload_resp.status_code != 200) return false;
    
    json upload_result = json::parse(upload_resp.text);
    if (!upload_result.contains("photo") || upload_result["photo"].empty()) return false;

    json save_response = call("photos.saveMessagesPhoto", cpr::Parameters{
        {"photo", upload_result["photo"].get<std::string>()},
        {"server", std::to_string(upload_result["server"].get<int>())},
        {"hash", upload_result["hash"].get<std::string>()}
    });

    if (!save_response.contains("response")) return false;

    std::string owner_id = std::to_string(save_response["response"][0]["owner_id"].get<int>());
    std::string photo_id = std::to_string(save_response["response"][0]["id"].get<int>());
    std::string attachment = "photo" + owner_id + "_" + photo_id;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 2147483647);

    json send_response = call("messages.send", cpr::Parameters{
        {"peer_id", std::to_string(peer_id)},
        {"message", text},
        {"attachment", attachment},
        {"random_id", std::to_string(dist(gen))}
    });

    return !send_response.contains("error");
}

bool VkApi::downloadImage(const std::string& url, const std::string& filepath) {
    cpr::Response r = cpr::Get(cpr::Url{url});
    if (r.status_code == 200) {
        std::ofstream fs(filepath, std::ios::binary);
        if (fs.is_open()) {
            fs << r.text;
            fs.close();
            return true;
        }
    }
    return false;
}

std::vector<VkContact> VkApi::getConversations(int count) {
    std::vector<VkContact> contacts;
    json response = call("messages.getConversations", cpr::Parameters{
        {"count", std::to_string(count)}
    });

    if (response.contains("response") && response["response"].contains("items")) {
        for (const auto& item : response["response"]["items"]) {
            int peer_id = item["conversation"]["peer"]["id"];
            contacts.push_back({peer_id, getUserName(peer_id)});
        }
    }
    return contacts;
}

std::vector<VkMessage> VkApi::getChatHistory(int peer_id, int count) {
    std::vector<VkMessage> history;
    json response = call("messages.getHistory", cpr::Parameters{
        {"peer_id", std::to_string(peer_id)},
        {"count", std::to_string(count)}
    });

    if (response.contains("response") && response["response"].contains("items")) {
        auto items = response["response"]["items"];
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            int from_id = (*it)["from_id"];
            std::string text = (*it)["text"];
            text += parseAttachments(*it);
            history.push_back({from_id, text});
        }
    }
    return history;
}