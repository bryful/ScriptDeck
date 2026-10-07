#pragma once
#include <filesystem>
#include <optional>
#include <vector>
#include <string>
#include "nlohmann/json.hpp"
#include "ScriptIO.h"
struct FileDialogFilter { std::string name, pattern; };
struct FileDialogOptions {
    std::string title, defaultName, defaultExtension;
    std::filesystem::path initialDirectory;
    std::vector<FileDialogFilter> filters;
    bool multiple = false;
};
FileDialogOptions ParseFileDialogOptions(const nlohmann::json& value, bool save, const ScriptFiles& files);
#ifdef _WIN32
std::vector<std::filesystem::path> ShowOpenFileDialog(void* owner, const FileDialogOptions& options);
std::optional<std::filesystem::path> ShowSaveFileDialog(void* owner, const FileDialogOptions& options);
#endif
