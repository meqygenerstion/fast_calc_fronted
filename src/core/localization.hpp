#pragma once
#include "file_manager.hpp"
#include "../common.hpp"

class LocalizationManager : public FileManager
{
public:
    explicit LocalizationManager(const string &locale_dir);

    void load() override; // Загружает текущую локаль
    void save() override; // Можно сохранять кастомные тексты, если нужно

    bool load_locale(const string &locale_code);
    string get_text(const string &key, const string &default_text = "") const;
    vector<string> available_locales() const;
    bool detect_system_locale();
    const string &current_locale() const { return current_locale_; }

private:
    bool detect_system_locale(const vector<string> &locales);

    string locale_directory_;
    string current_locale_;
    unordered_map<string, string> texts_;
};
