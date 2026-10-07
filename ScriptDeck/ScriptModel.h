#pragma once
#include "Stack.h"
#include "nlohmann/json.hpp"
#include <functional>
#include <cstdint>
#include <map>
#include <set>

// JSの参照はIDで解決し、ベクター内要素のポインターを保持しない。
class ScriptModel
{
public:
    ScriptModel(std::function<Stack&()> deck, std::function<std::uint64_t()> generation,
                std::function<void()> changed);
    nlohmann::json Access(const std::string& operation, const std::string& kind,
        const std::string& cardId, const std::string& objectId, const std::string& property,
        const nlohmann::json& value = nullptr);
private:
    std::function<Stack&()> deck_;
    std::function<std::uint64_t()> generation_;
    std::function<void()> changed_;
    std::uint64_t capturedGeneration_;
    std::map<std::string, std::set<std::string>> usedIds_;
    std::uint64_t nextId_ = 1;
};
