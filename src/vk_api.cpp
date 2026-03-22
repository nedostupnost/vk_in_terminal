#include "vk_api.hpp"
#include "logger.hpp"
#include <fstream>
#include <random>

using json = nlohmann::json;

VkApi::VkApi(const std::string& token) : access_token(token) {
    LOG("VkApi: объект создан");
}

std::string VkApi::parseAttachments(const nlohmann::json& msg) {
    std::string res = "";
    if (msg.contains("attachments") && msg["attachments"].is_array()) {
        for (const auto& att : msg["attachments"]) {
            std::string type = att["type"];
            if (type == "photo") res += " [Фото]";
            else if (type == "video") res += " [Видео]";
            else if (type == "audio") {
                res += " [Аудио: " + att["audio"].value("artist", "Неизвестный") + " - " + att["audio"].value("title", "Трек") + "]";
            }
            else if (type == "audio_message") res += " [Голосовое сообщение]";
            else if (type == "doc") res += " [Документ: " + att["doc"].value("title", "Файл") + "]";
            else if (type == "sticker") res += " [Стикер]";
            else res += " [Вложение: " + type + "]";
        }
    }
    return res;
}

json VkApi::call(const std::string& method, cpr::Parameters params) {
    LOG("VkApi::call -> Вызов метода: " + method);
    std::string url = "https://api.vk.com/method/" + method;
    params.Add({"access_token", access_token});
    params.Add({"v", api_version});
    
    cpr::Response r = cpr::Post(cpr::Url{url}, params);
    
    LOG("VkApi::call -> Ответ HTTP код: " + std::to_string(r.status_code));
    LOG("VkApi::call -> СЫРОЙ ОТВЕТ ВК: " + r.text); // <--- Ловим ошибки ВК
    
    if (r.status_code == 200) {
        try {
            return json::parse(r.text);
        } catch (...) {
            LOG("VkApi::call -> ОШИБКА: ВК вернул невалидный JSON!");
            return json({});
        }
    }
    return json({});
}

int VkApi::getGroupId() {
    LOG("VkApi::getGroupId -> Старт");
    json response = call("groups.getById");
    
    if (response.contains("error")) {
        LOG("VkApi::getGroupId -> ВК ВЕРНУЛ ОШИБКУ: " + response["error"].dump());
        return 0;
    }

    // Обработка современного формата API 5.199+
    if (response.contains("response") && response["response"].is_object() && response["response"].contains("groups")) {
        if (response["response"]["groups"].is_array() && !response["response"]["groups"].empty()) {
            int id = response["response"]["groups"][0].value("id", 0);
            LOG("VkApi::getGroupId -> Успех, ID: " + std::to_string(id));
            return id;
        }
    }
    // Обработка старого формата (на случай, если ВК отдаст старый ответ)
    else if (response.contains("response") && response["response"].is_array() && !response["response"].empty()) {
        int id = response["response"][0].value("id", 0);
        LOG("VkApi::getGroupId -> Успех, ID: " + std::to_string(id));
        return id;
    }

    LOG("VkApi::getGroupId -> Ошибка: Не удалось найти ID в ответе");
    return 0;
}

std::string VkApi::getUserName(int user_id) {
    if (user_id < 0) return "Группа " + std::to_string(-user_id);
    json response = call("users.get", cpr::Parameters{{"user_ids", std::to_string(user_id)}});
    if (response.contains("response") && response["response"].is_array() && !response["response"].empty()) {
        return response["response"][0].value("first_name", "") + " " + response["response"][0].value("last_name", "");
    }
    return "ID " + std::to_string(user_id);
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
    json server_response = call("photos.getMessagesUploadServer", cpr::Parameters{{"peer_id", std::to_string(peer_id)}});
    if (!server_response.contains("response")) return false;
    std::string upload_url = server_response["response"]["upload_url"];
    cpr::Response upload_resp = cpr::Post(cpr::Url{upload_url}, cpr::Multipart{{"photo", cpr::File{filepath}}});
    if (upload_resp.status_code != 200) return false;
    json upload_result;
    try { upload_result = json::parse(upload_resp.text); } catch (...) { return false; }
    
    json save_response = call("photos.saveMessagesPhoto", cpr::Parameters{
        {"photo", upload_result.value("photo", "")},
        {"server", std::to_string(upload_result.value("server", 0))},
        {"hash", upload_result.value("hash", "")}
    });
    if (!save_response.contains("response")) return false;
    std::string attachment = "photo" + std::to_string(save_response["response"][0].value("owner_id", 0)) + "_" + std::to_string(save_response["response"][0].value("id", 0));
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
        if (fs.is_open()) { fs << r.text; fs.close(); return true; }
    }
    return false;
}

std::vector<VkContact> VkApi::getConversations(int count) {
    std::vector<VkContact> contacts;
    json response = call("messages.getConversations", cpr::Parameters{{"count", std::to_string(count)}});
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
    json response = call("messages.getHistory", cpr::Parameters{{"peer_id", std::to_string(peer_id)}, {"count", std::to_string(count)}});
    if (response.contains("response") && response["response"].contains("items")) {
        auto items = response["response"]["items"];
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            history.push_back({(*it).value("from_id", 0), (*it).value("text", "") + parseAttachments(*it)});
        }
    }
    return history;
}