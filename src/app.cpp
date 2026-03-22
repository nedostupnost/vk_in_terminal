#include "app.hpp"
#include "logger.hpp"
#include <thread>

App::App(const std::string& token) 
    : api(token), 
      group_id((LOG("Вызов api.getGroupId()..."), api.getGroupId())), 
      lp((LOG("Инициализация LongPoll..."), api), group_id) 
{
    LOG("Конструктор App: инициализация завершена.");
}

std::string App::getUserNameSafe(int peer_id)
{
    std::lock_guard<std::mutex> lock(names_mutex);
    if (user_names.find(peer_id) == user_names.end())
    {
        user_names[peer_id] = api.getUserName(peer_id);
    }
    return user_names[peer_id];
}

void App::refreshContactsUI()
{
    std::vector<std::pair<std::string, bool>> display_contacts;
    for (const auto& conv : conversations)
    {
        bool has_unread = (unread_chats.find(conv.peer_id) != unread_chats.end());
        display_contacts.push_back({conv.name, has_unread});
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

void App::processCommand(const std::string& input)
{
    LOG("Введена команда: " + input);
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
        std::string filepath = "temp_media" + ext;
        
        if (api.downloadImage(last_media_url, filepath)) {
            std::string sender = (active_chat == 0) ? "Контакт" : getUserNameSafe(active_chat);
            ui.printMedia(sender, filepath);
        } else {
            ui.printSystem("ОШИБКА: Не удалось скачать файл.");
        }
        return;
    }

    if (input.length() >= 4 && input.substr(0, 3) == "/c ")
    {
        try
        {
            int idx = std::stoi(input.substr(3)) - 1;
            if (idx >= 0 && idx < (int)conversations.size())
            {
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

    if (input.length() > 5 && input.substr(0, 5) == "/img ")
    {
        std::string filepath = input.substr(5);
        ui.printSystem("Отправка картинки...");
        if (api.sendImage(active_chat, filepath))
        {
            ui.printSystem("УСПЕХ: Картинка отправлена!");
        } 
        else
        {
            ui.printSystem("ОШИБКА: Не удалось отправить картинку.");
        }
    } 
    else
    {
        ui.printMessage("Вы", input);
        api.sendMessage(active_chat, input);
    }
}

void App::run()
{
    LOG("App::run() -> Начало работы");
    if (group_id == 0)
    {
        LOG("ОШИБКА: group_id равен 0");
        ui.printSystem("ОШИБКА: Неверный токен или нет доступа к группе.");
        return;
    }

    LOG("Загрузка контактов...");
    ui.printSystem("Загрузка контактов...");
    conversations = api.getConversations(15);
    
    {
        std::lock_guard<std::mutex> lock(names_mutex);
        for (const auto& conv : conversations)
        {
            user_names[conv.peer_id] = conv.name;
        }
    }
    refreshContactsUI();
    LOG("Контакты успешно загружены.");

    LOG("Запуск фонового потока LongPoll...");
    std::thread listener_thread([this]()
    {
        LOG("Поток LongPoll успешно стартовал.");
        lp.listen(
            [this](int peer_id, const std::string& text, const std::string& att_type, const std::string& att_url)
            {
                std::string sender_name = getUserNameSafe(peer_id);
                if (!att_url.empty())
                {
                    last_media_type = att_type;
                    last_media_url = att_url;
                }
                if (peer_id == active_chat)
                {
                    if (!text.empty())
                    {
                        ui.clearSystem();
                        ui.printMessage(sender_name, text);
                    }
                }
                else
                {
                    unread_chats.insert(peer_id);
                    refreshContactsUI();
                    ui.printSystem("Новое сообщение от: " + sender_name);
                }
            },
            [this](int peer_id) {
                std::string sender_name = getUserNameSafe(peer_id);
                if (peer_id == active_chat) {
                    ui.showTyping(sender_name);
                }
            }
        );
    });
    
    listener_thread.detach();
    LOG("Поток отсоединен. Переход в главный цикл.");

    while (true)
    {
        std::string user_input = ui.getUserInput();
        if (user_input.empty()) continue;
        processCommand(user_input);
    }
}