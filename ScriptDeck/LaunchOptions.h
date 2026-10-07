#pragma once
#include <string>
#include <filesystem>
#include <vector>

enum class LaunchMode { Player, Magic, Console };
struct LaunchOptions
{
    LaunchMode mode = LaunchMode::Player;
    std::filesystem::path stackFile;
    std::string command;
    std::string input;
    std::vector<std::string> scriptArgs;
    bool showHelp = false;
    std::string error;
};
LaunchOptions ParseCommandLine(const std::vector<std::string>& args);
LaunchOptions ParseCommandLine(int argc, char* argv[]);
void PrintHelp();
