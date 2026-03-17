#include "ui_console.hpp"
#include <locale.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

UIConsole::UIConsole() {
    setlocale(LC_ALL, ""); 
    log_file.open("system_log.txt", std::ios::app);

    initscr();  
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, -1);
        init_pair(2, COLOR_CYAN, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_WHITE, COLOR_BLUE);
    }

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int h = max_y - 4; 
    int cw = max_x - 30; 

    chat_view_h = h - 2;
    chat_view_w = cw - 4; 
    scroll_offset = 0;
    current_pad_y = 0;

    chat_bdr = newwin(h, cw, 1, 0);
    chat_pad = newpad(2000, chat_view_w); 
    scrollok(chat_pad, TRUE);

    contacts_bdr = newwin(h, 30, 1, cw);
    contacts_win = newwin(h - 2, 28, 2, cw + 1);

    status_win = newwin(1, max_x, max_y - 4, 0);
    input_win = newwin(3, max_x, max_y - 3, 0);

    keypad(input_win, TRUE); 

    drawBorders();
}

UIConsole::~UIConsole() {
    if (log_file.is_open()) {
        log_file.close();
    }
    delwin(chat_pad); delwin(chat_bdr);
    delwin(contacts_win); delwin(contacts_bdr);
    delwin(status_win); delwin(input_win);
    endwin();
}

std::string UIConsole::sanitizeText(const std::string& text) {
    std::string res;
    for (size_t i = 0; i < text.length(); ) {
        unsigned char c = text[i];
        if ((c & 0x80) == 0) { res += text[i++]; }
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < text.length()) { res += text[i++]; res += text[i++]; } else break;
        }
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < text.length()) { res += text[i++]; res += text[i++]; res += text[i++]; } else break;
        }
        else if ((c & 0xF8) == 0xF0) {
            res += "[эмодзи]"; 
            i += 4;
        } else { i++; }
    }
    return res;
}

void UIConsole::refreshChat() {
    prefresh(chat_pad, scroll_offset, 0, 2, 2, 2 + chat_view_h - 1, 2 + chat_view_w - 1);
}

void UIConsole::drawContactsText() {
    werase(contacts_win);
    for (size_t i = 0; i < cached_contacts.size(); ++i) {
        if (cached_contacts[i].second) { 
            wattron(contacts_win, COLOR_PAIR(1) | A_BOLD); 
            wprintw(contacts_win, "[%zu] * %s\n", i + 1, cached_contacts[i].first.c_str());
            wattroff(contacts_win, COLOR_PAIR(1) | A_BOLD);
        } else {
            wprintw(contacts_win, "[%zu]   %s\n", i + 1, cached_contacts[i].first.c_str());
        }
    }
    wrefresh(contacts_win);
}

void UIConsole::drawBorders() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    int max_x = getmaxx(stdscr);

    attron(COLOR_PAIR(4) | A_BOLD);
    mvhline(0, 0, ' ', max_x);
    mvprintw(0, 2, " 🌻 VK Console ");
    attroff(COLOR_PAIR(4) | A_BOLD);

    box(chat_bdr, 0, 0);
    mvwprintw(chat_bdr, 0, 2, " Чат ");
    
    box(contacts_bdr, 0, 0);
    mvwprintw(contacts_bdr, 0, 2, " Контакты ");
    
    box(input_win, 0, 0);
    wattron(input_win, COLOR_PAIR(4));
    mvwprintw(input_win, 0, 2, " Send Message ");
    wattroff(input_win, COLOR_PAIR(4));

    refresh();
    wrefresh(chat_bdr); 
    wrefresh(contacts_bdr); 
    wrefresh(input_win);

    drawContactsText(); 
    refreshChat();
}

void UIConsole::updateContacts(const std::vector<std::pair<std::string, bool>>& contacts) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    cached_contacts = contacts; 
    drawContactsText();
}

void UIConsole::clearChat() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    werase(chat_pad);
    wmove(chat_pad, 0, 0);
    current_pad_y = 0;
    scroll_offset = 0;
    drawBorders();
}

void UIConsole::scrollChat(int direction) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    scroll_offset += direction;
    
    if (scroll_offset < 0) scroll_offset = 0;
    
    int max_scroll = current_pad_y - chat_view_h;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    
    refreshChat();
    wrefresh(input_win);
}

void UIConsole::printMessage(const std::string& sender, const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    std::string safe_text = sanitizeText(text); 

    int color_pair = (sender == "Вы") ? 1 : 2;

    wprintw(chat_pad, "[%s] ", getCurrentTime().c_str());
    wattron(chat_pad, COLOR_PAIR(color_pair) | A_BOLD);
    wprintw(chat_pad, "%s", sender.c_str());
    wattroff(chat_pad, COLOR_PAIR(color_pair) | A_BOLD);
    wprintw(chat_pad, ": %s\n", safe_text.c_str());
    
    int cur_x;
    getyx(chat_pad, current_pad_y, cur_x);

    int max_scroll = current_pad_y - chat_view_h;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset >= max_scroll - 2) scroll_offset = max_scroll;

    refreshChat();
    wrefresh(input_win);
}

void UIConsole::printSystem(const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    status_msg_id++;
    int current_id = status_msg_id;

    if (log_file.is_open()) {
        log_file << "[" << getCurrentTime() << "] " << text << std::endl;
    }

    werase(status_win);
    wattron(status_win, COLOR_PAIR(3) | A_BLINK | A_BOLD);
    mvwprintw(status_win, 0, 2, ">>> %s <<<", text.c_str());
    wattroff(status_win, COLOR_PAIR(3) | A_BLINK | A_BOLD);
    wrefresh(status_win);
    wrefresh(input_win);

    std::thread([this, current_id]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::lock_guard<std::recursive_mutex> lock(ui_mutex);
        if (status_msg_id == current_id) {
            werase(status_win);
            wrefresh(status_win);
            wrefresh(input_win);
        }
    }).detach();
}

std::string UIConsole::getUserInput() {
    std::string input;
    int ch;
    wtimeout(input_win, 50);

    while (true) {
        {
            std::lock_guard<std::recursive_mutex> lock(ui_mutex);
            werase(input_win);
            box(input_win, 0, 0);
            wattron(input_win, COLOR_PAIR(4));
            mvwprintw(input_win, 0, 2, " Send Message ");
            wattroff(input_win, COLOR_PAIR(4));
            mvwprintw(input_win, 1, 2, "%s", input.c_str());
            wrefresh(input_win);
        }

        ch = wgetch(input_win);
        if (ch == ERR) continue;

        if (ch == KEY_UP) { scrollChat(-1); }
        else if (ch == KEY_DOWN) { scrollChat(1); }
        else if (ch == KEY_PPAGE) { scrollChat(-chat_view_h / 2); } 
        else if (ch == KEY_NPAGE) { scrollChat(chat_view_h / 2); }  
        else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) break;
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (!input.empty()) {
                while (!input.empty() && (input.back() & 0xC0) == 0x80) input.pop_back();
                if (!input.empty()) input.pop_back();
            }
        } 
        else if (ch >= 32 && ch <= 255) input.push_back(ch);
    }

    {
        std::lock_guard<std::recursive_mutex> lock(ui_mutex);
        werase(input_win);
        drawBorders();
    }
    return input;
}