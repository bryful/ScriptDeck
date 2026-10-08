#include "FileDialogs.h"
#include "Utf8Path.h"
#include <stdexcept>
#include <algorithm>
namespace {
std::string String(const nlohmann::json& value) {
    if(!value.is_string())throw std::invalid_argument("Dialog option must be a string.");
    auto text=value.get<std::string>();
    if(text.find('\0')!=std::string::npos)throw std::invalid_argument("Dialog strings must not contain NUL.");
    return text;
}
void FileName(const std::string& value) {
    if(value.find_first_of("\\/:*?\"<>|")!=std::string::npos ||
       std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32;}))
        throw std::invalid_argument("Invalid default filename or extension.");
}
}
FileDialogOptions ParseFileDialogOptions(const nlohmann::json& value,bool save,const ScriptFiles& files) {
    if(!value.is_object())throw std::invalid_argument("File dialog options must be an object.");
    FileDialogOptions result;
    for(auto option=value.begin();option!=value.end();++option) {
        const auto& key=option.key();
        if(key=="title")result.title=String(option.value());
        else if(key=="defaultName"){result.defaultName=String(option.value());FileName(result.defaultName);}
        else if(key=="defaultExtension") {
            result.defaultExtension=String(option.value());
            if(!result.defaultExtension.empty()&&result.defaultExtension.front()=='.')result.defaultExtension.erase(0,1);
            FileName(result.defaultExtension);
        } else if(key=="initialDirectory") {
            const auto path=String(option.value());if(!path.empty())result.initialDirectory=files.Resolve(path);
        } else if(key=="multiple") {
            if(save)throw std::invalid_argument("SaveFileDialog does not support multiple.");
            if(!option.value().is_boolean())throw std::invalid_argument("multiple must be a boolean.");
            result.multiple=option.value().get<bool>();
        } else if(key=="filters") {
            if(!option.value().is_array()||option.value().empty())throw std::invalid_argument("filters must be a nonempty array.");
            for(const auto& filter:option.value()) {
                if(!filter.is_object()||filter.size()!=2||!filter.contains("name")||!filter.contains("pattern"))throw std::invalid_argument("Filter requires name and pattern.");
                FileDialogFilter item{String(filter.at("name")),String(filter.at("pattern"))};
                if(item.name.empty()||item.pattern.empty()||item.pattern.find_first_of("\\/")!=std::string::npos)throw std::invalid_argument("Invalid file dialog filter.");
                result.filters.push_back(std::move(item));
            }
        } else throw std::invalid_argument("Unknown file dialog option: "+key);
    }
    if(result.filters.empty())result.filters.push_back({"All files (*.*)","*.*"});
    return result;
}
FileDialogOptions ParseFolderDialogOptions(const nlohmann::json& value,const ScriptFiles& files) {
    if(!value.is_object())throw std::invalid_argument("Folder dialog options must be an object.");
    for(auto option=value.begin();option!=value.end();++option)
        if(option.key()!="title" && option.key()!="initialDirectory")
            throw std::invalid_argument("Unknown folder dialog option: "+option.key());
    return ParseFileDialogOptions(value,false,files);
}
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace {
void Require(HRESULT result,const char* action) {
    if(FAILED(result))throw std::runtime_error(std::string(action)+" (HRESULT "+std::to_string(static_cast<long>(result))+")");
}
void Configure(IFileDialog* dialog,const FileDialogOptions& options,bool save) {
    FILEOPENDIALOGOPTIONS flags{};Require(dialog->GetOptions(&flags),"Cannot get dialog options");
    flags|=FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|FOS_PATHMUSTEXIST;
    flags|=save?FOS_OVERWRITEPROMPT:FOS_FILEMUSTEXIST;
    if(options.multiple)flags|=FOS_ALLOWMULTISELECT;
    Require(dialog->SetOptions(flags),"Cannot set dialog options");
    std::vector<std::wstring> names,patterns;names.reserve(options.filters.size());patterns.reserve(options.filters.size());
    for(const auto& filter:options.filters){names.push_back(PathFromUtf8(filter.name).native());patterns.push_back(PathFromUtf8(filter.pattern).native());}
    std::vector<COMDLG_FILTERSPEC> filters;for(std::size_t i=0;i<names.size();++i)filters.push_back({names[i].c_str(),patterns[i].c_str()});
    Require(dialog->SetFileTypes(static_cast<UINT>(filters.size()),filters.data()),"Cannot set file filters");
    Require(dialog->SetFileTypeIndex(1),"Cannot set file filter index");
    if(!options.title.empty())Require(dialog->SetTitle(PathFromUtf8(options.title).c_str()),"Cannot set dialog title");
    if(!options.defaultName.empty())Require(dialog->SetFileName(PathFromUtf8(options.defaultName).c_str()),"Cannot set default filename");
    if(!options.defaultExtension.empty())Require(dialog->SetDefaultExtension(PathFromUtf8(options.defaultExtension).c_str()),"Cannot set default extension");
    if(!options.initialDirectory.empty()) {
        if(!std::filesystem::is_directory(options.initialDirectory))throw std::invalid_argument("initialDirectory must be an existing directory.");
        ComPtr<IShellItem> folder;Require(SHCreateItemFromParsingName(options.initialDirectory.c_str(),nullptr,IID_PPV_ARGS(folder.GetAddressOf())),"Cannot resolve initial directory");
        Require(dialog->SetFolder(folder.Get()),"Cannot set initial directory");
    }
}
std::filesystem::path ItemPath(IShellItem* item) {
    PWSTR name=nullptr;Require(item->GetDisplayName(SIGDN_FILESYSPATH,&name),"Cannot get selected path");
    try {std::filesystem::path result(name);CoTaskMemFree(name);return result;}
    catch(...) {CoTaskMemFree(name);throw;}
}
}
std::vector<std::filesystem::path> ShowOpenFileDialog(void* owner,const FileDialogOptions& options) {
    ComPtr<IFileOpenDialog> dialog;
    Require(CoCreateInstance(__uuidof(FileOpenDialog),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())),"Cannot create OpenFileDialog");
    Configure(dialog.Get(),options,false);const auto result=dialog->Show(static_cast<HWND>(owner));
    if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};
    Require(result,"OpenFileDialog failed");
    ComPtr<IShellItemArray> items;Require(dialog->GetResults(items.GetAddressOf()),"Cannot get selected files");
    DWORD count=0;Require(items->GetCount(&count),"Cannot get file count");
    std::vector<std::filesystem::path> paths;for(DWORD i=0;i<count;++i){ComPtr<IShellItem> item;Require(items->GetItemAt(i,item.GetAddressOf()),"Cannot get selected file");paths.push_back(ItemPath(item.Get()));}
    return paths;
}
std::optional<std::filesystem::path> ShowSaveFileDialog(void* owner,const FileDialogOptions& options) {
    ComPtr<IFileSaveDialog> dialog;
    Require(CoCreateInstance(__uuidof(FileSaveDialog),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())),"Cannot create SaveFileDialog");
    Configure(dialog.Get(),options,true);const auto result=dialog->Show(static_cast<HWND>(owner));
    if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED))return std::nullopt;
    Require(result,"SaveFileDialog failed");ComPtr<IShellItem> item;Require(dialog->GetResult(item.GetAddressOf()),"Cannot get save path");
    return ItemPath(item.Get());
}
std::optional<std::filesystem::path> ShowFolderDialog(void* owner,const FileDialogOptions& options) {
    ComPtr<IFileOpenDialog> dialog;
    Require(CoCreateInstance(__uuidof(FileOpenDialog),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())),"Cannot create FolderDialog");
    FILEOPENDIALOGOPTIONS flags{};
    Require(dialog->GetOptions(&flags),"Cannot get folder dialog options");
    flags|=FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|FOS_PATHMUSTEXIST;
    flags&=~FOS_ALLOWMULTISELECT;
    Require(dialog->SetOptions(flags),"Cannot set folder dialog options");
    if(!options.title.empty())Require(dialog->SetTitle(PathFromUtf8(options.title).c_str()),"Cannot set folder dialog title");
    if(!options.initialDirectory.empty()) {
        if(!std::filesystem::is_directory(options.initialDirectory))throw std::invalid_argument("initialDirectory must be an existing directory.");
        ComPtr<IShellItem> folder;
        Require(SHCreateItemFromParsingName(options.initialDirectory.c_str(),nullptr,IID_PPV_ARGS(folder.GetAddressOf())),"Cannot resolve initial directory");
        Require(dialog->SetFolder(folder.Get()),"Cannot set initial directory");
    }
    const auto result=dialog->Show(static_cast<HWND>(owner));
    if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED))return std::nullopt;
    Require(result,"FolderDialog failed");
    ComPtr<IShellItem> item;
    Require(dialog->GetResult(item.GetAddressOf()),"Cannot get selected folder");
    return ItemPath(item.Get());
}
#endif
