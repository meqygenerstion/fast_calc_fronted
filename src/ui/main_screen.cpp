#include "main_screen.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

namespace
{
    bool ParseHexComponent(const std::string &component, int &out)
    {
        try
        {
            out = std::stoi(component, nullptr, 16);
            return out >= 0 && out <= 255;
        }
        catch (...)
        {
            return false;
        }
    }
} // namespace

MainScreen::MainScreen(function<double(const string &)> evaluator,
                       ConfigManager &config,
                       LocalizationManager &localization)
    : eval_fn(std::move(evaluator)),
      config_(config),
      localization_(localization)
{
    config_.load();
    init_from_config();
    if (config_dirty_)
    {
        config_.save();
        config_dirty_ = false;
    }
}

void MainScreen::Run()
{
    vector<string> tabs = {
        localization_.get_text("main.tabs.calculator", "Calculator"),
        localization_.get_text("main.tabs.help", "Help"),
        localization_.get_text("main.tabs.about", "About"),
    };
    Component tab_toggle = Toggle(&tabs, &current_tab);
    HistoryScreen history(calc, &localization_);
    Component input_box = Input(&input, localization_.get_text("main.input_placeholder", "Enter expression..."));

    input_box |= CatchEvent([&](Event e)
                            {
        if (e == Event::Return || (e.is_character() && e.character() == "=")) {
            if (!input.empty()) {
                try {
                    double res = eval_fn(input);
                    calc.add_result(input, res);
                } catch (...) {
                    calc.add_result(input, nan(""));
                }
                input.clear();
                history.handle_event(1);
                current_tab = 0; // Возвращаемся в калькулятор
            }
            return true;
        }
        return false; });

    std::vector<std::string> help_text;
    for (int i = 1; i <= 90; ++i)
        help_text.push_back("Line " + std::to_string(i));

    TextScreen all_story(help_text);
    all_story.set_default_string(localization_.get_text("text_screen.empty", "No lines"));
    TextScreen help(help_text);
    help.set_default_string(localization_.get_text("text_screen.empty", "No lines"));

    Component container = Container::Vertical({tab_toggle,
                                               input_box});

    int blatmodule = 0;
    container |= CatchEvent([&](Event e)
                            {
        if (e == Event::CtrlQ) {
            screen.Exit();
            return true;
        }


        if (e == Event::ArrowUpCtrl){
            blatmodule = 0;
            switch (current_tab) {
                case 0: {
                    history.handle_event(blatmodule);
                    break;
                }
                case 1: {
                    help.handle_event(blatmodule);
                    break;
                }
                case 2:{
                    all_story.handle_event(blatmodule);
                    break;
                }
                default:{

                    break;
                }
            }

            return true;
        }

        if (e == Event::ArrowDownCtrl){

            blatmodule = 1;

            switch (current_tab) {
                case 0: {

                    history.handle_event(blatmodule);
                    break;
                }
                case 1: {

                    help.handle_event(blatmodule);
                    break;
                }
                case 2:{

                    all_story.handle_event(blatmodule);
                    break;
                }
                default:{

                    break;
                }
            }


            return true;
        }

        if (e == Event::ArrowLeftCtrl && current_tab == 0){

            history.handle_event(2);

            return true;
        }

        if (e == Event::ArrowRightCtrl && current_tab == 0){

            history.handle_event(3);

            return true;
        }

        if (e == Event::CtrlR && current_tab == 0){

            history.handle_event(4);

            return true;
        }

        return false; });

    Component tabs_content = Container::Tab({history.get_component(),
                                             help.get_component(),
                                             all_story.get_component()},
                                            &current_tab);

    Component renderer = Renderer(container, [&]
                                  {
        auto title_element = text(localization_.get_text("main.title", "FTXUI Calculator")) | bold | center;
        if (has_title_color_) {
            title_element = title_element | color(title_color_);
        }

        auto tabs_element = tab_toggle->Render() | center;
        if (has_accent_color_) {
            tabs_element = tabs_element | color(accent_color_);
        }

        auto tab_content_element = tabs_content->Render() | flex;
        auto input_row = hbox({
            text(localization_.get_text("history.entry_prefix", "> ")),
            input_box->Render()
        });
        if (has_accent_color_) {
            input_row = input_row | color(accent_color_);
        }

        return vbox({
            std::move(title_element),
            separator(),
            std::move(tabs_element),
            separator(),
            std::move(tab_content_element),
            separator(),
            std::move(input_row) | border
        }) | flex; });

    screen.Loop(renderer);

    config_.save();
}

void MainScreen::init_from_config()
{
    std::string title_color_name = config_.get_color("title");
    if (title_color_name.empty())
    {
        title_color_name = "yellow";
        config_.set_color("title", title_color_name);
        config_dirty_ = true;
    }
    auto [title_color_value, title_valid] = parse_color(title_color_name);
    has_title_color_ = title_valid;
    title_color_ = title_color_value;

    std::string accent_color_name = config_.get_color("accent");
    if (accent_color_name.empty())
    {
        accent_color_name = "cyan";
        config_.set_color("accent", accent_color_name);
        config_dirty_ = true;
    }
    auto [accent_color_value, accent_valid] = parse_color(accent_color_name);
    has_accent_color_ = accent_valid;
    accent_color_ = accent_color_value;
}

std::pair<Color, bool> MainScreen::parse_color(const string &lower) const
{
    if (lower.empty())
    {
        return {Color::Default, false};
    }

    if (lower == "default")
    {
        return {Color::Default, false};
    }

    static const std::unordered_map<std::string, Color> kNamedColors = {
        {"black", Color::Black},
        {"red", Color::Red},
        {"green", Color::Green},
        {"yellow", Color::Yellow},
        {"blue", Color::Blue},
        {"magenta", Color::Magenta},
        {"cyan", Color::Cyan},
        {"white", Color::White},
        {"gray", Color::GrayLight},
        {"grey", Color::GrayLight},
    };

    if (auto it = kNamedColors.find(lower); it != kNamedColors.end())
    {
        return {it->second, true};
    }

    if (lower.size() == 7 && lower[0] == '#')
    {
        int r = 0;
        int g = 0;
        int b = 0;
        if (ParseHexComponent(lower.substr(1, 2), r) &&
            ParseHexComponent(lower.substr(3, 2), g) &&
            ParseHexComponent(lower.substr(5, 2), b))
        {
            return {Color::RGB(r, g, b), true};
        }
    }

    return {Color::Default, false};
}
