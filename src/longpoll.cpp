#include "longpoll.hpp"
#include "logger.hpp"
#include <thread>

using json = nlohmann::json;

LongPoll::LongPoll(VkApi& api, int group_id) : vk(api), group_id(group_id) { 
    LOG("LongPoll: объект создается, запрашиваем сервер...");
    updateServerInfo(); 
    LOG("LongPoll: объект успешно создан.");
}

bool LongPoll::updateServerInfo() {
    LOG("LongPoll::updateServerInfo -> Старт");
    json response = vk.call("groups.getLongPollServer", cpr::Parameters{{"group_id", std::to_string(group_id)}});
    if (response.contains("response")) {
        server = response["response"]["server"];
        key = response["response"]["key"];
        ts = response["response"]["ts"];
        LOG("LongPoll::updateServerInfo -> Успех");
        return true;
    }
    LOG("LongPoll::updateServerInfo -> Ошибка");
    return false;
}

void LongPoll::listen(
    std::function<void(int, const std::string&, const std::string&, const std::string&)> on_message,
    std::function<void(int)> on_typing) 
{
    LOG("LongPoll::listen -> Вошли в цикл прослушивания");
    while (true) {
        if (server.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        try {
            cpr::Response r = cpr::Get(cpr::Url{server}, cpr::Parameters{{"act", "a_check"}, {"key", key}, {"ts", ts}, {"wait", "25"}});
            if (r.status_code == 200) {
                json resp = json::parse(r.text);
                if (resp.contains("failed")) {
                    updateServerInfo();
                    continue;
                }
                ts = resp["ts"];
            }
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}