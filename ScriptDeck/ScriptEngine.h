#pragma once
#include <functional>
#include <memory>
#include <string>
#include "ScriptIO.h"
#include "ScriptModel.h"
#include "FileDialogs.h"

struct ScriptHost
{
    std::shared_ptr<ScriptModel> model;
    std::function<std::vector<std::filesystem::path>(const FileDialogOptions&)> openFileDialog;
    std::function<std::optional<std::filesystem::path>(const FileDialogOptions&)> saveFileDialog;
    std::vector<std::string> args;
    std::filesystem::path workingDirectory;
    std::function<bool(const std::string&,const nlohmann::json&)> navigateCard;
    std::function<void(const std::filesystem::path&,bool)> changeDeck;
    std::function<void(bool)> goHome;
    std::function<std::filesystem::path()> getDeckPath;
    std::function<bool()> install;
    std::function<bool()> uninstall;
    std::function<void(bool)> setTopMost;
    std::function<bool()> getTopMost;
    std::function<void()> windowFront;
    std::function<void(bool)> setMagic;
    std::function<void(bool)> setConsoleMode;
    std::function<void(int,int)> beep;
    std::function<void(const std::filesystem::path&,bool)> playWav;
    std::function<void()> stopWav;
    std::function<ScriptBytes(std::size_t,bool)> readInput;
    std::function<void(const ScriptBytes&,bool)> writeOutput;
    std::function<void()> flushOutput;
};

// UIスレッドで使用する組み込みAPI付きJavaScriptエンジン。
class ScriptEngine
{
public:
    using AlertCallback = std::function<void(const std::string&)>;
    explicit ScriptEngine(AlertCallback alert, ScriptHost host = {});
    bool ExitRequested() const;
    int ExitCode() const;
    void ClearExitRequest();
    ~ScriptEngine();
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;
    void RunGlobal(const std::string& code, const std::string& sourceName);
    void RunScoped(const std::string& code, const std::string& sourceName, const std::string& handler, const std::string& cardId = {}, const std::string& objectId = {}, const nlohmann::json& eventData = nlohmann::json::object());
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
