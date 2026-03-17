#include "longpoll.hpp"
#include <iostream>

using json = nlohmann::json;

LongPoll::LongPoll(VkApi& api, int group_id) : vk(api), group_id(group_id) {
    updateServerInfo();
}

bool LongPoll::updateServerInfo() {
    json response = vk.call("groups.getLongPollServer", cpr::Parameters{
        {"group_id", std::to_string(group_id)}
    });

    if (response.contains("response")) {
        server = response["response"]["server"];
        key = response["response"]["key"];
        ts = response["response"]["ts"];
        return true;
    }
    return false;
}

void LongPoll::listen(std::function<void(int peer_id, const std::string& text, const std::string& photo_url)> on_message) {
    while (true) {
        cpr::Response r = cpr::Get(cpr::Url{server}, cpr::Parameters{
            {"act", "a_check"},
            {"key", key},
            {"ts", ts},
            {"wait", "25"}
        });

        if (r.status_code == 200) {
            json response = json::parse(r.text);

            if (response.contains("failed")) {
                int failed = response["failed"];
                if (failed == 1) ts = response["ts"];
                else updateServerInfo();
                continue;
            }

            ts = response["ts"];

            for (const auto& update : response["updates"]) {
                if (update["type"] == "message_new") {
                    auto message = update["object"]["message"];
                    int peer_id = message["peer_id"];
                    std::string text = message["text"];
                    std::string photo_url = "";

                    if (message.contains("attachments")) {
                        for (const auto& attachment : message["attachments"]) {
                            if (attachment["type"] == "photo") {
                                auto sizes = attachment["photo"]["sizes"];
                                photo_url = sizes.back()["url"];
                            }
                        }
                    }
                    
                    on_message(peer_id, text, photo_url);
                }
            }
        }
    }
}