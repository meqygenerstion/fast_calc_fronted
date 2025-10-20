#include "localization.hpp"
#include "../common.hpp"
#include <libintl.h>
#include <clocale>

LocalizationManager::LocalizationManager(const string &locale_dir)
{
    bindtextdomain("ui", locale_dir.c_str());
    bind_textdomain_codeset("ui", "UTF-8");
    textdomain("ui");
}

std::string LocalizationManager::get_text(const std::string &key, const std::string &default_text) const
{
    const char *raw = dgettext("ui", key.c_str());
    if (!raw)
    {
        return default_text.empty() ? key : default_text;
    }

    std::string translated(raw);
    if (translated == key)
    {
        return default_text.empty() ? key : default_text;
    }

    return translated;
}
