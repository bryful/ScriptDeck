#pragma once
#include "CardObject.h"
#include <vector>

struct Card
{
    std::string id;
    std::string name;
    std::string script;
    DeckColor backgroundColor = {245.0f/255, 246.0f/255, 250.0f/255, 1};
    std::string enterButtonId;
    std::string escapeButtonId;
    std::vector<CardObject> objects;
    bool SetChecked(const std::string& objectId, bool value);
    bool SetRadioGroup(const std::string& objectId, const std::string& value);
    CardObject* FindObject(const std::string& objectId);
    const CardObject* FindObject(const std::string& objectId) const;
};
