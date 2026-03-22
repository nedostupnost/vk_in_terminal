#include "ui_console.hpp"
#include <clocale>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

std::string getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

UIConsole::UIConsole()
{
    setlocale(LC_ALL, ""); 
    nc.reset(notcurses_init(nullptr, stdout));
    
    if (!nc)
    {
        std::cerr << "\n[КРИТИЧЕСКАЯ ОШИБКА] Не удалось инициализировать Notcurses!\n";
        exit(1);
    }
    
    std_plane = notcurses_stdplane(nc.get());
    ncplane_dim_yx(std_plane, &term_height, &term_width);

    if (term_height < 15 || term_width < 45)
    {
        std::cerr << "\n[ОШИБКА] Терминал слишком мал! (" << term_width << "x" << term_height << ")\n";
        exit(1);
    }

    unsigned int h = term_height - 4;
    unsigned int cw = term_width - 30;

    ncplane_options bdr_opts = {0};
    bdr_opts.y = 1; bdr_opts.x = 0; bdr_opts.rows = h; bdr_opts.cols = cw;
    chat_bdr = ncplane_create(std_plane, &bdr_opts);

    bdr_opts.x = cw; bdr_opts.cols = 30;
    contacts_bdr = ncplane_create(std_plane, &bdr_opts);

    bdr_opts.y = term_height - 3; bdr_opts.x = 0; bdr_opts.rows = 3; bdr_opts.cols = term_width;
    input_bdr = ncplane_create(std_plane, &bdr_opts);

    if (chat_bdr)
    {
        ncplane_options opts = {0};
        opts.y = 1; opts.x = 1; opts.rows = h - 2; opts.cols = cw - 2;
        chat_plane = ncplane_create(chat_bdr, &opts);
        if (chat_plane) ncplane_set_scrolling(chat_plane, true); 
    }

    if (contacts_bdr)
    {
        ncplane_options opts = {0};
        opts.y = 1; opts.x = 1; opts.rows = h - 2; opts.cols = 28;
        contacts_plane = ncplane_create(contacts_bdr, &opts);
    }

    if (input_bdr)
    {
        ncplane_options opts = {0};
        opts.y = 1; opts.x = 1; opts.rows = 1; opts.cols = term_width - 2;
        input_plane = ncplane_create(input_bdr, &opts);
    }

    ncplane_options status_opts = {0};
    status_opts.y = term_height - 4; status_opts.x = 0; status_opts.rows = 1; status_opts.cols = term_width;
    status_plane = ncplane_create(std_plane, &status_opts);

    drawBorders();
}

void UIConsole::drawContactsText()
{
    if (!contacts_plane) return;
    ncplane_erase(contacts_plane);
    int y = 0;
    
    for (size_t i = 0; i < cached_contacts.size(); ++i)
    {
        const auto& [name, has_unread] = cached_contacts[i];
        if (has_unread)
        {
            ncplane_set_fg_rgb8(contacts_plane, 0, 255, 0); 
            ncplane_printf_yx(contacts_plane, y++, 0, "[%zu] * %s", i + 1, name.c_str());
        }
        else
        {
            ncplane_set_fg_rgb8(contacts_plane, 200, 200, 200); 
            ncplane_printf_yx(contacts_plane, y++, 0, "[%zu]   %s", i + 1, name.c_str());
        }
    }
}

void UIConsole::drawBorders()
{
    if (!nc || !std_plane) return; 

    ncplane_set_bg_rgb8(std_plane, 0, 0, 150);
    ncplane_set_fg_rgb8(std_plane, 255, 255, 255);
    ncplane_printf_yx(std_plane, 0, 2, " 🌻 VK Console Messenger ");

    if (chat_bdr)
    {
        ncplane_set_fg_rgb8(chat_bdr, 255, 255, 255);
        ncplane_perimeter_rounded(chat_bdr, 0, 0, 0); 
        ncplane_printf_yx(chat_bdr, 0, 2, " Чат ");
    }
    if (contacts_bdr)
    {
        ncplane_set_fg_rgb8(contacts_bdr, 255, 255, 255);
        ncplane_perimeter_rounded(contacts_bdr, 0, 0, 0);
        ncplane_printf_yx(contacts_bdr, 0, 2, " Контакты ");
    }
    if (input_bdr)
    {
        ncplane_set_fg_rgb8(input_bdr, 255, 255, 255);
        ncplane_perimeter_rounded(input_bdr, 0, 0, 0);
        ncplane_printf_yx(input_bdr, 0, 2, " Send Message ");
    }

    drawContactsText();
    notcurses_render(nc.get());
}

void UIConsole::updateContacts(const std::vector<std::pair<std::string, bool>>& contacts)
{
    cached_contacts = contacts; 
    drawContactsText();
    if (nc) notcurses_render(nc.get());
}

void UIConsole::clearChat()
{
    if (chat_plane)
    {
        ncplane_erase(chat_plane);
        ncplane_cursor_move_yx(chat_plane, 0, 0); 
    }
    if (nc) notcurses_render(nc.get());
}

void UIConsole::printMessage(std::string_view sender, std::string_view text)
{
    if (!chat_plane || !nc) return;

    if (sender == "Вы") ncplane_set_fg_rgb8(chat_plane, 0, 255, 0);
    else ncplane_set_fg_rgb8(chat_plane, 0, 255, 255);

    ncplane_printf(chat_plane, "[%s] %.*s: ", getCurrentTime().c_str(), (int)sender.length(), sender.data());
    
    ncplane_set_fg_rgb8(chat_plane, 200, 200, 200);
    ncplane_printf(chat_plane, "%.*s\n", (int)text.length(), text.data());
    
    notcurses_render(nc.get());
}

void UIConsole::printMedia(std::string_view sender, std::string_view filepath)
{
    if (!chat_plane || !nc) return;
    
    ncplane_set_fg_rgb8(chat_plane, 0, 255, 255);
    ncplane_printf(chat_plane, "[%s] %.*s прислал медиа:\n", getCurrentTime().c_str(), (int)sender.length(), sender.data());
    
    struct ncvisual* ncv = ncvisual_from_file(std::string(filepath).c_str());
    if (ncv)
    {
        struct ncvisual_options vopts = {0};
        vopts.n = chat_plane;
        vopts.scaling = NCSCALE_SCALE;
        vopts.blitter = NCBLIT_PIXEL; 
        vopts.flags = NCVISUAL_OPTION_NODEGRADE;
        
        if (ncvisual_blit(nc.get(), ncv, &vopts) == nullptr)
        {
            vopts.blitter = NCBLIT_2x1; 
            vopts.flags = 0;
            ncvisual_blit(nc.get(), ncv, &vopts);
        }
        ncvisual_destroy(ncv);
        ncplane_printf(chat_plane, "\n"); 
    }
    else
    {
        ncplane_set_fg_rgb8(chat_plane, 255, 0, 0);
        ncplane_printf(chat_plane, "[ОШИБКА: Не удалось прочитать медиафайл]\n");
    }
    
    notcurses_render(nc.get());
}

void UIConsole::clearSystem()
{
    if (!status_plane || !nc) return;
    ncplane_erase(status_plane);
    notcurses_render(nc.get());
}

void UIConsole::showTyping(std::string_view sender)
{
    if (!status_plane || !nc) return;
    ncplane_erase(status_plane);
    ncplane_set_fg_rgb8(status_plane, 0, 255, 0); 
    ncplane_printf_yx(status_plane, 0, 2, "✎ %.*s набирает сообщение...", (int)sender.length(), sender.data());
    notcurses_render(nc.get());
}

void UIConsole::printSystem(std::string_view text)
{
    if (!status_plane || !nc) return;
    ncplane_erase(status_plane);
    ncplane_set_fg_rgb8(status_plane, 255, 255, 0); 
    ncplane_printf_yx(status_plane, 0, 2, ">>> %.*s <<<", (int)text.length(), text.data());
    notcurses_render(nc.get());
}

std::string UIConsole::getUserInput()
{
    std::string input;
    ncinput ni;
    struct timespec ts = {0, 50000000};

    while (true)
    {
        if (!input_plane || !nc) break;
        ncplane_erase(input_plane);
        ncplane_set_fg_rgb8(input_plane, 255, 255, 255);
        ncplane_printf_yx(input_plane, 0, 0, "%s", input.c_str());
        notcurses_render(nc.get());

        uint32_t id = notcurses_get(nc.get(), &ts, &ni);
        
        if (id == (uint32_t)-1 || id == 0) return "";

        if (id == NCKEY_ENTER || id == '\n' || id == '\r')
        {
            break;
        } else if (id == NCKEY_BACKSPACE || id == 127 || id == '\b')
        {
            if (!input.empty())
            {
                while (!input.empty() && (input.back() & 0xC0) == 0x80) input.pop_back();
                if (!input.empty()) input.pop_back();
            }
        } 
        else if (id >= 32 && id < NCKEY_INVALID)
        {
            if (ni.utf8[0] != '\0') input += ni.utf8;
            else if (id < 128) input.push_back((char)id);
        }
    }

    if (input_plane && nc)
    {
        ncplane_erase(input_plane);
        notcurses_render(nc.get());
    }
    
    return input;
}