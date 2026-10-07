#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <stdexcept>

// UIスレッドで使用する、プロセス共通のコンソール管理。
class ConsoleMode
{
public:
    static ConsoleMode& Instance() { static ConsoleMode mode; return mode; }
    void SetEnabled(bool enabled, bool forceWindow = true)
    {
        CaptureRedirects();
        if (enabled) {
            if (enabled_) return;
            attached_ = GetConsoleWindow() != nullptr;
            if (!attached_) attached_ = AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
            if (!attached_) {
                const DWORD error = GetLastError();
                if (error == ERROR_ACCESS_DENIED) attached_ = true;
                else if (forceWindow || (!redirects_[1] && !redirects_[2])) {
                    if (!AllocConsole()) throw std::runtime_error("Cannot allocate console.");
                    attached_ = true;
                }
            }
            for (int i = 0; i < 3; ++i) Bind(i, redirects_[i] ? redirects_[i] : GetStdHandle(Ids()[i]));
            SetConsoleCP(CP_UTF8); SetConsoleOutputCP(CP_UTF8);
            enabled_ = true;
        } else {
            if (!enabled_) return;
            std::cout.flush(); std::cerr.flush(); std::fflush(nullptr);
            // コンソールのハンドルをCRTから切り離してから接続を解除する。
            for (int i = 0; i < 3; ++i) Bind(i, redirects_[i]);
            if (attached_ && !FreeConsole()) throw std::runtime_error("Cannot detach console.");
            for (int i = 0; i < 3; ++i) SetStdHandle(Ids()[i], redirects_[i]);
            attached_ = false;
            enabled_ = false;
        }
        std::cout.clear(); std::cerr.clear(); std::cin.clear();
    }
private:
    HANDLE redirects_[3] = {};
    bool captured_ = false, enabled_ = false, attached_ = false;
    static const DWORD* Ids() {
        static const DWORD ids[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}; return ids;
    }
    void CaptureRedirects()
    {
        if (captured_) return;
        for (int i = 0; i < 3; ++i) {
            const auto handle = GetStdHandle(Ids()[i]);
            if (!handle || handle == INVALID_HANDLE_VALUE) continue;
            const auto type = GetFileType(handle);
            if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE) {
                if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &redirects_[i], 0, FALSE, DUPLICATE_SAME_ACCESS))
                    throw std::runtime_error("Cannot preserve redirected stream.");
            }
        }
        captured_ = true;
    }
    static void Bind(int index, HANDLE handle)
    {
        FILE* streams[] = {stdin, stdout, stderr};
        HANDLE duplicate = nullptr;
        if (handle && handle != INVALID_HANDLE_VALUE &&
            !DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS))
            throw std::runtime_error("Cannot duplicate console stream.");
        const int fd = duplicate ? _open_osfhandle(reinterpret_cast<intptr_t>(duplicate), _O_BINARY | (index == 0 ? _O_RDONLY : _O_WRONLY)) : -1;
        if (duplicate && fd < 0) { CloseHandle(duplicate); throw std::runtime_error("Cannot open console stream."); }
        FILE* reopened = nullptr;
        const int result = freopen_s(&reopened, "NUL", index == 0 ? "rb" : "wb", streams[index]);
        const bool failed = result != 0 || (fd >= 0 && _dup2(fd, _fileno(streams[index])) != 0);
        if (fd >= 0) _close(fd);
        if (failed) throw std::runtime_error("Cannot bind console stream.");
        std::setvbuf(streams[index], nullptr, _IONBF, 0);
    }
    ~ConsoleMode() { for (auto handle : redirects_) if (handle) CloseHandle(handle); }
};
#endif
