#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>
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
    ui.printSystem("Подключение установлено. Загрузка контактов...");

    auto conversations = api.getConversations(15);
    std::vector<std::string> contact_names;
    std::unordered_map<int, std::string> user_names;
    
    for (const auto& conv : conversations) {
        contact_names.push_back(conv.name);
        user_names[conv.peer_id] = conv.name;
    }
    ui.updateContacts(contact_names);

    LongPoll lp(api, group_id);
    std::atomic<int> active_chat{0};

    std::thread listener_thread([&api, &lp, &ui, &active_chat, &user_names]() {
        lp.listen([&api, &ui, &active_chat, &user_names](int peer_id, const std::string& text, const std::string& photo_url) {
            
            if (user_names.find(peer_id) == user_names.end()) {
                user_names[peer_id] = api.getUserName(peer_id);
            }
            
            if (peer_id == active_chat) {
                if (!text.empty()) ui.printMessage(user_names[peer_id], text);
            } else {
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
                    ui.clearChat();
                    ui.printSystem("Загрузка истории чата...");

                    auto history = api.getChatHistory(active_chat, 20);
                    ui.clearChat();
                    for (const auto& msg : history) {
                        std::string s_name = (msg.sender_id < 0) ? "Вы" : user_names[msg.sender_id];
                        ui.printMessage(s_name, msg.text);
                    }
                } else {
                    ui.printSystem("Ошибка: Неверный номер контакта.");
                }
            } catch (...) {
                ui.printSystem("Ошибка: Введите номер контакта (например: /c 1)");
            }
            continue;
        }

        if (active_chat == 0) {
            ui.printSystem("Выберите контакт командой /c номер (например: /c 1)");
            continue;
        }

        ui.printMessage("Вы", user_input);
        api.sendMessage(active_chat, user_input);
    }

    listener_thread.detach();
    return 0;
}