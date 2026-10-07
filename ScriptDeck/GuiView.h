#pragma once
#include "Stack.h"
#include "nlohmann/json.hpp"
#include <optional>
#include "LaunchOptions.h"
#include "imgui.h"
#include <functional>

struct GuiServices
{
    std::function<std::filesystem::path(bool)> chooseFile;
    std::function<bool()> discardChanges;
    std::function<void(const std::string&)> reportSaveError;
    std::function<std::filesystem::path()> chooseImage;
    std::function<ImTextureID(const std::filesystem::path&)> image;
    std::vector<std::pair<std::string, std::string>> imageResources;
    std::function<ImTextureID(const std::string&)> resourceImage;
    std::function<void(bool)> setConsoleMode;
};
struct ButtonEvent {
    std::string cardId; std::string buttonId;
    std::string handler = "mouseUp";
    bool checked = false;
    std::string group;
    unsigned long long generation = 0;
};
struct ConsoleCommand { std::string code; unsigned long long generation = 0; int readyFrame = 0; };
struct FileDropEvent {
    std::string cardId;
    std::vector<std::string> paths;
    double x = 0, y = 0;
    unsigned long long generation = 0;
};

class GuiView
{
public:
    GuiView(const LaunchOptions& options, GuiServices services);
    void Draw();
    std::vector<ConsoleCommand> TakeConsoleCommands();
    bool IsScriptConsoleOpen() const { return scriptConsoleOpen_; }
    std::vector<ButtonEvent> TakeButtonEvents();
    bool QueueFileDrop(const std::vector<std::string>& paths, ImVec2 clientPosition);
    std::vector<FileDropEvent> TakeFileDropEvents();
    bool NavigateCard(const std::string& action, const nlohmann::json& target=nullptr);
    void RequestDeckChange(const std::filesystem::path& path, bool saveCurrent=true);
    void RequestHome(bool saveCurrent=true);
    void RequestOpenDeck(const std::filesystem::path& path={});
    void SaveDeck(const std::filesystem::path& path={});
    bool HasPendingDeckChange() const { return pendingDeck_.has_value(); }
    bool ApplyPendingDeckChange();
    void CancelPendingDeckChange() { pendingDeck_.reset(); }
    void SetMagic(bool enabled);
    void SetConsoleMode(bool enabled);
    bool IsMagic() const { return magic_; }
    Stack& MutableDeckData() { return stack_; }
    void NotifyScriptMutation() { dirty_ = true; }
    const Stack& DeckData() const { return stack_; }
    unsigned long long ScriptGeneration() const { return scriptGeneration_; }
    bool ScriptsEnabled() const { return !magic_ || test_; }
    bool HasUnsavedChanges() const { return dirty_; }
    std::string WindowTitle() const;
    const std::filesystem::path& DeckPath() const { return path_; }
    void SetCardSize(int width, int height);
    const std::string& StartupPlacement() const { return stack_.startupPlacement; }
    bool CanClose();
    int CardWidth() const { return stack_.width; }
    int CardHeight() const { return stack_.height; }
    const std::string& DeckName() const { return stack_.name; }
private:
    struct PendingDeck { Stack data; std::filesystem::path path; bool saveCurrent; };
    std::optional<PendingDeck> pendingDeck_;
    Stack stack_;
    std::filesystem::path path_;
    GuiServices services_;
    bool magic_ = false, test_ = false, dirty_ = false;
    bool scriptConsoleOpen_ = false, scriptConsoleFocus_ = false;
    std::string scriptConsoleInput_;
    std::vector<ConsoleCommand> consoleCommands_;
    void DrawScriptConsole();
    std::string selected_, status_;
    std::vector<ButtonEvent> buttonEvents_;
    std::vector<FileDropEvent> fileDropEvents_;
    ImVec2 cardOrigin_{}, cardExtent_{}, cardClipMin_{}, cardClipMax_{};
    std::string drawnCardId_;
    unsigned long long drawnGeneration_ = 0;
    bool popupWasOpen_ = false;
    unsigned long long scriptGeneration_ = 1;
    void ChangeChecked(Card& card, const std::string& objectId, bool value);
    void PushButton(Card& card, const std::string& buttonId);
    void Toolbar();
    void Editor();
    void Canvas();
    bool Save(bool saveAs);
    void Open();
    void New();
    Card* Current();
};
int RunGui(const LaunchOptions& options);
