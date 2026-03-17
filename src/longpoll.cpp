#include "longpoll.hpp"
#include <thread>

using json = nlohmann::json;

LongPoll::LongPoll(VkApi& api, int group_id) : vk(api), group_id(group_id) { updateServerInfo(); }

bool LongPoll::updateServerInfo() {
    json response = vk.call("groups.getLongPollServer", cpr::Parameters{{"group_id", std::to_string(group_id)}});
    if (response.contains("response")) {
        server = response["response"]["server"];
        key = response["response"]["key"];
        ts = response["response"]["ts"];
        return true;
    }
    return false;
}

void LongPoll::listen(std::function<void(int, const std::string&, const std::string&, const std::string&)> on_message) {
    while (true) {
        cpr::Response r = cpr::Get(cpr::Url{server}, cpr::Parameters{{"act", "a_check"}, {"key", key}, {"ts", ts}, {"wait", "25"}});
        if (r.status_code == 200) {
            json resp = json::parse(r.text);
            if (resp.contains("failed")) {
                if (resp["failed"] == 1) ts = resp["ts"]; else updateServerInfo();
                continue;
            }
            ts = resp["ts"];
            for (const auto& update : resp["updates"]) {
                if (update["type"] == "message_new") {
                    auto msg = update["object"]["message"];
                    std::string a_t = "", a_u = "";
                    if (msg.contains("attachments") && !msg["attachments"].empty()) {
                        auto att = msg["attachments"][0];
                        if (att["type"] == "photo") { a_t = "photo"; a_u = att["photo"]["sizes"].back()["url"]; }
                        else if (att["type"] == "audio_message") { a_t = "audio"; a_u = att["audio_message"]["link_mp3"]; }
                    }
                    on_message(msg.value("peer_id", 0), msg.value("text", "") + VkApi::parseAttachments(msg), a_t, a_u);
                }
            }
        } else { std::this_thread::sleep_for(std::chrono::seconds(1)); }
    }
}