#pragma once
#include <filesystem>
#include <string>

// C++17/C++20のUTF-8文字列型の違いをここで吸収する。
inline std::filesystem::path PathFromUtf8(const std::string& text)
{
#if defined(__cpp_char8_t)
    std::u8string value;
    value.reserve(text.size());
    for (unsigned char byte : text) value.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(value);
#else
    return std::filesystem::u8path(text);
#endif
}
inline std::string PathToUtf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
#if defined(__cpp_char8_t)
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
    return value;
#endif
}
