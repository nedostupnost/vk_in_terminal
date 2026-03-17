#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <set>
#include "vk_api.hpp"
#include "longpoll.hpp"
#include "ui_console.hpp"

std::string readToken(const std::string& path) {
    std::ifstream file(path);
    std::string token;
    if (file.is_open()) std::getline(file, token);
    return token;
}

int main() {
    std::string token = readToken("token.txt");
    if (token.empty()) return 1;

    VkApi api(token);
    int group_id = api.getGroupId();
    if (group_id == 0) return 1;
    
    UIConsole ui;
    ui.printSystem("Загрузка контактов...");

    auto conversations = api.getConversations(15);
    std::unordered_map<int, std::string> user_names;
    std::set<int> unread_chats; 

    for (const auto& conv : conversations) {
        user_names[conv.peer_id] = conv.name;
    }

    auto refresh_contacts_ui = [&]() {
        std::vector<std::pair<std::string, bool>> display_contacts;
        for (const auto& conv : conversations) {
            bool has_unread = (unread_chats.find(conv.peer_id) != unread_chats.end());
            display_contacts.push_back({conv.name, has_unread});
        }
        ui.updateContacts(display_contacts);
    };

    refresh_contacts_ui();

    LongPoll lp(api, group_id);
    std::atomic<int> active_chat{0};

    std::thread listener_thread([&]() {
        lp.listen([&](int peer_id, const std::string& text, const std::string& att_type, const std::string& att_url) {
            if (user_names.find(peer_id) == user_names.end()) {
                user_names[peer_id] = api.getUserName(peer_id);
            }
            
            if (peer_id == active_chat) {
                if (!text.empty()) ui.printMessage(user_names[peer_id], text);
            } else {
                unread_chats.insert(peer_id);
                refresh_contacts_ui();
                ui.printSystem("Новое сообщение от: " + user_names[peer_id]);
            }
        });
    });

    while (true) {
        std::string user_input = ui.getUserInput();
        if (user_input.empty()) continue;

        if (user_input == "/exit") break;

        if (user_input.length() >= 4 && user_input.substr(0, 3) == "/c ") {
            try {
                int idx = std::stoi(user_input.substr(3)) - 1;
                if (idx >= 0 && idx < conversations.size()) {
                    active_chat = conversations[idx].peer_id;
                    
                    unread_chats.erase(active_chat);
                    refresh_contacts_ui();

                    ui.clearChat();
                    ui.printSystem("Загрузка истории чата...");
                    
                    auto history = api.getChatHistory(active_chat, 20);
                    ui.clearChat();
                    for (const auto& msg : history) {
                        std::string s_name = (msg.sender_id < 0) ? "Вы" : user_names[msg.sender_id];
                        ui.printMessage(s_name, msg.text);
                    }
                }
            } catch (...) { }
            continue;
        }

        if (active_chat == 0) {
            ui.printSystem("Выберите контакт: /c номер");
            continue;
        }

        if (user_input.length() > 5 && user_input.substr(0, 5) == "/img ") {
            std::string filepath = user_input.substr(5);
            ui.printSystem("Отправка картинки...");
            if (api.sendImage(active_chat, filepath)) ui.printSystem("УСПЕХ: Картинка отправлена!");
            else ui.printSystem("ОШИБКА: Не удалось отправить картинку.");
        } else {
            ui.printMessage("Вы", user_input);
            api.sendMessage(active_chat, user_input);
        }
    }

    listener_thread.detach();
}