#pragma once
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <string>

// 前回のウィンドウ状態。デッキ本体とは別に保存する。
struct WindowState
{
    int left = 0, top = 0, width = 640, height = 480;
    bool maximized = false;
    bool Load(const std::filesystem::path& path)
    {
        try {
            std::ifstream f(path, std::ios::binary);
            if (!f) return false;
            nlohmann::json j; f >> j;
            WindowState s;
            const char* keys[] = {"left", "top", "width", "height"};
            int* values[] = {&s.left, &s.top, &s.width, &s.height};
            for (int i=0; i<4; ++i) {
                const auto& v = j.at(keys[i]);
                if (!v.is_number_integer()) return false;
                if (v.is_number_unsigned() && v.get<std::uint64_t>() > 2147483647ULL) return false;
                const auto n = v.get<std::int64_t>();
                if (n < -2147483647LL-1 || n > 2147483647LL) return false;
                *values[i] = static_cast<int>(n);
            }
            if (s.width < 1 || s.height < 1 || s.width > 8192 || s.height > 8192) return false;
            if (static_cast<std::int64_t>(s.left) + s.width > 2147483647LL ||
                static_cast<std::int64_t>(s.top) + s.height > 2147483647LL) return false;
            s.maximized = j.value("maximized", false);
            *this = s; return true;
        } catch (...) { return false; }
    }
    bool Save(const std::filesystem::path& path) const
    {
        try {
            if (path.empty()) return false;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            if (!f) return false;
            f << nlohmann::json({{"left", left}, {"top", top}, {"width", width}, {"height", height}, {"maximized", maximized}}).dump(2);
            f.close(); return static_cast<bool>(f);
        } catch (...) { return false; }
    }
};

enum class StartupAction { Default, Restore, Center };
inline bool UsesPreviousState(const std::string& policy)
{
    return policy == "previous" || policy == "previous_default" || policy == "previous_center";
}
inline StartupAction ResolveStartupAction(const std::string& policy, bool hasPreviousState)
{
    if (UsesPreviousState(policy) && hasPreviousState) return StartupAction::Restore;
    if (policy == "center" || policy == "previous_center") return StartupAction::Center;
    return StartupAction::Default;
}
