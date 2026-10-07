#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <stdexcept>
#include <limits>
#include <cstring>
namespace TextClipboard {
struct Open {
    explicit Open(HWND owner) {if(!OpenClipboard(owner))throw std::runtime_error("Cannot open clipboard; another application may be using it.");}
    ~Open(){CloseClipboard();}
};
struct Locked {
    HGLOBAL handle;
    void* data;
    explicit Locked(HGLOBAL value):handle(value),data(GlobalLock(value)){if(!data)throw std::runtime_error("Cannot lock clipboard text.");}
    ~Locked(){GlobalUnlock(handle);}
};
inline std::string Read(HWND owner) {
    Open open(owner);
    if(!IsClipboardFormatAvailable(CF_UNICODETEXT))return {};
    const auto handle=static_cast<HGLOBAL>(GetClipboardData(CF_UNICODETEXT));
    if(!handle)throw std::runtime_error("Cannot read clipboard text.");
    const auto count=GlobalSize(handle)/sizeof(wchar_t);
    Locked locked(handle);
    const auto* data=static_cast<const wchar_t*>(locked.data);
    std::size_t length=0;while(length<count && data[length]!=L'\0')++length;
    if(length==count)throw std::runtime_error("Clipboard text is not terminated.");
    if(length==0)return {};
    if(length>32*1024*1024 || length>static_cast<std::size_t>((std::numeric_limits<int>::max)()))throw std::runtime_error("Clipboard text is too large.");
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,data,static_cast<int>(length),nullptr,0,nullptr,nullptr);
    if(size>32*1024*1024)throw std::runtime_error("Clipboard text exceeds 32 MiB.");
    if(!size)throw std::runtime_error("Clipboard contains invalid Unicode text.");
    std::string result(size,'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,data,static_cast<int>(length),result.data(),size,nullptr,nullptr))throw std::runtime_error("Cannot convert clipboard text.");
    return result;
}
inline void Write(HWND owner,const std::string& text) {
    if(text.find('\0')!=std::string::npos)throw std::invalid_argument("Clipboard text cannot contain NUL.");
    if(text.size()>32*1024*1024)throw std::invalid_argument("Clipboard text exceeds 32 MiB.");
    int count=0;
    if(!text.empty()) {
        count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
        if(!count)throw std::invalid_argument("Invalid UTF-8 clipboard text.");
    }
    const auto memory=GlobalAlloc(GMEM_MOVEABLE,(static_cast<std::size_t>(count)+1)*sizeof(wchar_t));
    if(!memory)throw std::runtime_error("Cannot allocate clipboard text.");
    try {
        {
            Locked locked(memory);auto* data=static_cast<wchar_t*>(locked.data);
            if(count && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),data,count))throw std::runtime_error("Cannot convert clipboard text.");
            data[count]=L'\0';
        }
        Open open(owner);
        if(!EmptyClipboard())throw std::runtime_error("Cannot empty clipboard.");
        if(!SetClipboardData(CF_UNICODETEXT,memory))throw std::runtime_error("Cannot write clipboard text.");
        // Windows owns memory after SetClipboardData succeeds.
    } catch(...) {GlobalFree(memory);throw;}
}
}
#endif
