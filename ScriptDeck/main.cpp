#include "LaunchOptions.h"
#include "ScriptDeckApp.h"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include "ConsoleMode.h"
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#endif

static int RunApp(const LaunchOptions& options)
{
    if (options.showHelp) { PrintHelp(); return 0; }
    if (!options.error.empty()) { std::cerr << "ERROR: " << options.error << '\n'; return 1; }
    ScriptDeckApp app;
    return app.Run(options);
}
#ifdef _WIN32
static std::string Utf8(const wchar_t* value)
{
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Cannot convert command line to UTF-8.");
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            result.data(), size, nullptr, nullptr))
        throw std::runtime_error("Cannot convert command line to UTF-8.");
    result.pop_back();
    return result;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    bool console = false;
    try {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) throw std::runtime_error("Cannot parse command line.");
        std::vector<std::string> args;
        try { for (int i = 1; i < argc; ++i) args.push_back(Utf8(argv[i])); }
        catch (...) { LocalFree(argv); throw; }
        LocalFree(argv);
        const auto options = ParseCommandLine(args);
        console = options.mode == LaunchMode::Console || options.showHelp;
        if (console) ConsoleMode::Instance().SetEnabled(true, false);
        if (!console && !options.error.empty()) throw std::runtime_error(options.error);
        const int result = RunApp(options);
        if (console) { std::cout.flush(); std::cerr.flush(); }
        return result;
    } catch (const std::exception& e) {
        if (console) { std::cerr << "ERROR: " << e.what() << '\n'; std::cerr.flush(); }
        else {
            const std::string message = std::string("ERROR: ") + e.what();
            const int length = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
            std::wstring wide(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wide.data(), length);
            MessageBoxW(nullptr, wide.c_str(), L"ScriptDeck", MB_OK | MB_ICONERROR);
        }
        return 1;
    }
}
#else
int main(int argc, char* argv[])
{
    try { return RunApp(ParseCommandLine(argc, argv)); }
    catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << '\n'; return 1; }
}
#endif
