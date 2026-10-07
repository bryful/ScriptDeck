#pragma once
#include "FileAssociation.h"
#include <algorithm>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
namespace DeckAssociation {
inline void CheckRegistry(LSTATUS code) {
    if(code!=ERROR_SUCCESS)throw std::runtime_error("File association registry error: "+std::to_string(code));
}
class NativeRegistry final:public Registry {
    struct Key {HKEY value=nullptr;~Key(){if(value)RegCloseKey(value);}};
public:
    std::optional<Value> Read(const std::wstring& path,const std::wstring& name) override {
        Key key;auto status=RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_QUERY_VALUE,&key.value);
        if(status==ERROR_FILE_NOT_FOUND || status==ERROR_PATH_NOT_FOUND)return std::nullopt;
        CheckRegistry(status);DWORD type=0,size=0;
        status=RegQueryValueExW(key.value,name.c_str(),nullptr,&type,nullptr,&size);
        if(status==ERROR_FILE_NOT_FOUND)return std::nullopt;CheckRegistry(status);
        if(size>1024*1024)throw std::runtime_error("Association registry value is too large.");
        Value value{type,std::vector<unsigned char>(size)};
        CheckRegistry(RegQueryValueExW(key.value,name.c_str(),nullptr,&type,value.bytes.data(),&size));
        value.type=type;value.bytes.resize(size);return value;
    }
    void Write(const std::wstring& path,const std::wstring& name,const Value& value) override {
        Key key;CheckRegistry(RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&key.value,nullptr));
        CheckRegistry(RegSetValueExW(key.value,name.c_str(),0,value.type,value.bytes.data(),static_cast<DWORD>(value.bytes.size())));
    }
    void RemoveValue(const std::wstring& path,const std::wstring& name) override {
        Key key;auto status=RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_SET_VALUE,&key.value);
        if(status==ERROR_FILE_NOT_FOUND || status==ERROR_PATH_NOT_FOUND)return;CheckRegistry(status);
        status=RegDeleteValueW(key.value,name.c_str());if(status!=ERROR_FILE_NOT_FOUND)CheckRegistry(status);
    }
    void RemoveTree(const std::wstring& path) override {
        const auto status=RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());
        if(status!=ERROR_FILE_NOT_FOUND && status!=ERROR_PATH_NOT_FOUND)CheckRegistry(status);
    }
};
inline std::wstring ExecutablePath() {
    std::vector<wchar_t> buffer(512);
    for(;;) {
        const DWORD length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
        if(!length)throw std::runtime_error("Cannot locate ScriptDeck executable.");
        if(length<buffer.size()-1)return std::wstring(buffer.data(),length);
        if(buffer.size()>=32768)throw std::runtime_error("Executable path is too long.");
        buffer.resize((std::min)(std::size_t(32768),buffer.size()*2));
    }
}
inline bool InstallCurrentApplication() {
    NativeRegistry registry;const bool result=Install(registry,ExecutablePath());
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);return result;
}
inline bool UninstallCurrentApplication() {
    NativeRegistry registry;const bool result=Uninstall(registry,ExecutablePath());
    if(result)SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);return result;
}
}
#endif
