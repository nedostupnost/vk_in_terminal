#include "ui_console.hpp"
#include "logger.hpp"
#include <clocale>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <cstring>
#include <iostream>

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

UIConsole::UIConsole() {
    setlocale(LC_ALL, ""); 
    LOG("UIConsole: Запуск конструктора...");
    
    LOG("UIConsole: Вызов notcurses_init(nullptr, stdout)...");
    nc = notcurses_init(nullptr, stdout);
    
    if (!nc) {
        LOG("UIConsole: КРИТИЧЕСКАЯ ОШИБКА - Не удалось инициализировать Notcurses!");
        std::cerr << "\n[КРИТИЧЕСКАЯ ОШИБКА] Не удалось инициализировать Notcurses!\n";
        exit(1);
    }
    LOG("UIConsole: notcurses_init успешно отработал!");
    
    std_plane = notcurses_stdplane(nc);
    if (!std_plane) {
        LOG("UIConsole: ОШИБКА - std_plane == nullptr");
        exit(1);
    }

    ncplane_dim_yx(std_plane, &term_height, &term_width);
    LOG("UIConsole: Размеры терминала: " + std::to_string(term_width) + "x" + std::to_string(term_height));

    if (term_height < 15 || term_width < 45) {
        LOG("UIConsole: ОШИБКА - Терминал слишком мал.");
        notcurses_stop(nc);
        std::cerr << "\n[ОШИБКА] Ваш терминал слишком мал! (" << term_width << "x" << term_height << ")\n"
                  << "Растяните окно минимум до 45x15 и запустите снова.\n";
        exit(1);
    }

    unsigned int h = term_height - 4;
    unsigned int cw = term_width - 30;

    LOG("UIConsole: Создание базовых плоскостей...");
    ncplane_options chat_bdr_opts = {0};
    chat_bdr_opts.y = 1; chat_bdr_opts.x = 0; chat_bdr_opts.rows = h; chat_bdr_opts.cols = cw;
    chat_bdr = ncplane_create(std_plane, &chat_bdr_opts);

    ncplane_options contacts_bdr_opts = {0};
    contacts_bdr_opts.y = 1; contacts_bdr_opts.x = cw; contacts_bdr_opts.rows = h; contacts_bdr_opts.cols = 30;
    contacts_bdr = ncplane_create(std_plane, &contacts_bdr_opts);

    ncplane_options input_bdr_opts = {0};
    input_bdr_opts.y = term_height - 3; input_bdr_opts.x = 0; input_bdr_opts.rows = 3; input_bdr_opts.cols = term_width;
    input_bdr = ncplane_create(std_plane, &input_bdr_opts);

    if (chat_bdr) {
        ncplane_options chat_opts = {0};
        chat_opts.y = 1; chat_opts.x = 1; chat_opts.rows = h - 2; chat_opts.cols = cw - 2;
        chat_plane = ncplane_create(chat_bdr, &chat_opts);
        if (chat_plane) ncplane_set_scrolling(chat_plane, true); 
    }

    if (contacts_bdr) {
        ncplane_options contacts_opts = {0};
        contacts_opts.y = 1; contacts_opts.x = 1; contacts_opts.rows = h - 2; contacts_opts.cols = 28;
        contacts_plane = ncplane_create(contacts_bdr, &contacts_opts);
    }

    if (input_bdr) {
        ncplane_options input_opts = {0};
        input_opts.y = 1; input_opts.x = 1; input_opts.rows = 1; input_opts.cols = term_width - 2;
        input_plane = ncplane_create(input_bdr, &input_opts);
    }

    ncplane_options status_opts = {0};
    status_opts.y = term_height - 4; status_opts.x = 0; status_opts.rows = 1; status_opts.cols = term_width;
    status_plane = ncplane_create(std_plane, &status_opts);

    LOG("UIConsole: Переход к drawBorders()...");
    drawBorders();
    LOG("UIConsole: Инициализация полностью и успешно завершена.");
}

UIConsole::~UIConsole() {
    LOG("UIConsole: Вызов деструктора...");
    if (nc) notcurses_stop(nc); 
}

void UIConsole::drawContactsText() {
    if (!contacts_plane) return;
    ncplane_erase(contacts_plane);
    int y = 0;
    for (size_t i = 0; i < cached_contacts.size(); ++i) {
        if (cached_contacts[i].second) {
            ncplane_set_fg_rgb8(contacts_plane, 0, 255, 0); 
            ncplane_printf_yx(contacts_plane, y++, 0, "[%zu] * %s", i + 1, cached_contacts[i].first.c_str());
        } else {
            ncplane_set_fg_rgb8(contacts_plane, 200, 200, 200); 
            ncplane_printf_yx(contacts_plane, y++, 0, "[%zu]   %s", i + 1, cached_contacts[i].first.c_str());
        }
    }
}

void UIConsole::drawBorders() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (!nc || !std_plane) return; 

    LOG("drawBorders: Отрисовка шапки...");
    ncplane_set_bg_rgb8(std_plane, 0, 0, 150);
    ncplane_set_fg_rgb8(std_plane, 255, 255, 255);
    ncplane_printf_yx(std_plane, 0, 2, " 🌻 VK Console Messenger (Notcurses Engine) ");

    LOG("drawBorders: Отрисовка рамки чата...");
    if (chat_bdr) {
        ncplane_set_fg_rgb8(chat_bdr, 255, 255, 255);
        ncplane_perimeter_rounded(chat_bdr, 0, 0, 0); // Идеально безопасная функция
        ncplane_printf_yx(chat_bdr, 0, 2, " Чат ");
    }
    
    LOG("drawBorders: Отрисовка рамки контактов...");
    if (contacts_bdr) {
        ncplane_set_fg_rgb8(contacts_bdr, 255, 255, 255);
        ncplane_perimeter_rounded(contacts_bdr, 0, 0, 0);
        ncplane_printf_yx(contacts_bdr, 0, 2, " Контакты ");
    }
    
    LOG("drawBorders: Отрисовка рамки ввода...");
    if (input_bdr) {
        ncplane_set_fg_rgb8(input_bdr, 255, 255, 255);
        ncplane_perimeter_rounded(input_bdr, 0, 0, 0);
        ncplane_printf_yx(input_bdr, 0, 2, " Send Message ");
    }

    LOG("drawBorders: drawContactsText()...");
    drawContactsText();
    
    LOG("drawBorders: вызов notcurses_render()...");
    notcurses_render(nc);
    LOG("drawBorders: Рендер прошел успешно!");
}

void UIConsole::updateContacts(const std::vector<std::pair<std::string, bool>>& contacts) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    cached_contacts = contacts; 
    drawContactsText();
    if (nc) notcurses_render(nc);
}

void UIConsole::clearChat() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (chat_plane) ncplane_erase(chat_plane);
    if (nc) notcurses_render(nc);
}

void UIConsole::printMessage(const std::string& sender, const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (!chat_plane || !nc) return;

    if (sender == "Вы") ncplane_set_fg_rgb8(chat_plane, 0, 255, 0);
    else ncplane_set_fg_rgb8(chat_plane, 0, 255, 255);

    ncplane_printf(chat_plane, "[%s] %s: ", getCurrentTime().c_str(), sender.c_str());
    
    ncplane_set_fg_rgb8(chat_plane, 200, 200, 200);
    ncplane_printf(chat_plane, "%s\n", text.c_str());
    
    notcurses_render(nc);
}

void UIConsole::printMedia(const std::string& sender, const std::string& filepath) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (!chat_plane || !nc) return;
    
    ncplane_set_fg_rgb8(chat_plane, 0, 255, 255);
    ncplane_printf(chat_plane, "[%s] %s прислал медиа:\n", getCurrentTime().c_str(), sender.c_str());
    
    struct ncvisual* ncv = ncvisual_from_file(filepath.c_str());
    if (ncv) {
        struct ncvisual_options vopts = {0};
        vopts.n = chat_plane;
        vopts.scaling = NCSCALE_SCALE;
        vopts.blitter = NCBLIT_PIXEL; 
        vopts.flags = NCVISUAL_OPTION_NODEGRADE;
        
        if (ncvisual_blit(nc, ncv, &vopts) == nullptr) {
            vopts.blitter = NCBLIT_2x1; 
            vopts.flags = 0;
            ncvisual_blit(nc, ncv, &vopts);
        }
        ncvisual_destroy(ncv);
        ncplane_printf(chat_plane, "\n"); 
    } else {
        ncplane_set_fg_rgb8(chat_plane, 255, 0, 0);
        ncplane_printf(chat_plane, "[ОШИБКА: Не удалось прочитать файл %s]\n", filepath.c_str());
    }
    
    notcurses_render(nc);
}

void UIConsole::clearSystem() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (!status_plane || !nc) return;
    ncplane_erase(status_plane);
    notcurses_render(nc);
}

void UIConsole::showTyping(const std::string& sender) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (!status_plane || !nc) return;
    ncplane_erase(status_plane);
    ncplane_set_fg_rgb8(status_plane, 0, 255, 0); 
    ncplane_printf_yx(status_plane, 0, 2, "✎ %s набирает сообщение...", sender.c_str());
    notcurses_render(nc);
}

void UIConsole::printSystem(const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    if (!status_plane || !nc) return;
    
    LOG("СИСТЕМА: " + text);

    ncplane_erase(status_plane);
    ncplane_set_fg_rgb8(status_plane, 255, 255, 0); 
    ncplane_printf_yx(status_plane, 0, 2, ">>> %s <<<", text.c_str());
    notcurses_render(nc);
}

std::string UIConsole::getUserInput() {
    std::string input;
    ncinput ni;

    while (true) {
        {
            std::lock_guard<std::recursive_mutex> lock(ui_mutex);
            if (!input_plane || !nc) break;
            ncplane_erase(input_plane);
            ncplane_set_fg_rgb8(input_plane, 255, 255, 255);
            ncplane_printf_yx(input_plane, 0, 0, "%s", input.c_str());
            notcurses_render(nc);
        }

        uint32_t id = notcurses_get(nc, nullptr, &ni);
        if (id == (uint32_t)-1) continue;

        if (id == NCKEY_ENTER || id == '\n' || id == '\r') {
            break;
        } else if (id == NCKEY_BACKSPACE || id == 127 || id == '\b') {
            if (!input.empty()) {
                while (!input.empty() && (input.back() & 0xC0) == 0x80) input.pop_back();
                if (!input.empty()) input.pop_back();
            }
        } 
        else if (id >= 32 && id < NCKEY_INVALID) {
            if (ni.utf8[0] != '\0') input += ni.utf8;
            else if (id < 128) input.push_back((char)id);
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(ui_mutex);
        if (input_plane && nc) {
            ncplane_erase(input_plane);
            notcurses_render(nc);
        }
    }
    return input;
}