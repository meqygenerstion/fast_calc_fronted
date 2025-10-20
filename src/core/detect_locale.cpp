#include "detect_locale.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace
{
    char ToLower(char ch)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    char ToUpper(char ch)
    {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
} // namespace

std::string normalize_to_bcp47(std::string s)
{
    const auto erase_at = [&](char marker)
    {
        if (const auto pos = s.find(marker); pos != std::string::npos)
        {
            s.erase(pos);
        }
    };

    erase_at('@');
    erase_at('.');

    if (const auto colon = s.find(':'); colon != std::string::npos)
    {
        s.erase(colon);
    }

    std::replace(s.begin(), s.end(), '_', '-');

    const auto dash = s.find('-');
    if (dash != std::string::npos)
    {
        std::transform(s.begin(), s.begin() + static_cast<std::ptrdiff_t>(dash), s.begin(), ToLower);
        std::transform(s.begin() + static_cast<std::ptrdiff_t>(dash) + 1, s.end(),
                       s.begin() + static_cast<std::ptrdiff_t>(dash) + 1, ToUpper);
    }
    else
    {
        std::transform(s.begin(), s.end(), s.begin(), ToLower);
    }

    return s;
}

std::string bcp47_to_posix_utf8(std::string tag)
{
    std::replace(tag.begin(), tag.end(), '-', '_');
    return tag + ".UTF-8";
}

std::string detect_bcp47_language()
{
#if defined(_WIN32)
    wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {0};
    const int length = GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH);
    if (length > 0)
    {
        const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(utf8_length > 0 ? utf8_length - 1 : 0), '\0');
        if (!utf8.empty())
        {
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8.data(), utf8_length, nullptr, nullptr);
            return normalize_to_bcp47(utf8);
        }
    }
#elif defined(__APPLE__)
    if (CFArrayRef langs = CFLocaleCopyPreferredLanguages())
    {
        if (CFArrayGetCount(langs) > 0)
        {
            const auto first = static_cast<CFStringRef>(CFArrayGetValueAtIndex(langs, 0));
            char buf[256];
            if (CFStringGetCString(first, buf, sizeof(buf), kCFStringEncodingUTF8))
            {
                CFRelease(langs);
                return normalize_to_bcp47(std::string(buf));
            }
        }
        CFRelease(langs);
    }

    if (CFLocaleRef loc = CFLocaleCopyCurrent())
    {
        const auto id = static_cast<CFStringRef>(CFLocaleGetIdentifier(loc));
        char buf[128];
        if (CFStringGetCString(id, buf, sizeof(buf), kCFStringEncodingUTF8))
        {
            CFRelease(loc);
            return normalize_to_bcp47(std::string(buf));
        }
        CFRelease(loc);
    }
#endif

    const char *env_vars[] = {"LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"};
    for (const char *name : env_vars)
    {
        if (const char *value = std::getenv(name); value && *value)
        {
            return normalize_to_bcp47(std::string(value));
        }
    }

    return "en-US";
}
