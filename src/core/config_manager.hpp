#pragma once
#include "file_manager.hpp"
#include "../common.hpp"

class ConfigManager : public FileManager
{
public:
    explicit ConfigManager(const string &config_path);

    void load() override;
    void save() override;

    // Работа с цветами элементов
    void set_color(const string &element, const string &color);
    string get_color(const string &element) const;

    // Работа с горячими клавишами
    void set_key(const string &action, const string &key);
    string get_key(const string &action) const;

private:
    string config_file_path_;
    toml::table config_data_;
};
