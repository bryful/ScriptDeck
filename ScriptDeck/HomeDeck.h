#pragma once
#include "Stack.h"
#include "Utf8Path.h"
#include "ResourceIds.h"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

inline std::filesystem::path HomeDeckPath()
{
#ifdef _WIN32
    PWSTR directory=nullptr;
    const auto status=SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&directory);
    if(FAILED(status)) {CoTaskMemFree(directory);throw std::runtime_error("Cannot locate LocalAppData.");}
    std::filesystem::path result;
    try {result=std::filesystem::path(directory)/L"ScriptDeck"/L"home.deck";}
    catch(...) {CoTaskMemFree(directory);throw;}
    CoTaskMemFree(directory);return result;
#else
    // Portable build/test location; Windows uses the Known Folder API above.
    if(const char* base=std::getenv("XDG_DATA_HOME"))if(*base)return PathFromUtf8(base)/"ScriptDeck"/"home.deck";
    if(const char* base=std::getenv("HOME"))if(*base)return PathFromUtf8(base)/".local"/"share"/"ScriptDeck"/"home.deck";
    throw std::runtime_error("Cannot locate application data directory.");
#endif
}
inline std::string EmbeddedHomeDeck()
{
#ifdef _WIN32
    const auto module=GetModuleHandleW(nullptr);
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(IDR_SCRIPTDECK_HOME),RT_RCDATA);
    if(!resource)throw std::runtime_error("Embedded home.deck resource was not found.");
    const auto data=LoadResource(module,resource);
    const auto bytes=static_cast<const char*>(LockResource(data));
    const auto size=SizeofResource(module,resource);
    if(!bytes||!size)throw std::runtime_error("Cannot read embedded home.deck.");
    return std::string(bytes,size);
#else
    return R"HOMEDECK({
  "format": "ScriptDeck",
  "version": 1,
  "name": "Home",
  "width": 640,
  "height": 480,
  "script": "",
  "startupPlacement": "previous_center",
  "currentCardId": "home",
  "cards": [
    {
      "id": "home",
      "name": "Home",
      "script": "",
      "objects": [
        {
          "id": "title",
          "type": "text",
          "name": "title",
          "text": "ScriptDeck Home",
          "x": 32,
          "y": 32,
          "width": 560,
          "height": 40
        },
        {
          "id": "open",
          "type": "button",
          "name": "open",
          "text": "Open Deck",
          "x": 32,
          "y": 100,
          "width": 180,
          "height": 36,
          "script": "function mouseUp() { const path = app.openFileDialog({title: \"Open Deck\", filters: [{name: \"ScriptDeck\", pattern: \"*.deck\"}]}); if (path !== null) app.changeDeck(path); }"
        }
      ]
    }
  ]
}
)HOMEDECK";
#endif
}
inline std::filesystem::path EnsureHomeDeck(const std::filesystem::path& path=HomeDeckPath())
{
    if(!std::filesystem::exists(path)) {
        Stack initial;
        if(!initial.FromJson(EmbeddedHomeDeck()))throw std::runtime_error("Invalid embedded home.deck: "+initial.LastError());
        std::filesystem::create_directories(path.parent_path());
        if(!initial.Save(path))throw std::runtime_error("Cannot create home.deck: "+initial.LastError());
    }
    return path;
}
