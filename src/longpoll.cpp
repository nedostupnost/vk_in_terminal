#include "longpoll.hpp"
#include "logger.hpp"
#include <thread>

using json = nlohmann::json;

LongPoll::LongPoll(VkApi& api, int group_id) : vk(api), group_id(group_id)
{ 
    LOG("LongPoll: объект создается, запрашиваем сервер...");
    updateServerInfo(); 
    LOG("LongPoll: объект успешно создан.");
}

bool LongPoll::updateServerInfo()
{
    LOG("LongPoll::updateServerInfo -> Старт");
    json response = vk.call("groups.getLongPollServer", cpr::Parameters{{"group_id", std::to_string(group_id)}});
    if (response.contains("response"))
    {
        server = response["response"]["server"];
        key = response["response"]["key"];
        if (response["response"]["ts"].is_number()) ts = std::to_string(response["response"]["ts"].get<long long>());
        else if (response["response"]["ts"].is_string()) ts = response["response"]["ts"].get<std::string>();
        
        LOG("LongPoll::updateServerInfo -> Успех, ts: " + ts);
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
    while (true)
    {
        if (server.empty())
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            updateServerInfo();
            continue;
        }

        try {
            cpr::Response r = cpr::Get(cpr::Url{server}, cpr::Parameters{{"act", "a_check"}, {"key", key}, {"ts", ts}, {"wait", "25"}});
            if (r.status_code == 200)
            {
                json resp = json::parse(r.text);
                if (resp.contains("failed"))
                {
                    updateServerInfo();
                    continue;
                }
    
                if (resp["ts"].is_number()) ts = std::to_string(resp["ts"].get<long long>());
                else if (resp["ts"].is_string()) ts = resp["ts"].get<std::string>();

                for (const auto& update : resp["updates"])
                {
                    if (update["type"] == "message_new")
                    {
                        auto msg = update["object"]["message"];
                        std::string a_t = "", a_u = "";
                        
                        if (msg.contains("attachments") && msg["attachments"].is_array() && !msg["attachments"].empty())
                        {
                            auto att = msg["attachments"][0];
                            if (att["type"] == "photo" && att["photo"].contains("sizes") && !att["photo"]["sizes"].empty())
                            { 
                                a_t = "photo"; 
                                a_u = att["photo"]["sizes"].back()["url"]; 
                            }
                            else if (att["type"] == "audio_message")
                            { 
                                a_t = "audio"; 
                                a_u = att["audio_message"]["link_mp3"]; 
                            }
                        }
                        LOG("LongPoll: Входящее сообщение получено!");
                        on_message(msg.value("peer_id", 0), msg.value("text", "") + VkApi::parseAttachments(msg), a_t, a_u);
                    } 
                    else if (update["type"] == "message_typing_state")
                    {
                        int from_id = update["object"].value("from_id", 0);
                        if (from_id != 0) on_typing(from_id);
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception& e) {
            LOG(std::string("LongPoll::listen -> ОШИБКА ПАРСИНГА: ") + e.what());
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } catch (...) {
            LOG("LongPoll::listen -> НЕИЗВЕСТНАЯ ОШИБКА");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}