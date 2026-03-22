#include "app.hpp"
#include "logger.hpp"
#include <thread>
#include <filesystem>

App::App(std::string token) 
    : api(std::move(token)), 
      group_id(api.getGroupId()), 
      lp(api, group_id.value_or(0)) {}

std::string App::getUserNameSafe(int peer_id)
{
    {
        std::shared_lock read_lock(names_mutex);
        if (auto it = user_names.find(peer_id); it != user_names.end())
        {
            return it->second;
        }
    }
    std::string name = api.getUserName(peer_id);
    {
        std::scoped_lock write_lock(names_mutex);
        user_names[peer_id] = name;
    }
    return name;
}

void App::refreshContactsUI()
{
    std::vector<std::pair<std::string, bool>> display_contacts;
    for (const auto& conv : conversations) {
        bool has_unread = (unread_chats.find(conv.peer_id) != unread_chats.end());
        display_contacts.emplace_back(conv.name, has_unread);
    }
    ui.updateContacts(display_contacts);
}

void App::loadChatHistory(int peer_id)
{
    ui.clearChat();
    ui.printSystem("Загрузка истории чата...");
    auto history = api.getChatHistory(peer_id, 20);
    ui.clearChat();
    for (const auto& msg : history)
    {
        std::string s_name = (msg.sender_id < 0) ? "Вы" : getUserNameSafe(msg.sender_id);
        ui.printMessage(s_name, msg.text);
    }
}

void App::processCommand(std::string_view input)
{
    if (input == "/exit") exit(0);
    
    if (input == "/open")
    {
        if (last_media_url.empty())
        {
            ui.printSystem("Нет медиафайлов для открытия.");
            return;
        }
        ui.printSystem("Скачивание медиафайла...");
        std::string ext = (last_media_type == "photo") ? ".jpg" : ".mp3";

        std::filesystem::path filepath = std::filesystem::temp_directory_path() / ("temp_media" + ext);
        
        if (api.downloadImage(last_media_url, filepath.string()))
        {
            std::string sender = (active_chat == 0) ? "Контакт" : getUserNameSafe(active_chat);
            ui.printMedia(sender, filepath.string());
        }
        else
        {
            ui.printSystem("ОШИБКА: Не удалось скачать файл.");
        }
        return;
    }

    if (input.length() >= 4 && input.substr(0, 3) == "/c ")
    {
        try
        {
            int idx = std::stoi(std::string(input.substr(3))) - 1;
            if (idx >= 0 && idx < static_cast<int>(conversations.size())) {
                active_chat = conversations[idx].peer_id;
                unread_chats.erase(active_chat);
                refreshContactsUI();
                loadChatHistory(active_chat);
            }
        } catch (...) { }
        return;
    }

    if (active_chat == 0)
    {
        ui.printSystem("Выберите контакт командой: /c номер");
        return;
    }

    ui.printMessage("Вы", std::string(input));
    api.sendMessage(active_chat, input);
}

void App::run()
{
    if (!group_id.has_value() || group_id.value() == 0)
    {
        ui.printSystem("ОШИБКА: Неверный токен или нет доступа к группе.");
        return;
    }

    ui.printSystem("Загрузка контактов...");
    conversations = api.getConversations(15);
    
    {
        std::scoped_lock lock(names_mutex);
        for (const auto& conv : conversations) user_names[conv.peer_id] = conv.name;
    }
    refreshContactsUI();

    std::thread listener_thread([this]()
    {
        lp.listen(
            [this](int peer_id, const std::string& text, const std::string& att_type, const std::string& att_url)
            {
                std::scoped_lock lock(queue_mutex);
                event_queue.push(MessageEvent{peer_id, text, att_type, att_url});
            },
            [this](int peer_id)
            {
                std::scoped_lock lock(queue_mutex);
                event_queue.push(TypingEvent{peer_id});
            }
        );
    });
    listener_thread.detach();

    while (true)
    {
        std::string user_input = ui.getUserInput();
        if (!user_input.empty()) processCommand(user_input);

        std::queue<AppEvent> local_queue;
        {
            std::scoped_lock lock(queue_mutex);
            std::swap(local_queue, event_queue);
        }

        while (!local_queue.empty())
        {
            auto event = std::move(local_queue.front());
            local_queue.pop();

            std::visit([this](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;
                
                if constexpr (std::is_same_v<T, MessageEvent>)
                {
                    std::string sender_name = getUserNameSafe(arg.peer_id);
                    if (!arg.att_url.empty())
                    {
                        last_media_type = arg.att_type;
                        last_media_url = arg.att_url;
                    }
                    if (arg.peer_id == active_chat)
                    {
                        if (!arg.text.empty())
                        {
                            ui.clearSystem();
                            ui.printMessage(sender_name, arg.text);
                        }
                    }
                    else
                    {
                        unread_chats.insert(arg.peer_id);
                        refreshContactsUI();
                        ui.printSystem("Новое сообщение от: " + sender_name);
                    }
                } 
                else if constexpr (std::is_same_v<T, TypingEvent>)
                {
                    if (arg.peer_id == active_chat)
                    {
                        ui.showTyping(getUserNameSafe(arg.peer_id));
                    }
                }
            }, event);
        }
    }
}