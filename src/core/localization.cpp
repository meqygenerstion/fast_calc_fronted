#include "localization.hpp"

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <locale>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winnls.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace std::string_literals;

namespace
{

    using StringMap = std::unordered_map<std::string, std::string>;

    std::string Lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string NormalizeLocaleCode(std::string code)
    {
        if (code.empty())
        {
            return code;
        }

        auto first = code.find_first_not_of(" \t\n\r");
        auto last = code.find_last_not_of(" \t\n\r");
        if (first == std::string::npos)
        {
            return {};
        }
        code = code.substr(first, last - first + 1);

        auto dot_pos = code.find('.');
        if (dot_pos != std::string::npos)
        {
            code = code.substr(0, dot_pos);
        }

        auto at_pos = code.find('@');
        if (at_pos != std::string::npos)
        {
            code = code.substr(0, at_pos);
        }

        std::replace(code.begin(), code.end(), '-', '_');
        return Lowercase(code);
    }

    std::string ExtractLanguageCode(const std::string &locale)
    {
        auto sep = locale.find('_');
        if (sep == std::string::npos)
        {
            sep = locale.find('-');
        }
        if (sep == std::string::npos)
        {
            return locale;
        }
        return locale.substr(0, sep);
    }

    std::string NormalizeIfValid(const std::string &code)
    {
        const std::string normalized = NormalizeLocaleCode(code);
        if (normalized.empty() || normalized == "c" || normalized == "posix")
        {
            return {};
        }
        return normalized;
    }

    std::string DetectLocaleFromEnv()
    {
        static constexpr const char *kEnvVars[] = {"LANG", "LANGUAGE"};
        for (const char *var : kEnvVars)
        {
            if (const char *value = std::getenv(var))
            {
                if (*value)
                {
                    const std::string normalized = NormalizeIfValid(value);
                    if (!normalized.empty())
                    {
                        return normalized;
                    }
                }
            }
        }
        return {};
    }

#if defined(_WIN32)

    std::string DetectLocaleWindows()
    {
#ifdef LOCALE_SNAME
        {
            LCID lcid = GetUserDefaultLCID();
            if (lcid == 0)
            {
                lcid = GetThreadLocale();
            }
            char buffer[128] = {0};
            if (GetLocaleInfoA(lcid, LOCALE_SNAME, buffer, static_cast<int>(sizeof(buffer))) > 0)
            {
                const std::string normalized = NormalizeIfValid(buffer);
                if (!normalized.empty())
                {
                    return normalized;
                }
            }
        }
#endif

        {
            LCID lcid = GetUserDefaultLCID();
            if (lcid == 0)
            {
                lcid = GetThreadLocale();
            }
            char language[16] = {0};
            if (GetLocaleInfoA(lcid, LOCALE_SISO639LANGNAME, language, static_cast<int>(sizeof(language))) > 0)
            {
                char country[16] = {0};
                if (GetLocaleInfoA(lcid, LOCALE_SISO3166CTRYNAME, country,
                                   static_cast<int>(sizeof(country))) > 0)
                {
                    const std::string normalized = NormalizeIfValid(std::string(language) + "_" + country);
                    if (!normalized.empty())
                    {
                        return normalized;
                    }
                }

                const std::string normalized = NormalizeIfValid(language);
                if (!normalized.empty())
                {
                    return normalized;
                }
            }
        }

        {
            LANGID langid = GetUserDefaultUILanguage();
            if (langid != 0)
            {
                char language[16] = {0};
                if (GetLocaleInfoA(MAKELCID(langid, SORT_DEFAULT), LOCALE_SISO639LANGNAME, language,
                                   static_cast<int>(sizeof(language))) > 0)
                {
                    const std::string normalized = NormalizeIfValid(language);
                    if (!normalized.empty())
                    {
                        return normalized;
                    }
                }
            }
        }

        return {};
    }

#else // !_WIN32

#if defined(__APPLE__)
    std::string CFStringToStdString(CFStringRef value)
    {
        if (!value)
        {
            return {};
        }
        CFIndex length = CFStringGetLength(value);
        CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
        if (max_size <= 0)
        {
            return {};
        }
        std::string buffer(static_cast<std::size_t>(max_size) + 1, '\0');
        if (CFStringGetCString(value, buffer.data(), buffer.size(), kCFStringEncodingUTF8))
        {
            buffer.resize(std::strlen(buffer.c_str()));
            return buffer;
        }
        return {};
    }

    std::string DetectLocaleApple()
    {
        if (CFArrayRef languages = CFLocaleCopyPreferredLanguages())
        {
            if (CFArrayGetCount(languages) > 0)
            {
                CFStringRef cf_lang = static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, 0));
                const std::string normalized = NormalizeIfValid(CFStringToStdString(cf_lang));
                CFRelease(languages);
                if (!normalized.empty())
                {
                    return normalized;
                }
            }
            else
            {
                CFRelease(languages);
            }
        }

        if (CFLocaleRef locale = CFLocaleCopyCurrent())
        {
            CFStringRef identifier = CFLocaleGetIdentifier(locale);
            const std::string normalized = NormalizeIfValid(CFStringToStdString(identifier));
            CFRelease(locale);
            if (!normalized.empty())
            {
                return normalized;
            }
        }

        if (CFStringRef value = static_cast<CFStringRef>(
                CFPreferencesCopyValue(CFSTR("AppleLocale"), kCFPreferencesAnyApplication,
                                       kCFPreferencesCurrentUser, kCFPreferencesAnyHost)))
        {
            const std::string normalized = NormalizeIfValid(CFStringToStdString(value));
            CFRelease(value);
            if (!normalized.empty())
            {
                return normalized;
            }
        }

        return {};
    }
#endif // __APPLE__

    std::string DetectLocalePosix()
    {
#if defined(LC_MESSAGES)
        constexpr int category = LC_MESSAGES;
#else
        constexpr int category = LC_ALL;
#endif

        std::string previous;
        if (const char *current = std::setlocale(category, nullptr))
        {
            previous = current;
            const std::string normalized = NormalizeIfValid(current);
            if (!normalized.empty())
            {
                return normalized;
            }
        }

        const char *activated = std::setlocale(category, "");
        std::string normalized;
        if (activated)
        {
            normalized = NormalizeIfValid(activated);
        }

        if (!previous.empty())
        {
            std::setlocale(category, previous.c_str());
        }
        else
        {
            std::setlocale(category, "C");
        }

        return normalized;
    }

#endif // !_WIN32

    std::string DetectSystemLocale()
    {
        std::string detected;
#if defined(_WIN32)
        detected = DetectLocaleWindows();
#elif defined(__APPLE__)
        detected = DetectLocaleApple();
        if (detected.empty())
        {
            detected = DetectLocalePosix();
        }
#else
        detected = DetectLocaleFromEnv();
#endif

        if (!detected.empty())
        {
            return detected;
        }

        try
        {
            std::locale system_locale("");
            const std::string name = NormalizeIfValid(system_locale.name());
            if (!name.empty())
            {
                return name;
            }
        }
        catch (...)
        {
        }

        return {};
    }

    std::string ChooseMatchingLocale(const std::vector<std::string> &available,
                                     const std::string &requested)
    {
        if (requested.empty() || available.empty())
        {
            return {};
        }

        const std::string normalized = requested;
        for (const auto &candidate : available)
        {
            if (NormalizeLocaleCode(candidate) == normalized)
            {
                return candidate;
            }
        }

        const std::string language_only = ExtractLanguageCode(normalized);
        if (!language_only.empty())
        {
            for (const auto &candidate : available)
            {
                if (NormalizeLocaleCode(candidate) == language_only)
                {
                    return candidate;
                }
            }
        }

        return {};
    }

    void FlattenNode(const toml::node &node, const std::string &prefix, StringMap &out);

    void FlattenArray(const toml::array &array, const std::string &prefix, StringMap &out)
    {
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            const toml::node *element = array.get(i);
            if (!element)
            {
                continue;
            }
            std::string key = prefix.empty() ? std::to_string(i) : prefix + "." + std::to_string(i);
            FlattenNode(*element, key, out);
        }
    }

    void FlattenNode(const toml::node &node, const std::string &prefix, StringMap &out)
    {
        if (const auto *table = node.as_table())
        {
            for (const auto &[child_key, child_node] : *table)
            {
                const std::string child_key_str = std::string(child_key.str());
                const std::string key = prefix.empty()
                                            ? child_key_str
                                            : prefix + "." + child_key_str;
                FlattenNode(child_node, key, out);
            }
            return;
        }

        if (const auto *array = node.as_array())
        {
            FlattenArray(*array, prefix, out);
            return;
        }

        if (const auto string_value = node.value<std::string>())
        {
            out[prefix] = *string_value;
        }
        else if (const auto integer_value = node.value<int64_t>())
        {
            out[prefix] = std::to_string(*integer_value);
        }
        else if (const auto float_value = node.value<double>())
        {
            out[prefix] = std::to_string(*float_value);
        }
        else if (const auto bool_value = node.value<bool>())
        {
            out[prefix] = *bool_value ? "true"s : "false"s;
        }
    }

    bool ParseTomlFile(const std::filesystem::path &path, StringMap &out)
    {
        try
        {
            const toml::table table = toml::parse_file(path.string());
            FlattenNode(table, "", out);
            return true;
        }
        catch (const toml::parse_error &)
        {
            return false;
        }
    }

    StringMap ParseLocalePath(const std::filesystem::path &locale_path)
    {
        StringMap collected;
        if (!std::filesystem::exists(locale_path))
        {
            return collected;
        }

        if (std::filesystem::is_directory(locale_path))
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(locale_path))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const auto extension = Lowercase(entry.path().extension().string());
                if (extension == ".toml")
                {
                    ParseTomlFile(entry.path(), collected);
                }
            }
        }
        else if (std::filesystem::is_regular_file(locale_path))
        {
            ParseTomlFile(locale_path, collected);
        }

        return collected;
    }

    void InsertFlatKey(toml::table &table, const std::string &flat_key, const std::string &value)
    {
        std::stringstream stream(flat_key);
        std::string segment;
        toml::table *current = &table;

        while (std::getline(stream, segment, '.'))
        {
            if (stream.peek() == EOF)
            {
                current->insert_or_assign(segment, value);
                break;
            }

            toml::table *next = current->get_as<toml::table>(segment);
            if (!next)
            {
                auto [it, _] = current->insert_or_assign(segment, toml::table{});
                next = it->second.as_table();
            }
            current = next;
        }
    }

} // namespace

LocalizationManager::LocalizationManager(const string &locale_dir)
    : locale_directory_(to_path(locale_dir).string())
{
    create_dir(locale_directory_);
    detect_system_locale();
}

void LocalizationManager::load()
{
    const auto locales = available_locales();

    if (current_locale_.empty())
    {
        detect_system_locale(locales);
    }

    if (!current_locale_.empty())
    {
        if (load_locale(current_locale_))
        {
            return;
        }
    }

    for (const auto &locale : locales)
    {
        if (locale == current_locale_)
        {
            continue;
        }
        if (load_locale(locale))
        {
            return;
        }
    }

    texts_.clear();
}

void LocalizationManager::save()
{
}

bool LocalizationManager::load_locale(const string &locale_code)
{
    const auto normalized_code = locale_code;

    std::filesystem::path locale_path = std::filesystem::path(locale_directory_) / normalized_code;
    if (!std::filesystem::exists(locale_path))
    {
        locale_path = std::filesystem::path(locale_directory_) / (normalized_code + ".toml");
        if (!std::filesystem::exists(locale_path))
        {
            return false;
        }
    }

    StringMap collected = ParseLocalePath(locale_path);
    if (collected.empty())
    {
        return false;
    }

    current_locale_ = normalized_code;
    texts_ = std::move(collected);
    return true;
}

string LocalizationManager::get_text(const string &key, const string &default_text) const
{
    const auto it = texts_.find(key);
    if (it != texts_.end())
    {
        return it->second;
    }
    return default_text;
}

vector<string> LocalizationManager::available_locales() const
{
    vector<string> locales;
    const std::filesystem::path dir(locale_directory_);
    if (!std::filesystem::exists(dir))
    {
        return locales;
    }

    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
        {
            break;
        }

        if (entry.is_directory())
        {
            locales.emplace_back(entry.path().filename().string());
        }
        else if (entry.is_regular_file())
        {
            const auto extension = Lowercase(entry.path().extension().string());
            if (extension == ".toml")
            {
                locales.emplace_back(entry.path().stem().string());
            }
        }
    }

    std::sort(locales.begin(), locales.end());
    locales.erase(std::unique(locales.begin(), locales.end()), locales.end());
    return locales;
}

bool LocalizationManager::detect_system_locale()
{
    const auto locales = available_locales();
    return detect_system_locale(locales);
}

bool LocalizationManager::detect_system_locale(const std::vector<std::string> &locales)
{
    if (locales.empty())
    {
        return false;
    }

    const std::string system_locale = DetectSystemLocale();

    std::cout << system_locale;

    if (system_locale.empty())
    {
        return false;
    }

    const std::string matched = ChooseMatchingLocale(locales, system_locale);
    if (matched.empty())
    {
        return false;
    }

    current_locale_ = matched;
    return true;
}
