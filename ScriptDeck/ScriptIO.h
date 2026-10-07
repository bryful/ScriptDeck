#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

using ScriptBytes = std::vector<std::uint8_t>;
class ScriptFiles
{
public:
    static constexpr std::size_t MaxReadSize = 32 * 1024 * 1024;
    explicit ScriptFiles(std::filesystem::path workingDirectory = {});
    std::filesystem::path Resolve(const std::string& path) const;
    ScriptBytes ReadBytes(const std::string& path) const;
    std::string ReadText(const std::string& path) const;
    void WriteBytes(const std::string& path, const ScriptBytes& bytes) const;
    void WriteText(const std::string& path, const std::string& text, bool bom, bool append = false) const;
    std::vector<std::string> List(const std::string& path, bool directories) const;
    bool Exists(const std::string& path, int kind) const;
    void Move(const std::string& source, const std::string& destination) const;
    void Rename(const std::string& path, const std::string& newName) const;
    void Delete(const std::string& path, bool recursive) const;
private:
    std::filesystem::path workingDirectory_;
};
