#include "Stack.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using nlohmann::json;
namespace {
void ValidateColor(const DeckColor& color)
{
    for (float component : color)
        if (!std::isfinite(component) || component < 0 || component > 1)
            throw std::runtime_error("Color components must be between 0 and 1.");
}
DeckColor ReadColor(const json& value)
{
    if (!value.is_array() || value.size() != 4)
        throw std::runtime_error("Color must be an RGBA array of four numbers.");
    DeckColor color{};
    for (std::size_t i = 0; i < 4; ++i) {
        if (!value[i].is_number()) throw std::runtime_error("Color components must be numbers.");
        const double n = value[i].get<double>();
        if (!std::isfinite(n) || n < 0 || n > 1) throw std::runtime_error("Invalid color component.");
        color[i] = static_cast<float>(n);
    }
    return color;
}
void ReadOptionalColor(const json& object, const char* key, std::optional<DeckColor>& color)
{
    if (object.contains(key) && !object.at(key).is_null()) color = ReadColor(object.at(key));
}
json ColorJson(const std::optional<DeckColor>& color)
{
    return color ? json(*color) : json(nullptr);
}
}

std::string ObjectTypeName(ObjectType type)
{
    switch (type) {
    case ObjectType::Button: return "button";
    case ObjectType::Field: return "field";
    case ObjectType::TextEditor: return "texteditor";
    case ObjectType::Text: return "text";
    case ObjectType::Image: return "image";
    case ObjectType::Listbox: return "listbox";
    case ObjectType::DropdownList: return "dropdownlist";
    case ObjectType::InputBox: return "inputbox";
    case ObjectType::Checkbox: return "checkbox";
    case ObjectType::RadioButton: return "radiobutton";
    }
    throw std::runtime_error("Invalid object type.");
}
ObjectType ParseObjectType(const std::string& name)
{
    if (name == "button") return ObjectType::Button;
    if (name == "field") return ObjectType::Field;
    if (name == "texteditor") return ObjectType::TextEditor;
    if (name == "text") return ObjectType::Text;
    if (name == "image") return ObjectType::Image;
    if (name == "listbox") return ObjectType::Listbox;
    if (name == "dropdownlist") return ObjectType::DropdownList;
    if (name == "inputbox") return ObjectType::InputBox;
    if (name == "checkbox") return ObjectType::Checkbox;
    if (name == "radiobutton") return ObjectType::RadioButton;
    throw std::runtime_error("Unknown object type: " + name);
}
CardObject* Card::FindObject(const std::string& objectId)
{
    for (auto& o : objects) if (o.id == objectId) return &o;
    return nullptr;
}
const CardObject* Card::FindObject(const std::string& objectId) const
{
    for (const auto& o : objects) if (o.id == objectId) return &o;
    return nullptr;
}
bool Card::SetChecked(const std::string& objectId, bool value)
{
    auto* target=FindObject(objectId);
    if(!target || (target->type!=ObjectType::Checkbox && target->type!=ObjectType::RadioButton))
        throw std::invalid_argument("checked requires a Checkbox or RadioButton.");
    bool changed=target->checked!=value;target->checked=value;
    if(value && target->type==ObjectType::RadioButton)
        for(auto& peer:objects)if(peer.id!=objectId && peer.type==ObjectType::RadioButton && peer.group==target->group && peer.checked){peer.checked=false;changed=true;}
    return changed;
}
bool Card::SetRadioGroup(const std::string& objectId, const std::string& value)
{
    auto* target=FindObject(objectId);
    if(!target || target->type!=ObjectType::RadioButton)throw std::invalid_argument("group requires a RadioButton.");
    bool changed=target->group!=value;target->group=value;
    return SetChecked(objectId,target->checked)||changed;
}
Card* Stack::FindCard(const std::string& id)
{
    for (auto& c : cards) if (c.id == id) return &c;
    return nullptr;
}
const Card* Stack::FindCard(const std::string& id) const
{
    for (const auto& c : cards) if (c.id == id) return &c;
    return nullptr;
}
void Stack::CreateNew(const std::string& stackName)
{
    *this = Stack{};
    name = stackName;
    script = DefaultDeckScript();
    AddCard("Card 1");
}
Card& Stack::AddCard(const std::string& cardName)
{
    std::size_t n = 1;
    while (FindCard("card" + std::to_string(n))) ++n;
    Card c;
    c.id = "card" + std::to_string(n);
    c.name = cardName;
    c.script = DefaultCardScript();
    cards.push_back(std::move(c));
    if (currentCardId.empty()) currentCardId = cards.back().id;
    return cards.back();
}
CardObject& Stack::AddObject(const std::string& cardId, ObjectType type,
                            const std::string& objectName)
{
    Card* c = FindCard(cardId);
    if (!c) throw std::runtime_error("Card not found: " + cardId);
    std::size_t n = 1;
    while (c->FindObject("object" + std::to_string(n))) ++n;
    CardObject o;
    o.id = "object" + std::to_string(n);
    o.type = type;
    o.script = DefaultObjectScript(type);
    o.name = objectName;
    o.text = objectName;
    if (type == ObjectType::Listbox || type == ObjectType::DropdownList) {
        o.items = {"項目1", "項目2", "項目3"}; o.selectedIndex = 0; o.text = o.items[0];
        o.width = 180; o.height = type == ObjectType::Listbox ? 120 : 32;
    }
    if (type == ObjectType::InputBox) { o.width = 180; o.text.clear(); }
    if (type == ObjectType::TextEditor) { o.width = 400; o.height = 240; o.text.clear(); }
    c->objects.push_back(std::move(o));
    return c->objects.back();
}
void Stack::Validate() const
{
    if (startupPlacement != "default" && startupPlacement != "previous" && startupPlacement != "previous_default" &&
        startupPlacement != "previous_center" && startupPlacement != "center")
        throw std::runtime_error("Invalid startup placement.");
    if (version != 1) throw std::runtime_error("Unsupported deck version.");
    if (width <= 0 || height <= 0) throw std::runtime_error("Invalid stack size.");
    if (cards.empty()) throw std::runtime_error("A stack needs at least one card.");
    std::set<std::string> cardIds;
    for (const auto& c : cards) {
        ValidateColor(c.backgroundColor);
        if (c.id.empty() || !cardIds.insert(c.id).second)
            throw std::runtime_error("Empty or duplicate card id: " + c.id);
        for (const auto* target : {&c.enterButtonId, &c.escapeButtonId}) {
            if (!target->empty()) {
                const auto* button = c.FindObject(*target);
                if (!button || button->type != ObjectType::Button)
                    throw std::runtime_error("Invalid card keyboard button: " + *target);
            }
        }
        std::set<std::string> objectIds, checkedGroups;
        for (const auto& o : c.objects) {
            if (o.id.empty() || !objectIds.insert(o.id).second)
                throw std::runtime_error("Empty or duplicate object id in " + c.id);
            ObjectTypeName(o.type);
            if(o.type==ObjectType::RadioButton && o.checked && !checkedGroups.insert(o.group).second)
                throw std::runtime_error("More than one checked RadioButton in group: "+o.group);
            for (const auto* color : {&o.backgroundColor, &o.textColor, &o.borderColor, &o.tintColor})
                if (*color) ValidateColor(**color);
            if (o.imageSource != "file" && o.imageSource != "resource")
                throw std::runtime_error("Invalid image source.");
            if (o.selectedIndex < -1 || (o.selectedIndex >= 0 && static_cast<std::size_t>(o.selectedIndex) >= o.items.size()))
                throw std::runtime_error("Invalid selected item index.");
            if (o.type == ObjectType::InputBox && o.text.find_first_of("\r\n") != std::string::npos)
                throw std::runtime_error("InputBox text must be one line.");
            if (!std::isfinite(o.x) || !std::isfinite(o.y) ||
                !std::isfinite(o.width) || !std::isfinite(o.height) ||
                o.width <= 0 || o.height <= 0)
                throw std::runtime_error("Invalid object rectangle: " + o.id);
        }
    }
    if (!FindCard(currentCardId)) throw std::runtime_error("Current card does not exist.");
}
std::string Stack::ToJson() const
{
    Validate();
    json j = {{"format", "ScriptDeck"}, {"version", version}, {"name", name},
              {"width", width}, {"height", height}, {"script", script},
              {"startupPlacement", startupPlacement}, {"currentCardId", currentCardId}, {"cards", json::array()}};
    for (const auto& c : cards) {
        json jc = {{"id", c.id}, {"name", c.name}, {"script", c.script},
                   {"enterButtonId", c.enterButtonId}, {"escapeButtonId", c.escapeButtonId},
                   {"backgroundColor", c.backgroundColor},
                   {"objects", json::array()}};
        for (const auto& o : c.objects)
            jc["objects"].push_back({{"id", o.id}, {"type", ObjectTypeName(o.type)},
                {"name", o.name}, {"text", o.text}, {"x", o.x}, {"y", o.y},
                {"width", o.width}, {"height", o.height}, {"visible", o.visible},
                {"enabled", o.enabled}, {"checked", o.checked}, {"group", o.group}, {"imagePath", o.imagePath}, {"imageSource", o.imageSource},
                {"backgroundColor", ColorJson(o.backgroundColor)}, {"textColor", ColorJson(o.textColor)},
                {"borderColor", ColorJson(o.borderColor)}, {"tintColor", ColorJson(o.tintColor)},
                {"resourceId", o.resourceId}, {"items", o.items}, {"selectedIndex", o.selectedIndex}, {"script", o.script}});
        j["cards"].push_back(std::move(jc));
    }
    return j.dump(2) + "\n";
}
bool Stack::FromJson(const std::string& text)
{
    try {
        const json j = json::parse(text);
        if (j.at("format").get<std::string>() != "ScriptDeck")
            throw std::runtime_error("Not a ScriptDeck file.");
        for (const char* key : {"version", "width", "height"}) {
            if (!j.at(key).is_number_integer())
                throw std::runtime_error(std::string(key) + " must be an integer.");
            const auto& v = j.at(key);
            if ((v.is_number_unsigned() && v.get<std::uint64_t>() > 2147483647ULL) ||
                (!v.is_number_unsigned() &&
                 (v.get<std::int64_t>() < -2147483647LL - 1 || v.get<std::int64_t>() > 2147483647LL)))
                throw std::runtime_error(std::string(key) + " is out of range.");
        }
        Stack s;
        s.version = j.at("version").get<int>();
        s.name = j.at("name").get<std::string>();
        s.width = j.at("width").get<int>();
        s.height = j.at("height").get<int>();
        s.script = j.value("script", std::string{});
        s.startupPlacement = j.value("startupPlacement", std::string("default"));
        // 旧版の「前回」設定はデフォルト位置へのフォールバックとして引き継ぐ。
        if (s.startupPlacement == "previous") s.startupPlacement = "previous_default";
        s.currentCardId = j.at("currentCardId").get<std::string>();
        if (!j.at("cards").is_array()) throw std::runtime_error("cards must be an array.");
        for (const auto& jc : j.at("cards")) {
            Card c;
            c.id = jc.at("id").get<std::string>();
            c.name = jc.at("name").get<std::string>();
            c.script = jc.value("script", std::string{});
            if (jc.contains("backgroundColor")) c.backgroundColor = ReadColor(jc.at("backgroundColor"));
            c.enterButtonId = jc.value("enterButtonId", std::string{});
            c.escapeButtonId = jc.value("escapeButtonId", std::string{});
            if (!jc.at("objects").is_array()) throw std::runtime_error("objects must be an array.");
            for (const auto& jo : jc.at("objects")) {
                CardObject o;
                o.id = jo.at("id").get<std::string>();
                o.type = ParseObjectType(jo.at("type").get<std::string>());
                o.name = jo.at("name").get<std::string>();
                o.text = jo.value("text", std::string{});
                o.x = jo.at("x").get<double>(); o.y = jo.at("y").get<double>();
                o.width = jo.at("width").get<double>(); o.height = jo.at("height").get<double>();
                o.visible = jo.value("visible", true); o.enabled = jo.value("enabled", true);
                o.checked = jo.value("checked", false); o.group = jo.value("group", std::string{});
                o.imagePath = jo.value("imagePath", std::string{});
                o.imageSource = jo.value("imageSource", std::string("file"));
                o.resourceId = jo.value("resourceId", std::string{});
                if (jo.contains("items")) o.items = jo.at("items").get<std::vector<std::string>>();
                if (jo.contains("selectedIndex")) {
                    const auto& index = jo.at("selectedIndex");
                    if (!index.is_number_integer() || (index.is_number_unsigned() && index.get<std::uint64_t>() > 2147483647ULL))
                        throw std::runtime_error("Invalid selected item index.");
                    const auto n = index.get<std::int64_t>();
                    if (n < -1 || n > 2147483647LL) throw std::runtime_error("Invalid selected item index.");
                    o.selectedIndex = static_cast<int>(n);
                }
                ReadOptionalColor(jo, "backgroundColor", o.backgroundColor);
                ReadOptionalColor(jo, "textColor", o.textColor);
                ReadOptionalColor(jo, "borderColor", o.borderColor);
                ReadOptionalColor(jo, "tintColor", o.tintColor);
                o.script = jo.value("script", std::string{});
                c.objects.push_back(std::move(o));
            }
            s.cards.push_back(std::move(c));
        }
        s.Validate();
        *this = std::move(s);
        return true;
    } catch (const std::exception& e) { lastError_ = e.what(); return false; }
}
bool Stack::Load(const std::filesystem::path& path)
{
    try {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open deck file.");
        std::string text((std::istreambuf_iterator<char>(f)), {});
        if (f.bad()) throw std::runtime_error("Cannot read deck file.");
        return FromJson(text);
    } catch (const std::exception& e) { lastError_ = e.what(); return false; }
}
bool Stack::Save(const std::filesystem::path& path)
{
    std::filesystem::path temp;
    try {
        const std::string text = ToJson();
        if (path.empty()) throw std::runtime_error("Output path is empty.");
        // 同じフォルダに一時ファイルを作り、書き込み成功後に置換する。
        // 同時に同一ファイルを保存する用途はこの雛形では扱わない。
        temp = path; temp += ".tmp";
        // 復旧用の一時ファイルを保持し、再試行には未使用の名前を使う。
        for (std::uint64_t attempt = 1; std::filesystem::exists(temp); ++attempt) {
            temp = path;
            temp += ".tmp." + std::to_string(attempt);
        }
        {
            std::ofstream f(temp, std::ios::binary | std::ios::trunc);
            if (!f) { temp.clear(); throw std::runtime_error("Cannot create temporary file."); }
            f.write(text.data(), static_cast<std::streamsize>(text.size()));
            f.flush();
            if (!f) throw std::runtime_error("Cannot write deck file.");
            f.close();
            if (!f) throw std::runtime_error("Cannot close deck file.");
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace deck file; Windows error " + std::to_string(GetLastError()));
#else
        std::filesystem::rename(temp, path);
#endif
        lastError_.clear();
        return true;
    } catch (const std::exception& e) {
        lastError_ = e.what();
        return false; // 失敗時の一時ファイルは復旧用に残す。
    }
}
