#ifdef _WIN32
#include "GuiView.h"
#endif
#include "Utf8Path.h"
#include "ScriptDeckApp.h"
#include "Stack.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <stdexcept>
using nlohmann::json;

namespace {
void ShowStack(const Stack& s)
{
    std::cout << "Stack: " << s.name << " (" << s.width << 'x' << s.height << ")\n";
    for (const auto& c : s.cards) {
        std::cout << (c.id == s.currentCardId ? "* " : "  ") << c.id << ": " << c.name << '\n';
        for (const auto& o : c.objects)
            std::cout << "    " << o.id << " [" << ObjectTypeName(o.type) << "] "
                      << o.name << " text=" << o.text << '\n';
    }
}
bool Ask(const std::string& prompt, std::string& answer)
{
    std::cout << prompt << std::flush;
    return static_cast<bool>(std::getline(std::cin, answer));
}
int Error(const Stack& s)
{
    std::cerr << "ERROR: " << s.LastError() << '\n'; return 1;
}
}
int ScriptDeckApp::Run(const LaunchOptions& options)
{
#ifdef _WIN32
    if (options.mode != LaunchMode::Console) return RunGui(options);
#endif
    switch (options.mode) {
    case LaunchMode::Player: return RunPlayer(options);
    case LaunchMode::Magic: return RunMagic(options);
    case LaunchMode::Console: return RunConsole(options);
    }
    return 1;
}
int ScriptDeckApp::RunPlayer(const LaunchOptions& options)
{
    std::cout << "=== Player Mode ===\n";
    if (options.stackFile.empty()) {
        std::cout << "No deck file specified.\n"; return 0;
    }
    Stack s;
    if (!s.Load(options.stackFile)) return Error(s);
    ShowStack(s);
    // GUIとJavaScript実行は後の段階で追加する。
    return 0;
}
int ScriptDeckApp::RunMagic(const LaunchOptions& options)
{
    std::cout << "=== Magic Mode ===\n";
    Stack s;
    auto path = options.stackFile;
    if (path.empty()) s.CreateNew();
    else if (!s.Load(path)) return Error(s);
    bool dirty = path.empty();
    ShowStack(s);
    for (;;) {
        std::string choice;
        if (!Ask("\n1:List 2:Rename stack 3:Add card 4:Add object 5:Set text\n"
                 "6:Select card 7:Save 8:Save as 0:Quit\n> ", choice)) {
            if (dirty) std::cerr << "Unsaved changes discarded (input closed).\n";
            return 0;
        }
        try {
            std::string a, b, c;
            if (choice == "1") ShowStack(s);
            else if (choice == "2") {
                if (!Ask("Stack name: ", a)) continue;
                s.name = a; dirty = true;
            } else if (choice == "3") {
                if (!Ask("Card name: ", a)) continue;
                const auto id = s.AddCard(a).id;
                s.currentCardId = id; dirty = true;
                std::cout << "Added " << id << '\n';
            } else if (choice == "4") {
                if (!Ask("Type (button/field/text/image): ", a) || !Ask("Object name: ", b)) continue;
                const auto id = s.AddObject(s.currentCardId, ParseObjectType(a), b).id;
                dirty = true; std::cout << "Added " << id << '\n';
            } else if (choice == "5") {
                if (!Ask("Object id (current card): ", a) || !Ask("Text: ", b)) continue;
                auto* card = s.FindCard(s.currentCardId);
                auto* obj = card ? card->FindObject(a) : nullptr;
                if (!obj) throw std::runtime_error("Object not found.");
                obj->text = b; dirty = true;
            } else if (choice == "6") {
                if (!Ask("Card id: ", a)) continue;
                if (!s.FindCard(a)) throw std::runtime_error("Card not found.");
                s.currentCardId = a; dirty = true;
            } else if (choice == "7" || choice == "8") {
                auto destination = path;
                if (destination.empty() || choice == "8") {
                    if (!Ask("Output deck path (without surrounding quotes): ", a) || a.empty()) continue;
                    destination = PathFromUtf8(a);
                }
                // 既存ファイルへの別名保存は明示確認する。
                if (destination != path && std::filesystem::exists(destination)) {
                    if (!Ask("Overwrite existing file? (y/N): ", a) || a != "y") continue;
                }
                if (!s.Save(destination)) { std::cerr << "ERROR: " << s.LastError() << '\n'; continue; }
                path = destination; dirty = false;
                std::cout << "Saved: " << PathToUtf8(path) << '\n';
            } else if (choice == "0") {
                if (dirty && (!Ask("Discard unsaved changes? (y/N): ", a) || a != "y")) continue;
                return 0;
            } else std::cout << "Unknown menu item.\n";
        } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << '\n'; }
    }
}
int ScriptDeckApp::RunConsole(const LaunchOptions& options)
{
    // Consoleモードでは
    // stdout = 実行結果
    // stderr = エラー
    Stack s;
    if (!s.Load(options.stackFile)) return Error(s);
    if (options.command == "dump") std::cout << s.ToJson();
    else if (options.command == "validate") std::cout << json({{"valid", true}}).dump() << '\n';
    else if (options.command == "info") {
        std::size_t count = 0;
        for (const auto& c : s.cards) count += c.objects.size();
        std::cout << json({{"name", s.name}, {"width", s.width}, {"height", s.height},
            {"cardCount", s.cards.size()}, {"objectCount", count},
            {"currentCardId", s.currentCardId}}).dump() << '\n';
    } else if (options.command == "copy") {
        if (options.input.empty()) { std::cerr << "ERROR: Output path is required.\n"; return 1; }
        const auto destination = PathFromUtf8(options.input);
        if (std::filesystem::exists(destination)) {
            std::cerr << "ERROR: Copy destination already exists.\n"; return 1;
        }
        if (!s.Save(destination)) return Error(s);
        std::cout << json({{"saved", PathToUtf8(destination)}}).dump() << '\n';
    } else {
        std::cerr << "ERROR: Unknown command: " << options.command
                  << ". Available: info, dump, validate, copy.\n";
        return 1;
    }
    return 0;
}
