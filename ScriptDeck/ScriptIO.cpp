#include "ScriptIO.h"
#include "Utf8Path.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
void ValidateUtf8(const std::string& text)
{
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i++]);
        if (c < 0x80) continue;
        int count = 0; std::uint32_t value = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { count=1; value=c&31; minimum=0x80; }
        else if (c >= 0xe0 && c <= 0xef) { count=2; value=c&15; minimum=0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { count=3; value=c&7; minimum=0x10000; }
        else throw std::runtime_error("Text is not valid UTF-8. Use readBytes for other encodings.");
        for (int n=0; n<count; ++n) {
            if (i >= text.size()) throw std::runtime_error("Incomplete UTF-8 text.");
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) throw std::runtime_error("Invalid UTF-8 continuation.");
            value = (value<<6) | (next&63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::runtime_error("Invalid UTF-8 code point.");
    }
}
bool EntryExists(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) return false;
    if (ec) throw std::filesystem::filesystem_error("Cannot inspect path",path,ec);
    return std::filesystem::exists(status);
}
void RenameNoReplace(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    if (EntryExists(destination)) throw std::runtime_error("Destination already exists.");
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), 0))
        throw std::filesystem::filesystem_error("Cannot move path", source, destination,
            std::error_code(static_cast<int>(GetLastError()), std::system_category()));
#else
    std::filesystem::rename(source,destination);
#endif
}
std::filesystem::path Normalized(const std::filesystem::path& path)
{
    auto result = std::filesystem::weakly_canonical(path);
#ifdef _WIN32
    auto text = result.native();
    CharLowerBuffW(text.data(),static_cast<DWORD>(text.size()));
    result = std::filesystem::path(text);
#endif
    return result;
}
}
ScriptFiles::ScriptFiles(std::filesystem::path directory)
    : workingDirectory_(directory.empty() ? std::filesystem::current_path() : std::filesystem::absolute(directory)) {}
std::filesystem::path ScriptFiles::Resolve(const std::string& path) const
{
    if (path.empty() || path.find('\0') != std::string::npos) throw std::runtime_error("Path must be nonempty and contain no NUL.");
    auto result = PathFromUtf8(path);
#ifdef _WIN32
    if (result.has_root_name() && !result.has_root_directory())
        throw std::runtime_error("Drive-relative paths are unsupported. Use an absolute path such as C:/folder/file.");
#endif
    if (result.is_relative()) result = workingDirectory_ / result;
    return result.lexically_normal();
}
ScriptBytes ScriptFiles::ReadBytes(const std::string& path) const
{
    const auto source = Resolve(path);
    if (!std::filesystem::is_regular_file(source)) throw std::runtime_error("Source is not a file.");
    std::ifstream f(source,std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file for reading.");
    ScriptBytes bytes; std::uint8_t chunk[8192];
    while (f) {
        f.read(reinterpret_cast<char*>(chunk),sizeof(chunk));
        const auto size = static_cast<std::size_t>(f.gcount());
        if (size > MaxReadSize-bytes.size()) throw std::runtime_error("Read exceeds 32 MiB per call.");
        bytes.insert(bytes.end(),chunk,chunk+size);
    }
    if (!f.eof()) throw std::runtime_error("Cannot read file.");
    return bytes;
}
std::string ScriptFiles::ReadText(const std::string& path) const
{
    const auto bytes = ReadBytes(path);
    std::size_t start = bytes.size() >= 3 && bytes[0]==0xef && bytes[1]==0xbb && bytes[2]==0xbf ? 3 : 0;
    const std::string text(bytes.begin()+static_cast<std::ptrdiff_t>(start),bytes.end());
    ValidateUtf8(text); return text;
}
void ScriptFiles::WriteBytes(const std::string& path,const ScriptBytes& bytes) const
{
    std::ofstream f(Resolve(path),std::ios::binary|std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot open file for writing.");
    if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    f.close(); if (!f) throw std::runtime_error("Cannot write/close file.");
}
void ScriptFiles::WriteText(const std::string& path,const std::string& text,bool bom,bool append) const
{
    ValidateUtf8(text);
    const auto target = Resolve(path);
    const bool existingContent = append && EntryExists(target) && std::filesystem::file_size(target)>0;
    std::ofstream f(target,std::ios::binary|(append ? std::ios::app : std::ios::trunc));
    if (!f) throw std::runtime_error("Cannot open text file for writing.");
    if (bom && !existingContent) f.write("\xef\xbb\xbf",3);
    f.write(text.data(),static_cast<std::streamsize>(text.size()));
    f.close(); if (!f) throw std::runtime_error("Cannot write/close text file.");
}
std::vector<std::string> ScriptFiles::List(const std::string& path,bool directories) const
{
    std::vector<std::string> result;
    for (const auto& entry : std::filesystem::directory_iterator(Resolve(path))) {
        if (directories ? entry.is_directory() : entry.is_regular_file()) result.push_back(PathToUtf8(entry.path()));
    }
    std::sort(result.begin(),result.end(),[](const std::string& a,const std::string& b){
        return PathToUtf8(PathFromUtf8(a).filename()) < PathToUtf8(PathFromUtf8(b).filename());
    });
    return result;
}
bool ScriptFiles::Exists(const std::string& path,int kind) const
{
    const auto target=Resolve(path); std::error_code ec;
    const auto status=std::filesystem::status(target,ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) return false;
    if(ec) throw std::filesystem::filesystem_error("Cannot inspect path",target,ec);
    if (kind==1) return std::filesystem::is_regular_file(status);
    if (kind==2) return std::filesystem::is_directory(status);
    return std::filesystem::exists(status);
}
void ScriptFiles::Move(const std::string& source,const std::string& destination) const
{
    const auto from=Resolve(source), to=Resolve(destination);
    if (!EntryExists(from)) throw std::runtime_error("Source does not exist.");
    if (EntryExists(to)) throw std::runtime_error("Destination already exists.");
    if (std::filesystem::is_directory(from) && !std::filesystem::is_symlink(from)) {
        const auto a=Normalized(from), b=Normalized(to);
        auto ai=a.begin(),bi=b.begin();for(;ai!=a.end()&&bi!=b.end()&&*ai==*bi;++ai,++bi){}
        if(ai==a.end()) throw std::runtime_error("Cannot move a directory into itself.");
    }
    try { RenameNoReplace(from,to); return; }
    catch(const std::filesystem::filesystem_error& error) {
#ifdef _WIN32
        if(error.code().value()!=ERROR_NOT_SAME_DEVICE) throw;
#else
        if(error.code()!=std::errc::cross_device_link) throw;
#endif
    }
    // 別ドライブではコピー成功後に移動元を削除する。
    auto staging=to;staging += ".scriptdeck-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp";
    if(EntryExists(staging)) throw std::runtime_error("Temporary destination already exists.");
    try {
        std::filesystem::copy(from,staging,std::filesystem::copy_options::recursive|std::filesystem::copy_options::copy_symlinks);
        RenameNoReplace(staging,to);
    } catch(...) {std::error_code ignored;std::filesystem::remove_all(staging,ignored);throw;}
    std::filesystem::remove_all(from);
}
void ScriptFiles::Rename(const std::string& path,const std::string& newName) const
{
    if(newName.empty()||newName=="."||newName==".."||newName.find_first_of("/\\:")!=std::string::npos||newName.find('\0')!=std::string::npos)
        throw std::runtime_error("newName must be a filename, without a directory.");
    Move(path,PathToUtf8(Resolve(path).parent_path()/PathFromUtf8(newName)));
}
void ScriptFiles::Delete(const std::string& path,bool recursive) const
{
    const auto target=Resolve(path);
    if(!EntryExists(target)) throw std::runtime_error("Path does not exist.");
    if(recursive) std::filesystem::remove_all(target);
    else if(!std::filesystem::remove(target)) throw std::runtime_error("Cannot delete path.");
}
