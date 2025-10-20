#include "../src/core/config_manager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <random>

namespace fs = std::filesystem;

static std::string UniqueSuffix()
{
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<unsigned long>(now));
    return std::to_string(rng());
}

static fs::path MakeTempDir()
{
    return fs::temp_directory_path() / "fast_calc_config_manager_tests" / UniqueSuffix();
}

struct TempDirGuard
{
    explicit TempDirGuard(const fs::path &target) : path(target) {}
    ~TempDirGuard()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path path;
};

struct EnvVarGuard
{
    EnvVarGuard(const std::string &name, const std::string &value) : name(name)
    {
#ifdef _WIN32
        char *previous_raw = nullptr;
        size_t length = 0;
        if (_dupenv_s(&previous_raw, &length, name.c_str()) == 0 && previous_raw)
        {
            previous = std::string(previous_raw, length ? length - 1 : 0);
            std::free(previous_raw);
        }
        _putenv_s(name.c_str(), value.c_str());
#else
        if (const char *existing = std::getenv(name.c_str()))
        {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
#endif
    }

    ~EnvVarGuard()
    {
#ifdef _WIN32
        if (previous)
        {
            _putenv_s(name.c_str(), previous->c_str());
        }
        else
        {
            _putenv_s(name.c_str(), "");
        }
#else
        if (previous)
        {
            setenv(name.c_str(), previous->c_str(), 1);
        }
        else
        {
            unsetenv(name.c_str());
        }
#endif
    }

    std::string name;
    std::optional<std::string> previous;
};

TEST_CASE("ConfigManager persists settings to TOML files", "[ConfigManager]")
{
    const auto base_dir = MakeTempDir();
    TempDirGuard cleanup(base_dir);

    const auto config_path = base_dir / "settings.toml";
    ConfigManager manager(config_path.string());

    manager.load();
    manager.set_color("background", "blue");
    manager.set_key("open", "Ctrl+O");
    manager.save();

    REQUIRE(fs::exists(config_path));

    toml::table stored;
    REQUIRE_NOTHROW(stored = toml::parse_file(config_path.string()));

    const auto stored_color = stored["colors"]["background"].value_or(std::string{});
    const auto stored_key = stored["keys"]["open"].value_or(std::string{});

    CHECK(stored_color == "blue");
    CHECK(stored_key == "Ctrl+O");

    ConfigManager reloaded(config_path.string());
    reloaded.load();
    CHECK(reloaded.get_color("background") == "blue");
    CHECK(reloaded.get_key("open") == "Ctrl+O");
    CHECK(reloaded.get_color("missing").empty());
    CHECK(reloaded.get_key("missing").empty());
}

TEST_CASE("ConfigManager stores relative paths inside platform config directory", "[ConfigManager]")
{
    const auto temp_root = MakeTempDir();
    TempDirGuard cleanup(temp_root);

#if defined(_WIN32)
    EnvVarGuard appdata_guard("APPDATA", temp_root.string());
    EnvVarGuard local_guard("LOCALAPPDATA", temp_root.string());
    const fs::path expected_root = temp_root;
#elif defined(__APPLE__)
    EnvVarGuard home_guard("HOME", temp_root.string());
    const fs::path expected_root = temp_root / "Library" / "Application Support";
#else
    EnvVarGuard xdg_guard("XDG_CONFIG_HOME", temp_root.string());
    EnvVarGuard home_guard("HOME", temp_root.string());
    const fs::path expected_root = temp_root;
#endif

    const fs::path relative_path = fs::path("fast_calc") / "test_app";
    const std::string relative_str = relative_path.generic_string();

    ConfigManager manager(relative_str);
    manager.load();
    manager.save();

    const fs::path expected_file = expected_root / relative_path / "config.toml";
    REQUIRE(fs::exists(expected_file));
}
