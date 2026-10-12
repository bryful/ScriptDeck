#pragma once
#include "Utf8Path.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// ZIP method 0 (STORE), UTF-8 names, classic single-disk ZIP. No ZIP64/encryption.
namespace DeckZip {
using Files=std::map<std::string,std::string>;
inline constexpr std::size_t MaxBytes=512u*1024u*1024u;
inline bool IsDeckx(const std::filesystem::path& path) {
    auto extension=path.extension().wstring();
    std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
    return extension==L".deckx";
}
inline std::uint32_t Crc(const std::string& bytes) {
    static const auto table=[] {
        std::vector<std::uint32_t> result(256);
        for(std::uint32_t i=0;i<256;++i){auto n=i;for(int k=0;k<8;++k)n=(n>>1)^((n&1)?0xedb88320u:0u);result[i]=n;}return result;
    }();
    std::uint32_t crc=0xffffffffu;
    for(unsigned char byte:bytes)crc=(crc>>8)^table[(crc^byte)&255];return crc^0xffffffffu;
}
inline std::wstring NameKey(const std::string& name) {
    auto key=PathFromUtf8(name).generic_wstring();
    std::transform(key.begin(),key.end(),key.begin(),::towlower);return key;
}
inline void CheckName(const std::string& name) {
    if(name.empty()||name.size()>65535||name.front()=='/'||name.find('\\')!=std::string::npos||name.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid deckx entry path.");
    auto path=PathFromUtf8(name);
    if(path.is_absolute()||path.has_root_name())throw std::runtime_error("Absolute deckx entry path.");
    for(const auto& part:path) {
        auto text=PathToUtf8(part);
        if(text=="."||text==".."||text.empty()||text.find_first_of(":*?\"<>|")!=std::string::npos||text.back()=='.'||text.back()==' ')
            throw std::runtime_error("Unsafe deckx entry path: "+name);
        auto base=NameKey(text.substr(0,text.find('.')));
        if(base==L"con"||base==L"prn"||base==L"aux"||base==L"nul"||
           (base.size()==4&&(base.substr(0,3)==L"com"||base.substr(0,3)==L"lpt")&&base[3]>=L'1'&&base[3]<=L'9'))
            throw std::runtime_error("Reserved deckx filename: "+name);
    }
}
inline std::string ReadFile(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    if(size>MaxBytes)throw std::runtime_error("deckx file exceeds 512 MiB limit.");
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read: "+PathToUtf8(path));
    std::string data(static_cast<std::size_t>(size),'\0');in.read(data.data(),static_cast<std::streamsize>(data.size()));
    if(!in)throw std::runtime_error("Cannot read: "+PathToUtf8(path));return data;
}
inline void Put(std::string& bytes,std::uint32_t n,int count) {for(int i=0;i<count;++i){bytes.push_back(static_cast<char>(n&255));n>>=8;}}
inline std::uint32_t Get(const std::string& bytes,std::size_t at,int count) {
    if(at>bytes.size()||static_cast<std::size_t>(count)>bytes.size()-at)throw std::runtime_error("Truncated deckx ZIP.");
    std::uint32_t n=0;for(int i=0;i<count;++i)n|=std::uint32_t(static_cast<unsigned char>(bytes[at+i]))<<(8*i);return n;
}
inline std::string Encode(const Files& files) {
    if(files.size()>65534)throw std::runtime_error("Too many deckx files.");
    std::string result,central;std::set<std::wstring> names;
    for(const auto& entry:files) {
        CheckName(entry.first);
        if(!names.insert(NameKey(entry.first)).second)throw std::runtime_error("Duplicate deckx entry.");
        if(entry.second.size()>MaxBytes||result.size()+entry.second.size()+central.size()+entry.first.size()*2+128>MaxBytes)
            throw std::runtime_error("deckx exceeds 512 MiB limit.");
        auto offset=static_cast<std::uint32_t>(result.size()),size=static_cast<std::uint32_t>(entry.second.size()),crc=Crc(entry.second);
        Put(result,0x04034b50,4);Put(result,20,2);Put(result,0x800,2);Put(result,0,2);Put(result,0,2);Put(result,33,2);
        Put(result,crc,4);Put(result,size,4);Put(result,size,4);Put(result,static_cast<std::uint32_t>(entry.first.size()),2);Put(result,0,2);
        result+=entry.first;result+=entry.second;
        Put(central,0x02014b50,4);Put(central,20,2);Put(central,20,2);Put(central,0x800,2);Put(central,0,2);Put(central,0,2);Put(central,33,2);
        Put(central,crc,4);Put(central,size,4);Put(central,size,4);Put(central,static_cast<std::uint32_t>(entry.first.size()),2);
        Put(central,0,2);Put(central,0,2);Put(central,0,2);Put(central,0,2);Put(central,0,4);Put(central,offset,4);central+=entry.first;
    }
    auto offset=static_cast<std::uint32_t>(result.size());result+=central;
    Put(result,0x06054b50,4);Put(result,0,2);Put(result,0,2);Put(result,static_cast<std::uint32_t>(files.size()),2);Put(result,static_cast<std::uint32_t>(files.size()),2);
    Put(result,static_cast<std::uint32_t>(central.size()),4);Put(result,offset,4);Put(result,0,2);return result;
}
inline Files Decode(const std::string& bytes) {
    if(bytes.size()<22||bytes.size()>MaxBytes)throw std::runtime_error("Invalid deckx ZIP size.");
    std::size_t end=bytes.size()-22;
    for(;;) {
        if(Get(bytes,end,4)==0x06054b50 && end+22+Get(bytes,end+20,2)==bytes.size())break;
        if(end==0||bytes.size()-end>65557)throw std::runtime_error("Missing deckx ZIP directory.");--end;
    }
    auto count=Get(bytes,end+10,2);std::size_t size=Get(bytes,end+12,4),position=Get(bytes,end+16,4);
    if(Get(bytes,end+4,2)||Get(bytes,end+6,2)||Get(bytes,end+8,2)!=count||count==65535||position>end||size!=end-position)
        throw std::runtime_error("Unsupported deckx ZIP layout.");
    Files result;std::set<std::wstring> names;std::vector<std::pair<std::size_t,std::size_t>> ranges;
    const auto central=position;
    for(std::uint32_t i=0;i<count;++i) {
        if(position>end||end-position<46||Get(bytes,position,4)!=0x02014b50)throw std::runtime_error("Invalid deckx directory entry.");
        auto flags=Get(bytes,position+8,2),method=Get(bytes,position+10,2),crc=Get(bytes,position+16,4),packed=Get(bytes,position+20,4),length=Get(bytes,position+24,4);
        auto nameLength=Get(bytes,position+28,2),extra=Get(bytes,position+30,2),comment=Get(bytes,position+32,2);
        std::size_t local=Get(bytes,position+42,4),next=position+46+nameLength+extra+comment;
        if(next>end||method!=0||(flags&~0x808u)||packed!=length||Get(bytes,position+34,2))throw std::runtime_error("deckx requires uncompressed, unencrypted ZIP entries.");
        auto name=bytes.substr(position+46,nameLength);bool directory=!name.empty()&&name.back()=='/';
        if(directory)name.pop_back();CheckName(name);
        if(!names.insert(NameKey(name)).second)throw std::runtime_error("Duplicate deckx path.");
        if(local>central||central-local<30||Get(bytes,local,4)!=0x04034b50||Get(bytes,local+6,2)!=flags||Get(bytes,local+8,2)!=method)
            throw std::runtime_error("Invalid deckx local header.");
        auto localName=Get(bytes,local+26,2),localExtra=Get(bytes,local+28,2);
        std::size_t data=local+30+localName+localExtra;
        if(data>central||length>central-data||bytes.substr(local+30,localName)!=(directory?name+"/":name))throw std::runtime_error("Invalid deckx file extent.");
        if(!(flags&8) && (Get(bytes,local+14,4)!=crc||Get(bytes,local+18,4)!=length||Get(bytes,local+22,4)!=length))throw std::runtime_error("Inconsistent deckx header.");
        ranges.push_back({local,data+length});auto content=bytes.substr(data,length);
        if(Crc(content)!=crc)throw std::runtime_error("deckx CRC mismatch: "+name);
        if(directory){if(length)throw std::runtime_error("Nonempty deckx directory.");}else result.emplace(name,std::move(content));
        position=next;
    }
    if(position!=end)throw std::runtime_error("Invalid deckx directory size.");
    std::sort(ranges.begin(),ranges.end());
    for(std::size_t i=1;i<ranges.size();++i)if(ranges[i].first<ranges[i-1].second)throw std::runtime_error("Overlapping deckx entries.");
    // Reject a file used as another entry's parent before extracting anything.
    std::set<std::wstring> fileNames;
    for(const auto& entry:result)fileNames.insert(NameKey(entry.first));
    for(const auto& entry:result)for(auto parent=PathFromUtf8(entry.first).parent_path();!parent.empty();parent=parent.parent_path())
        if(fileNames.count(NameKey(PathToUtf8(parent))))throw std::runtime_error("Conflicting deckx paths.");
    return result;
}
inline void WriteAtomic(const std::filesystem::path& path,const std::string& bytes,bool replace) {
    if(!replace && std::filesystem::exists(path))throw std::runtime_error("Export destination already exists.");
    auto temp=path;temp+=".tmp";
    for(std::uint64_t n=1;std::filesystem::exists(temp);++n){temp=path;temp+=".tmp."+std::to_string(n);}
    std::ofstream out(temp,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();
    if(!out)throw std::runtime_error("Cannot write deckx temporary file: "+PathToUtf8(temp));
#ifdef _WIN32
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0)))throw std::runtime_error("Cannot replace deckx; recovery file: "+PathToUtf8(temp));
#else
    if(!replace&&std::filesystem::exists(path))throw std::runtime_error("Export destination already exists.");
    std::filesystem::rename(temp,path);
#endif
}
inline Files Collect(const std::filesystem::path& root) {
    Files result;std::size_t total=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
        if(entry.is_symlink())throw std::runtime_error("Links are not supported inside deckx.");
        if(entry.is_regular_file()) {
            auto name=PathToUtf8(entry.path().lexically_relative(root));
            std::replace(name.begin(),name.end(),char(92),char(47));CheckName(name);
            auto bytes=ReadFile(entry.path());total+=bytes.size();if(total>MaxBytes)throw std::runtime_error("deckx exceeds 512 MiB limit.");
            result.emplace(name,std::move(bytes));
        }
    }
    return result;
}
struct Workspace {
    std::filesystem::path root;
    Workspace() {
        static std::atomic<unsigned long long> sequence{0};
        for(int attempt=0;attempt<100;++attempt) {
            root=std::filesystem::temp_directory_path()/("scriptdeck-deckx-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++));
            if(std::filesystem::create_directory(root))return;
        }
        throw std::runtime_error("Cannot create deckx workspace.");
    }
    ~Workspace(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}
    void Extract(const Files& files) {
        for(const auto& entry:files){CheckName(entry.first);auto path=root/PathFromUtf8(entry.first);std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path,std::ios::binary);out.write(entry.second.data(),static_cast<std::streamsize>(entry.second.size()));out.close();
            if(!out)throw std::runtime_error("Cannot extract deckx file.");}
    }
};
}
