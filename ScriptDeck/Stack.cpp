#include "Stack.h"
#include "DeckArchive.h"
#include "Utf8Path.h"
#include <map>
#include <cctype>
#include <cwctype>
#include <algorithm>
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
std::string SafeScriptName(std::string value) {
    for(auto& c:value)if(static_cast<unsigned char>(c)<32 || std::string("\\/:*?\"<>|").find(c)!=std::string::npos)c='_';
    while(!value.empty() && (value.back()=='.'||value.back()==' '))value.pop_back();
    if(value.empty())throw std::runtime_error("Name cannot be used as a filename.");
    auto upper=value.substr(0,value.find('.'));for(auto& c:upper)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL"||
       (upper.size()==4 && (upper.substr(0,3)=="COM"||upper.substr(0,3)=="LPT") && upper[3]>='1'&&upper[3]<='9'))value="_"+value;
    return value;
}
std::filesystem::path ScriptPath(const std::filesystem::path& root,const std::string& file) {
    auto relative=PathFromUtf8(file);
    if(relative.empty()||relative.is_absolute()||relative.has_root_name())throw std::runtime_error("Invalid script path: "+file);
    for(const auto& part:relative)if(part=="..")throw std::runtime_error("Script path escapes Folder: "+file);
    if(relative.begin()->string()!="script"||relative.extension()!=".js")throw std::runtime_error("Scripts must be inside script/: "+file);
    return root/relative;
}
std::string ReadScript(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read script: "+PathToUtf8(path));
    std::string text((std::istreambuf_iterator<char>(in)),{});if(in.bad())throw std::runtime_error("Cannot read script: "+PathToUtf8(path));
    if(text.compare(0,3,"\xef\xbb\xbf")==0)text.erase(0,3);return text;
}
void CreateScript(const std::filesystem::path& root,const std::string& file,std::string& code) {
    auto path=ScriptPath(root,file);
    if(std::filesystem::exists(path)){code=ReadScript(path);return;}
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);out<<code;out.close();if(!out)throw std::runtime_error("Cannot create script: "+PathToUtf8(path));
}
}
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
    if(folderMode) {
        c.scriptFile="script/"+SafeScriptName(cardName)+".js";
        CreateScript(folderRoot,c.scriptFile,c.script);
    }
    cards.push_back(std::move(c));
    if (currentCardId.empty()) currentCardId = cards.back().id;
    return cards.back();
}
CardObject& Stack::AddObject(const std::string& cardId, ObjectType type,
                            const std::string& objectName)
{
    Card* c = FindCard(cardId);
    if (!c) throw std::runtime_error("Card not found: " + cardId);
    if (std::any_of(c->objects.begin(), c->objects.end(), [&](const auto& object) { return object.name == objectName; }))
        throw std::runtime_error("An object with the same name already exists in this card.");
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
    if(folderMode && HasObjectScriptEvent(type)) {
        o.scriptFile="script/"+SafeScriptName(c->name)+"_"+SafeScriptName(objectName)+".js";
        CreateScript(folderRoot,o.scriptFile,o.script);
    }
    c->objects.push_back(std::move(o));
    return c->objects.back();
}
void Stack::Validate() const
{
    if(folderMode) {
        ScriptPath(folderRoot,scriptFile);
        for(const auto& c:cards){ScriptPath(folderRoot,c.scriptFile);for(const auto& o:c.objects)if(HasObjectScriptEvent(o.type)||!o.scriptFile.empty())ScriptPath(folderRoot,o.scriptFile);}
    } else {
        if(!scriptFile.empty())throw std::runtime_error("External scripts require Folder mode.");
        for(const auto& c:cards){if(!c.scriptFile.empty())throw std::runtime_error("External scripts require Folder mode.");for(const auto& o:c.objects)if(!o.scriptFile.empty())throw std::runtime_error("External scripts require Folder mode.");}
    }
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
    if (folderMode) {
        j["storage"] = "folder";
        auto external = [](json& item, const std::string& file) {
            if(file.empty()) { item.erase("scriptFile"); }
            else { item.erase("script"); item["scriptFile"] = file; }
        };
        external(j, scriptFile);
        for (std::size_t i=0;i<cards.size();++i) {
            external(j["cards"][i],cards[i].scriptFile);
            for(std::size_t k=0;k<cards[i].objects.size();++k)
                external(j["cards"][i]["objects"][k],cards[i].objects[k].scriptFile);
        }
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
        s.folderMode = j.value("storage", std::string{}) == "folder";
        s.scriptFile = j.value("scriptFile", std::string{});
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
            c.scriptFile = jc.value("scriptFile", std::string{});
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
                o.scriptFile = jo.value("scriptFile", std::string{});
                c.objects.push_back(std::move(o));
            }
            s.cards.push_back(std::move(c));
        }
        s.Validate();
        *this = std::move(s);
        return true;
    } catch (const std::exception& e) { lastError_ = e.what(); return false; }
}
bool Stack::Load(const std::filesystem::path& input, bool loadScripts)
{
    try {
        if(DeckZip::IsDeckx(input)) {
            auto files=DeckZip::Decode(DeckZip::ReadFile(input));
            if(!files.count("main.deck"))throw std::runtime_error("deckx must contain main.deck at its root.");
            auto workspace=std::make_shared<DeckZip::Workspace>();workspace->Extract(files);
            Stack candidate;
            if(!candidate.Load(workspace->root/"main.deck",loadScripts))throw std::runtime_error(candidate.LastError());
            if(candidate.folderMode) {
                auto metadata=json::parse(files.at("main.deck"));candidate.name=metadata.at("name").get<std::string>();
            }
            candidate.deckxMode=true;candidate.archivePath=std::filesystem::absolute(input).lexically_normal();
            candidate.archiveWorkspace=std::move(workspace);*this=std::move(candidate);return true;
        }
        const auto path = std::filesystem::is_directory(input) ? input / "main.deck" : input;
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open deck file.");
        std::string text((std::istreambuf_iterator<char>(f)), {});
        if (f.bad()) throw std::runtime_error("Cannot read deck file.");
        Stack candidate;
        if(!candidate.FromJson(text))throw std::runtime_error(candidate.LastError());
        if(candidate.folderMode) {
            if(path.filename() != "main.deck")throw std::runtime_error("Folder decks must use main.deck.");
            candidate.folderRoot=std::filesystem::absolute(path).parent_path().lexically_normal();
            candidate.name=PathToUtf8(candidate.folderRoot.filename());
            if(loadScripts)candidate.LoadExternalScripts();
        } else {
            candidate.folderRoot=std::filesystem::absolute(path).parent_path().lexically_normal();
            if(!candidate.scriptFile.empty())throw std::runtime_error("External scripts require Folder mode.");
            for(const auto& c:candidate.cards) {
                if(!c.scriptFile.empty())throw std::runtime_error("External scripts require Folder mode.");
                for(const auto& o:c.objects)if(!o.scriptFile.empty())throw std::runtime_error("External scripts require Folder mode.");
            }
        }
        *this=std::move(candidate);return true;
    } catch (const std::exception& e) { lastError_ = e.what(); return false; }
}
bool Stack::Save(const std::filesystem::path& path)
{
    std::filesystem::path temp;
    try {
        if(deckxMode) {
            if(!archiveWorkspace || std::filesystem::absolute(path).lexically_normal()!=archivePath)
                throw std::runtime_error("Use Export to change deckx storage or destination.");
            Stack extracted=*this;extracted.deckxMode=false;extracted.archiveWorkspace.reset();extracted.archivePath.clear();
            if(!extracted.Save(folderRoot/"main.deck"))throw std::runtime_error(extracted.LastError());
            DeckZip::WriteAtomic(archivePath,DeckZip::Encode(DeckZip::Collect(folderRoot)),true);
            lastError_.clear();return true;
        }
        if(DeckZip::IsDeckx(path))throw std::runtime_error("Use Export to create a deckx file.");
        if(folderMode && std::filesystem::absolute(path).lexically_normal()!=folderRoot/"main.deck")
            throw std::runtime_error("Use Export to change Folder storage or destination.");
        if(!folderMode && std::filesystem::is_regular_file(path)) {
            Stack existing;
            if(existing.Load(path,false) && existing.folderMode)
                throw std::runtime_error("Cannot overwrite Folder storage with ordinary deck. Use Export.");
        }
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

void Stack::LoadExternalScripts() {
    if(!folderMode)return;
    script=ReadScript(ScriptPath(folderRoot,scriptFile));
    for(auto& c:cards){c.script=ReadScript(ScriptPath(folderRoot,c.scriptFile));
        for(auto& o:c.objects)if(!o.scriptFile.empty())o.script=ReadScript(ScriptPath(folderRoot,o.scriptFile));}
}
void Stack::EnsureExternalScripts() {
    auto fillEmpty = [](std::string& code, const std::string& initial) {
        if(code.find_first_not_of(" \t\r\n\f\v") == std::string::npos) code = initial;
    };
    fillEmpty(script, DefaultDeckScript());
    scriptFile="script/deck.js";CreateScript(folderRoot,scriptFile,script);
    for(auto& c:cards) {
        fillEmpty(c.script, DefaultCardScript());
        c.scriptFile="script/"+SafeScriptName(c.name)+".js";
        CreateScript(folderRoot,c.scriptFile,c.script);
        for(auto& o:c.objects) {
            if(!HasObjectScriptEvent(o.type)){o.scriptFile.clear();continue;}
            fillEmpty(o.script, DefaultObjectScript(o.type));
            o.scriptFile="script/"+SafeScriptName(c.name)+"_"+SafeScriptName(o.name)+".js";
            CreateScript(folderRoot,o.scriptFile,o.script);
        }
    }
}

void Stack::CreateFolder(const std::filesystem::path& folder) {
    if(std::filesystem::exists(folder))throw std::runtime_error("Folder already exists.");
    Stack candidate;candidate.CreateNew(PathToUtf8(folder.filename()));
    candidate.folderMode=true;candidate.folderRoot=std::filesystem::absolute(folder).lexically_normal();
    std::filesystem::create_directories(candidate.folderRoot);
    candidate.EnsureExternalScripts();
    if(!candidate.Save(candidate.folderRoot/"main.deck"))throw std::runtime_error(candidate.LastError());
    *this=std::move(candidate);
}
void Stack::Export(const std::filesystem::path& destination,bool asFolder) const {
    Stack copy=*this;
    copy.deckxMode=false;copy.archivePath.clear();copy.archiveWorkspace.reset();
    if(folderMode)copy.LoadExternalScripts();
    auto target=std::filesystem::absolute(destination).lexically_normal();
    if(std::filesystem::exists(target))throw std::runtime_error("Export destination already exists.");
    // Plan all resources before writing output. Preserve data/ paths used by scripts,
    // and copy external images once even when several objects reference them.
    const auto outputRoot=asFolder?target:target.parent_path();
    auto key=[](const std::filesystem::path& path) {
        auto value=std::filesystem::absolute(path).lexically_normal().wstring();
#ifdef _WIN32
        std::transform(value.begin(),value.end(),value.begin(),::towlower);
#endif
        return value;
    };
    struct Asset {std::filesystem::path source, relative;};
    std::vector<Asset> assets;
    std::map<std::wstring,std::size_t> sources, outputs;
    auto addAsset=[&](const std::filesystem::path& input,std::filesystem::path relative,bool allocateName) {
        auto source=std::filesystem::absolute(input).lexically_normal();
        if(!std::filesystem::is_regular_file(source))throw std::runtime_error("Linked resource is missing or not a file: "+PathToUtf8(source));
        auto existing=sources.find(key(source));
        if(existing!=sources.end())return assets[existing->second].relative;
        const auto original=relative;
        for(std::size_t suffix=2;outputs.count(key(outputRoot/relative));++suffix) {
            if(!allocateName)throw std::runtime_error("Resource destination conflict: "+PathToUtf8(outputRoot/relative));
            relative=original.parent_path()/PathFromUtf8(PathToUtf8(original.stem())+"_"+std::to_string(suffix)+PathToUtf8(original.extension()));
        }
        auto output=outputRoot/relative;
        if(std::filesystem::exists(output) && !std::filesystem::equivalent(source,output))
            throw std::runtime_error("Resource destination already exists: "+PathToUtf8(output));
        auto index=assets.size();sources.emplace(key(source),index);outputs.emplace(key(output),index);
        assets.push_back({source,relative});return relative;
    };
    if(!folderRoot.empty() && std::filesystem::is_directory(folderRoot/"data")) {
        for(const auto& entry:std::filesystem::recursive_directory_iterator(folderRoot/"data")) {
            if(entry.is_regular_file())addAsset(entry.path(),entry.path().lexically_relative(folderRoot),false);
        }
    }
    for(auto& card:copy.cards)for(auto& object:card.objects)
        if(object.imageSource=="file"&&!object.imagePath.empty()) {
            auto source=PathFromUtf8(object.imagePath);
            if(source.is_relative()) {
                if(folderRoot.empty())throw std::runtime_error("Save the source deck before exporting relative resources.");
                source=folderRoot/source;
            }
            object.imagePath=PathToUtf8(addAsset(source,std::filesystem::path("data")/source.filename(),true));
        }
    if(asFolder) {
        copy.folderMode=true;copy.folderRoot=target;copy.name=PathToUtf8(target.filename());
        // Validate duplicate generated paths before creating any files.
        std::set<std::filesystem::path> files;
        auto check=[&](std::string file){auto p=PathFromUtf8(file).wstring();std::transform(p.begin(),p.end(),p.begin(),::towlower);
            if(!files.insert(std::filesystem::path(p)).second)throw std::runtime_error("Duplicate script filename: "+file);};
        check("deck.js");for(const auto& c:copy.cards){check(SafeScriptName(c.name)+".js");for(const auto& o:c.objects)if(HasObjectScriptEvent(o.type))check(SafeScriptName(c.name)+"_"+SafeScriptName(o.name)+".js");}
        std::filesystem::create_directories(target);copy.EnsureExternalScripts();target/="main.deck";
    } else {copy.folderMode=false;copy.folderRoot.clear();copy.scriptFile.clear();for(auto& c:copy.cards){c.scriptFile.clear();for(auto& o:c.objects)o.scriptFile.clear();}}
    for(const auto& asset:assets) {
        const auto output=outputRoot/asset.relative;
        if(std::filesystem::exists(output) && std::filesystem::equivalent(asset.source,output))continue;
        std::filesystem::create_directories(output.parent_path());
        std::filesystem::copy_file(asset.source,output);
    }
    if(!copy.Save(target))throw std::runtime_error(copy.LastError());
}
void Stack::ExportDeckx(const std::filesystem::path& destination) const {
    if(!DeckZip::IsDeckx(destination))throw std::runtime_error("deckx Export requires .deckx extension.");
    if(std::filesystem::exists(destination))throw std::runtime_error("Export destination already exists.");
    DeckZip::Workspace workspace;
    Export(workspace.root/"main.deck",false);
    DeckZip::WriteAtomic(destination,DeckZip::Encode(DeckZip::Collect(workspace.root)),false);
}
void Stack::Rename(const std::string& kind,const std::string& cardId,const std::string& objectId,const std::string& newName) {
    Card* card=FindCard(cardId);CardObject* object=card?card->FindObject(objectId):nullptr;
    std::string* current=kind=="deck"?&name:kind=="card"&&card?&card->name:kind=="object"&&object?&object->name:nullptr;
    if(!current)throw std::runtime_error("Rename target not found.");if(*current==newName)return;
    if(!folderMode){*current=newName;return;}
    if(kind=="deck") {
        if(SafeScriptName(newName)!=newName)throw std::runtime_error("Invalid Folder name.");
        auto destination=folderRoot.parent_path()/PathFromUtf8(newName);
        if(std::filesystem::exists(destination))throw std::runtime_error("Folder already exists: "+PathToUtf8(destination));
        const auto previousRoot=folderRoot;const auto previousName=name;
        std::filesystem::rename(folderRoot,destination);folderRoot=destination;name=newName;
        if(!Save(folderRoot/"main.deck")) {
            const auto error=LastError();std::filesystem::rename(folderRoot,previousRoot);
            folderRoot=previousRoot;name=previousName;throw std::runtime_error(error);
        }
        return;
    }
    struct Move {std::string* reference;std::string next;};std::vector<Move> moves;
    if(kind=="card") {
        moves.push_back({&card->scriptFile,"script/"+SafeScriptName(newName)+".js"});
        for(auto& o:card->objects)if(!o.scriptFile.empty())moves.push_back({&o.scriptFile,"script/"+SafeScriptName(newName)+"_"+SafeScriptName(o.name)+".js"});
    } else if(!object->scriptFile.empty())moves.push_back({&object->scriptFile,"script/"+SafeScriptName(card->name)+"_"+SafeScriptName(newName)+".js"});
    auto pathKey=[](const std::string& file){auto key=PathFromUtf8(file).lexically_normal().wstring();std::transform(key.begin(),key.end(),key.begin(),::towlower);return key;};
    std::map<std::wstring,int> uses;uses[pathKey(scriptFile)]++;
    for(const auto& c:cards){uses[pathKey(c.scriptFile)]++;for(const auto& o:c.objects)uses[pathKey(o.scriptFile)]++;}
    std::set<std::filesystem::path> destinations;
    for(const auto& m:moves)if(*m.reference!=m.next) {
        auto from=ScriptPath(folderRoot,*m.reference),to=ScriptPath(folderRoot,m.next);
        auto key=to.wstring();std::transform(key.begin(),key.end(),key.begin(),::towlower);
        if(uses[pathKey(*m.reference)]>1 || !destinations.insert(std::filesystem::path(key)).second || std::filesystem::exists(to))
            throw std::runtime_error("Script rename conflict: "+PathToUtf8(to));
        if(!std::filesystem::exists(from))throw std::runtime_error("Script missing: "+PathToUtf8(from));
    }
    std::vector<Move*> completed;
    try {for(auto& m:moves)if(*m.reference!=m.next){std::filesystem::rename(ScriptPath(folderRoot,*m.reference),ScriptPath(folderRoot,m.next));completed.push_back(&m);}}
    catch(...){for(auto it=completed.rbegin();it!=completed.rend();++it){std::error_code ec;std::filesystem::rename(ScriptPath(folderRoot,(*it)->next),ScriptPath(folderRoot,*(*it)->reference),ec);}throw;}
    const auto previousName=*current;
    std::vector<std::string> previousReferences;
    for(auto& m:moves){previousReferences.push_back(*m.reference);*m.reference=m.next;}
    *current=newName;
    if(!Save(folderRoot/"main.deck")) {
        const auto error=LastError();*current=previousName;
        for(std::size_t i=0;i<moves.size();++i)*moves[i].reference=previousReferences[i];
        for(auto it=completed.rbegin();it!=completed.rend();++it){std::error_code ec;std::filesystem::rename(ScriptPath(folderRoot,(*it)->next),ScriptPath(folderRoot,*(*it)->reference),ec);}
        throw std::runtime_error(error);
    }
}
CardObject& Stack::DuplicateObject(const std::string& cardId,const std::string& objectId) {
    auto* card=FindCard(cardId);auto* original=card?card->FindObject(objectId):nullptr;
    if(!original)throw std::runtime_error("Object not found.");CardObject copy=*original;
    std::size_t n=1;while(card->FindObject("object"+std::to_string(n)))++n;copy.id="object"+std::to_string(n);
    copy.name+=" copy";while(std::any_of(card->objects.begin(),card->objects.end(),[&](const auto& o){return o.name==copy.name;}))copy.name+=" copy";
    copy.x+=10;copy.y+=10;
    if(folderMode && !copy.scriptFile.empty())copy.script=ReadScript(ScriptPath(folderRoot,copy.scriptFile));
    if(folderMode && !HasObjectScriptEvent(copy.type))copy.scriptFile.clear();
    if(folderMode && HasObjectScriptEvent(copy.type)){copy.scriptFile="script/"+SafeScriptName(card->name)+"_"+SafeScriptName(copy.name)+".js";
        if(std::filesystem::exists(ScriptPath(folderRoot,copy.scriptFile)))throw std::runtime_error("Duplicate script destination exists.");CreateScript(folderRoot,copy.scriptFile,copy.script);}
    card->objects.push_back(std::move(copy));return card->objects.back();
}
