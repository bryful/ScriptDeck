#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>

// RGBA、各成分0～1。未指定は従来の表示色を使用する。
using DeckColor = std::array<float, 4>;

enum class ObjectType { Button, Field, Text, Image, Listbox, DropdownList, InputBox, Checkbox, RadioButton };

struct CardObject
{
    std::string id;
    ObjectType type = ObjectType::Button;
    std::string name;
    std::string text;
    double x = 20, y = 20, width = 120, height = 32;
    bool visible = true;
    bool enabled = true;
    bool checked = false;
    std::string group;
    std::string imagePath;
    std::string imageSource = "file";
    std::string resourceId;
    std::vector<std::string> items;
    int selectedIndex = -1;
    std::optional<DeckColor> backgroundColor;
    std::optional<DeckColor> textColor;
    std::optional<DeckColor> borderColor;
    std::optional<DeckColor> tintColor;
    std::string script;
};
std::string ObjectTypeName(ObjectType type);
ObjectType ParseObjectType(const std::string& name);
