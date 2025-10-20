#pragma once
#include "../common.hpp"
#include "calc_screen.hpp"
#include "text_screen.hpp"
#include "../core/config_manager.hpp"
#include "history_now_screen.hpp"
#include "../core/localization.hpp"
#include <utility>

class MainScreen
{
public:
    MainScreen(function<double(const string &)> evaluator,
               ConfigManager &config,
               LocalizationManager &localization);
    void Run();

private:
    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    Calc calc;
    function<double(const string &)> eval_fn;
    ConfigManager &config_;
    LocalizationManager &localization_;
    void init_from_config();
    std::pair<Color, bool> parse_color(const string &value) const;

    Color title_color_ = Color::Default;
    Color accent_color_ = Color::Default;
    bool has_title_color_ = false;
    bool has_accent_color_ = false;
    bool config_dirty_ = false;

    int current_tab = 0;
    string input;
};
