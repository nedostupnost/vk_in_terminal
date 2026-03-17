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

    chat_bdr = newwin(h, cw, 1, 0);
    chat_win = newwin(h - 2, cw - 2, 2, 1);
    scrollok(chat_win, TRUE);

    contacts_bdr = newwin(h, 30, 1, cw);
    contacts_win = newwin(h - 2, 28, 2, cw + 1);

    status_win = newwin(1, max_x, max_y - 4, 0);
    input_win = newwin(3, max_x, max_y - 3, 0);

    drawBorders();
}

UIConsole::~UIConsole() {
    delwin(chat_win); delwin(chat_bdr);
    delwin(contacts_win); delwin(contacts_bdr);
    delwin(status_win); delwin(input_win);
    endwin();
}

void UIConsole::drawBorders() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    int max_x = getmaxx(stdscr);

    attron(COLOR_PAIR(4) | A_BOLD);
    mvhline(0, 0, ' ', max_x);
    mvprintw(0, 2, " 🌻 VK Console Messenger ");
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
    wrefresh(chat_bdr); wrefresh(chat_win);
    wrefresh(contacts_bdr); wrefresh(contacts_win);
    wrefresh(input_win);
}

void UIConsole::updateContacts(const std::vector<std::string>& contacts) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    werase(contacts_win);
    for (size_t i = 0; i < contacts.size(); ++i) {
        wprintw(contacts_win, "[%zu] %s\n", i + 1, contacts[i].c_str());
    }
    wrefresh(contacts_win);
    wrefresh(input_win);
}

void UIConsole::clearChat() {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    werase(chat_win);
    wrefresh(chat_win);
}

void UIConsole::printMessage(const std::string& sender, const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    int color_pair = (sender == "Вы") ? 1 : 2;

    wprintw(chat_win, "[%s] ", getCurrentTime().c_str());
    wattron(chat_win, COLOR_PAIR(color_pair) | A_BOLD);
    wprintw(chat_win, "%s", sender.c_str());
    wattroff(chat_win, COLOR_PAIR(color_pair) | A_BOLD);
    wprintw(chat_win, ": %s\n", text.c_str());
    
    wrefresh(chat_win);
    wrefresh(input_win);
}

void UIConsole::printSystem(const std::string& text) {
    std::lock_guard<std::recursive_mutex> lock(ui_mutex);
    
    status_msg_id++;
    int current_id = status_msg_id;

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

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) break;
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