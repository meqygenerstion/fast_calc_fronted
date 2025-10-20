#pragma once
#include "../common.hpp"

class LocalizationManager
{
public:
    explicit LocalizationManager(const string &locale_dir);

    string get_text(const string &key, const string &default_text = "") const;
};
