#pragma once
#ifdef _WIN32
#include "ScriptSystem.h"
#include <vector>

inline std::filesystem::path FindVSCodeExecutable() {
    std::vector<std::filesystem::path> candidates;
    for(auto key:{L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Code.exe"}) {
        for(auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE}) {
            wchar_t value[32768]{};DWORD size=sizeof(value);
            if(RegGetValueW(hive,key,nullptr,RRF_RT_REG_SZ,nullptr,value,&size)==ERROR_SUCCESS)candidates.emplace_back(value);
        }
    }
    for(auto name:{L"Code.exe",L"code.cmd"}) {
        wchar_t found[32768]{};
        const auto size=SearchPathW(nullptr,name,nullptr,32768,found,nullptr);
        if(size && size<32768) {
            auto path=std::filesystem::path(found);
            candidates.push_back(path.extension()==L".cmd"?path.parent_path().parent_path()/"Code.exe":path);
        }
    }
    for(auto name:{"LOCALAPPDATA","ProgramFiles","ProgramFiles(x86)"}) {
        const auto value=ScriptSystem::Environment(name);if(!value)continue;
        auto base=PathFromUtf8(*value);
        if(std::string(name)=="LOCALAPPDATA")base/="Programs";
        candidates.push_back(base/"Microsoft VS Code"/"Code.exe");
    }
    for(const auto& path:candidates)if(std::filesystem::is_regular_file(path))return path;
    throw std::runtime_error("VSCodeが見つかりません。VSCodeをインストールするか、codeコマンドのPATHを設定してください。");
}
inline void OpenVSCodeFolder(const std::filesystem::path& folder) {
    if(!std::filesystem::is_directory(folder))throw std::runtime_error("Deck folder does not exist.");
    ScriptSystem::Process(PathToUtf8(FindVSCodeExecutable()),{"--new-window",PathToUtf8(folder)},folder,false);
}
#endif
