#pragma once
#include "Card.h"
#include <filesystem>
#include <string>
#include <vector>

class Stack
{
public:
    int version = 1;
    std::string name = "Untitled";
    int width = 640, height = 480;
    std::string script;
    std::string currentCardId;
    std::string startupPlacement = "default";
    std::vector<Card> cards;

    void CreateNew(const std::string& stackName = "Untitled");
    Card& AddCard(const std::string& cardName);
    CardObject& AddObject(const std::string& cardId, ObjectType type,
                          const std::string& objectName);
    Card* FindCard(const std::string& cardId);
    const Card* FindCard(const std::string& cardId) const;
    bool Load(const std::filesystem::path& path);
    bool Save(const std::filesystem::path& path);
    bool FromJson(const std::string& jsonText);
    std::string ToJson() const;
    const std::string& LastError() const { return lastError_; }
private:
    std::string lastError_;
    void Validate() const;
};
