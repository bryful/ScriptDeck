#include "Utf8Path.h"
#include "LaunchOptions.h"
#include <iostream>

LaunchOptions ParseCommandLine(const std::vector<std::string>& args)
{
    LaunchOptions o;
    std::vector<std::string> positional;
    bool modeSet = false, literal = false;
    for (const auto& arg : args) {
        // GUIのDeck指定後は、起動オプションではなくスクリプト用引数。
        if (o.mode != LaunchMode::Console && !positional.empty()) {
            if (!literal && arg == "--") { literal = true; continue; }
            positional.push_back(arg); continue;
        }
        if (!literal && arg == "--") { literal = true; continue; }
        if (!literal && (arg == "-h" || arg == "--help" || arg == "/?")) {
            o.showHelp = true; continue;
        }
        if (!literal && (arg == "-magic" || arg == "-run")) {
            if (modeSet) { o.error = "Specify only one launch mode."; return o; }
            o.mode = arg == "-magic" ? LaunchMode::Magic : LaunchMode::Console;
            modeSet = true; continue;
        }
        // -run の入力は - で始まる文字列も受け付ける。
        if (!literal && !arg.empty() && arg[0] == '-' &&
            !(o.mode == LaunchMode::Console && positional.size() >= 2)) {
            o.error = "Unknown option: " + arg; return o;
        }
        positional.push_back(arg);
    }
    if (o.showHelp) return o;
    if (o.mode == LaunchMode::Console) {
        if (positional.size() < 2 || positional.size() > 3) {
            o.error = "Usage: ScriptDeck.exe -run deckfile command [input]"; return o;
        }
        o.command = positional[1];
        if (positional.size() == 3) o.input = positional[2];
    } else if (positional.size() > 1) {
        o.scriptArgs.assign(positional.begin()+1,positional.end());
    }
    // -run 以外では最初の通常引数をStackファイルとして扱う
    if (!positional.empty()) o.stackFile = PathFromUtf8(positional[0]);
    return o;
}
LaunchOptions ParseCommandLine(int argc, char* argv[])
{
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return ParseCommandLine(args);
}
void PrintHelp()
{
    std::cout << "ScriptDeck\n\nUsage:\n"
        << "  ScriptDeck.exe [deckfile [args...]]\n"
        << "  ScriptDeck.exe -magic [deckfile [args...]]\n"
        << "  ScriptDeck.exe -run deckfile info\n"
        << "  ScriptDeck.exe -run deckfile dump\n"
        << "  ScriptDeck.exe -run deckfile validate\n"
        << "  ScriptDeck.exe -run deckfile copy output.deck\n\n"
        << "Default: Player. -magic: text editor. -run: JSON utilities.\n"
        << "Use -- to end option parsing. Player/Magic preview execute JavaScript with app and fs APIs.\n";
}
