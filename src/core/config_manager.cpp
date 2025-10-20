#include "config_manager.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string_view>

namespace
{
    namespace fs = std::filesystem;

    fs::path ResolveConfigRoot()
    {
#if defined(_WIN32)
        if (const char *appdata = std::getenv("APPDATA"))
        {
            return fs::path(appdata);
        }
        if (const char *local = std::getenv("LOCALAPPDATA"))
        {
            return fs::path(local);
        }
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"))
        {
            return fs::path(home) / "Library" / "Application Support";
        }
#else
        if (const char *xdg = std::getenv("XDG_CONFIG_HOME"))
        {
            return fs::path(xdg);
        }
        if (const char *home = std::getenv("HOME"))
        {
            return fs::path(home) / ".config";
        }
#endif
        return fs::current_path();
    }

    bool LooksLikeFile(const fs::path &value)
    {
        return value.has_extension() ||
               (!value.empty() && value.filename().string().find('.') != std::string::npos);
    }

    toml::table *EnsureTable(toml::table &root, std::string_view key)
    {
        if (auto *existing = root.get_as<toml::table>(key))
        {
            return existing;
        }

        root.insert_or_assign(key, toml::table{});
        return root.get_as<toml::table>(key);
    }

    const toml::table *FindTable(const toml::table &root, std::string_view key)
    {
        return root.get_as<toml::table>(key);
    }
} // namespace

ConfigManager::ConfigManager(const string &config_path)
{
    namespace fs = std::filesystem;

    fs::path provided = config_path;
    fs::path target;

    if (provided.is_absolute())
    {
        target = provided;
    }
    else
    {
        fs::path base = ResolveConfigRoot();
        if (LooksLikeFile(provided))
        {
            target = base / provided;
        }
        else
        {
            target = base / provided / "config.toml";
        }
    }

    if (!target.has_extension())
    {
        target += ".toml";
    }

    config_file_path_ = fs::absolute(target).string();
}

void ConfigManager::load()
{
    namespace fs = std::filesystem;

    config_data_ = toml::table{};

    const fs::path path = config_file_path_;
    const fs::path parent = path.parent_path();
    if (!parent.empty())
    {
        create_dir(parent.string());
    }

    if (!exists(config_file_path_))
    {
        return;
    }

    try
    {
        config_data_ = toml::parse_file(config_file_path_);
    }
    catch (const toml::parse_error &err)
    {
        std::cerr << "Failed to parse config file \"" << config_file_path_
                  << "\": " << err << std::endl;
        config_data_ = toml::table{};
    }
}

void ConfigManager::save()
{
    namespace fs = std::filesystem;

    const fs::path path = config_file_path_;
    const fs::path parent = path.parent_path();
    if (!parent.empty())
    {
        create_dir(parent.string());
    }

    std::ostringstream stream;
    stream << config_data_;
    write_file(config_file_path_, stream.str());
}

void ConfigManager::set_color(const string &element, const string &color)
{
    if (auto *colors = EnsureTable(config_data_, "colors"))
    {
        colors->insert_or_assign(element, color);
    }
}

string ConfigManager::get_color(const string &element) const
{
    if (const auto *colors = FindTable(config_data_, "colors"))
    {
        if (const auto *value_node = colors->get(element))
        {
            if (auto value = value_node->value<string>())
            {
                return *value;
            }
        }
    }
    return {};
}

void ConfigManager::set_key(const string &action, const string &key)
{
    if (auto *keys = EnsureTable(config_data_, "keys"))
    {
        keys->insert_or_assign(action, key);
    }
}

string ConfigManager::get_key(const string &action) const
{
    if (const auto *keys = FindTable(config_data_, "keys"))
    {
        if (const auto *value_node = keys->get(action))
        {
            if (auto value = value_node->value<string>())
            {
                return *value;
            }
        }
    }
    return {};
}
