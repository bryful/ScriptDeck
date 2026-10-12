#pragma once
#include "Card.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
namespace DeckZip { struct Workspace; }

class Stack
{
public:
    bool deckxMode = false;
    std::filesystem::path archivePath;
    std::shared_ptr<DeckZip::Workspace> archiveWorkspace;
    bool folderMode = false;
    std::filesystem::path folderRoot;
    int version = 1;
    std::string name = "Untitled";
    int width = 640, height = 480;
    std::string script;
    std::string scriptFile;
    std::string currentCardId;
    std::string startupPlacement = "default";
    std::vector<Card> cards;

    void CreateNew(const std::string& stackName = "Untitled");
    Card& AddCard(const std::string& cardName);
    CardObject& AddObject(const std::string& cardId, ObjectType type,
                          const std::string& objectName);
    Card* FindCard(const std::string& cardId);
    const Card* FindCard(const std::string& cardId) const;
    bool Load(const std::filesystem::path& path, bool loadScripts = true);
    void LoadExternalScripts();
    void CreateFolder(const std::filesystem::path& folder);
    void Export(const std::filesystem::path& destination, bool asFolder) const;
    void ExportDeckx(const std::filesystem::path& destination) const;
    void Rename(const std::string& kind, const std::string& cardId, const std::string& objectId, const std::string& newName);
    CardObject& DuplicateObject(const std::string& cardId, const std::string& objectId);
    void EnsureExternalScripts();
    bool Save(const std::filesystem::path& path);
    bool FromJson(const std::string& jsonText);
    std::string ToJson() const;
    const std::string& LastError() const { return lastError_; }
private:
    std::string lastError_;
    void Validate() const;
};
