#include "ScriptModel.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>
using nlohmann::json;
namespace {
std::string String(const json& value)
{
    if(!value.is_string())throw std::invalid_argument("Expected a string.");
    return value.get<std::string>();
}
bool Bool(const json& value)
{
    if(!value.is_boolean())throw std::invalid_argument("Expected a boolean.");
    return value.get<bool>();
}
double Number(const json& value)
{
    if(!value.is_number())throw std::invalid_argument("Expected a finite number.");
    const auto n=value.get<double>();if(!std::isfinite(n))throw std::invalid_argument("Expected a finite number.");return n;
}
int Integer(const json& value,int minimum,int maximum)
{
    const double n=Number(value);
    if(std::floor(n)!=n||n<minimum||n>maximum)throw std::out_of_range("Integer property is out of range.");
    return static_cast<int>(n);
}
DeckColor Color(const json& value)
{
    if(!value.is_array()||value.size()!=4)throw std::invalid_argument("Color must be [r,g,b,a] with components between 0 and 1.");
    DeckColor result{};for(std::size_t i=0;i<4;++i){const auto n=Number(value[i]);if(n<0||n>1)throw std::out_of_range("Color component is out of range.");result[i]=static_cast<float>(n);}return result;
}
json OptionalColor(const std::optional<DeckColor>& value){return value?json(*value):json(nullptr);}
void SyncSelection(CardObject& object)
{
    if(object.selectedIndex<0||static_cast<std::size_t>(object.selectedIndex)>=object.items.size()){object.selectedIndex=-1;object.text.clear();}
    else object.text=object.items[static_cast<std::size_t>(object.selectedIndex)];
}
void RequireList(const CardObject& object)
{
    if(object.type!=ObjectType::Listbox&&object.type!=ObjectType::DropdownList)throw std::invalid_argument("This operation requires a Listbox or DropdownList.");
}
json ObjectProperty(const CardObject& object,const std::string& property)
{
    if(property=="id")return object.id;
    if(property=="type")return ObjectTypeName(object.type);
    if(property=="name")return object.name;
    if(property=="text") {
        if(object.type==ObjectType::Listbox||object.type==ObjectType::DropdownList)
            return object.selectedIndex>=0?json(object.items.at(static_cast<std::size_t>(object.selectedIndex))):json("");
        return object.text;
    }
    if(property=="x")return object.x;
    if(property=="y")return object.y;
    if(property=="width")return object.width;
    if(property=="height")return object.height;
    if(property=="visible")return object.visible;
    if(property=="enabled")return object.enabled;
    if(property=="checked") {
        if(object.type!=ObjectType::Checkbox&&object.type!=ObjectType::RadioButton)throw std::invalid_argument("checked requires a Checkbox or RadioButton.");
        return object.checked;
    }
    if(property=="group") {
        if(object.type!=ObjectType::RadioButton)throw std::invalid_argument("group requires a RadioButton.");
        return object.group;
    }
    if(property=="script")return object.script;
    if(property=="imagePath")return object.imagePath;
    if(property=="imageSource")return object.imageSource;
    if(property=="resourceId")return object.resourceId;
    if(property=="backgroundColor")return OptionalColor(object.backgroundColor);
    if(property=="textColor")return OptionalColor(object.textColor);
    if(property=="borderColor")return OptionalColor(object.borderColor);
    if(property=="tintColor")return OptionalColor(object.tintColor);
    if(property=="items"||property=="itemCount"||property=="selectedIndex"||property=="selectedText"){
        RequireList(object);
        if(property=="items")return object.items;
        if(property=="itemCount")return object.items.size();
        if(property=="selectedIndex")return object.selectedIndex;
        return object.selectedIndex>=0?json(object.items.at(static_cast<std::size_t>(object.selectedIndex))):json("");
    }
    throw std::invalid_argument("Unknown object property: "+property);
}
}
ScriptModel::ScriptModel(std::function<Stack&()> deck,std::function<std::uint64_t()> generation,std::function<void()> changed)
    :deck_(std::move(deck)),generation_(std::move(generation)),changed_(std::move(changed)),capturedGeneration_(generation_())
{
    for(const auto& card:deck_().cards)
        for(const auto& object:card.objects)usedIds_[card.id].insert(object.id);
}
json ScriptModel::Access(const std::string& operation,const std::string& kind,const std::string& cardId,
                        const std::string& objectId,const std::string& property,const json& value)
{
    if(generation_()!=capturedGeneration_)throw std::runtime_error("This reference belongs to a deck that has been replaced.");
    Stack& deck=deck_();
    if(operation=="findCard"){
        const auto name=String(value);const Card* found=nullptr;
        for(const auto& card:deck.cards)if((property=="id"?card.id:card.name)==name){if(found)throw std::runtime_error("Duplicate card name. Use cardById.");found=&card;}
        return found?json(found->id):json(nullptr);
    }
    Card* card=kind=="deck"?nullptr:deck.FindCard(cardId);
    if(kind!="deck"&&!card)throw std::runtime_error("Referenced card no longer exists.");
    if(operation=="findObject"){
        if(!card)throw std::invalid_argument("Object lookup requires a card.");
        const auto name=String(value);const CardObject* found=nullptr;
        for(const auto& object:card->objects)if((property=="id"?object.id:object.name)==name){if(found)throw std::runtime_error("Duplicate object name. Use objectById.");found=&object;}
        return found?json(found->id):json(nullptr);
    }
    if(operation=="exists")return card && card->FindObject(objectId)!=nullptr;
    if(operation=="objects") {
        if(!card)throw std::invalid_argument("Object enumeration requires a card.");
        json ids=json::array();for(const auto& item:card->objects)ids.push_back(item.id);return ids;
    }
    if(operation=="remove") {
        if(!card)throw std::invalid_argument("Object removal requires a card.");
        auto found=std::find_if(card->objects.begin(),card->objects.end(),[&](const CardObject& item){return item.id==objectId;});
        if(found==card->objects.end())return false;
        usedIds_[card->id].insert(objectId);
        if(card->enterButtonId==objectId)card->enterButtonId.clear();
        if(card->escapeButtonId==objectId)card->escapeButtonId.clear();
        card->objects.erase(found);changed_();return true;
    }
    if(operation=="create"||operation=="clone") {
        if(!card)throw std::invalid_argument("Creation requires a card.");
        if(!value.is_object())throw std::invalid_argument("Initial properties must be an object.");
        auto& used=usedIds_[card->id];
        for(const auto& item:card->objects)used.insert(item.id);
        std::string newId;
        if(value.contains("id")) {
            newId=String(value.at("id"));
            if(newId.empty()||newId.find('\0')!=std::string::npos||used.count(newId))throw std::invalid_argument("Object ID is empty, contains NUL, or has already been used on this card.");
        } else {do {newId="dynamicObject"+std::to_string(nextId_++);}while(used.count(newId));}
        const std::string initialName=value.contains("name")?String(value.at("name")):newId;
        Stack staging;staging.width=deck.width;staging.height=deck.height;
        Card temporary;temporary.id=card->id;staging.cards.push_back(temporary);staging.currentCardId=card->id;
        if(operation=="clone") {
            const auto* source=card->FindObject(objectId);
            if(!source)throw std::runtime_error("Referenced object no longer exists.");
            staging.cards.front().objects.push_back(*source);
        } else staging.AddObject(card->id,ParseObjectType(property),initialName);
        auto& candidate=staging.cards.front().objects.front();candidate.id=newId;candidate.name=initialName;
        ScriptModel validator([&]()->Stack&{return staging;},[]{return std::uint64_t(1);},[]{});
        if(value.contains("items"))validator.Access("set","object",card->id,newId,"items",value.at("items"));
        for(auto item=value.begin();item!=value.end();++item)
            if(item.key()!="id"&&item.key()!="items"&&item.key()!="selectedIndex")
                validator.Access("set","object",card->id,newId,item.key(),item.value());
        if(value.contains("selectedIndex"))validator.Access("set","object",card->id,newId,"selectedIndex",value.at("selectedIndex"));
        used.insert(newId);card->objects.push_back(std::move(candidate));
        if(card->objects.back().type==ObjectType::RadioButton)card->SetChecked(newId,card->objects.back().checked);
        changed_();return newId;
    }
    if(operation=="order") {
        if(!card)throw std::invalid_argument("Ordering requires a card.");
        auto found=std::find_if(card->objects.begin(),card->objects.end(),[&](const CardObject& item){return item.id==objectId;});
        if(found==card->objects.end())throw std::runtime_error("Referenced object no longer exists.");
        const int index=Integer(value,0,static_cast<int>(card->objects.size())-1);
        const auto old=static_cast<int>(found-card->objects.begin());
        if(index!=old){auto moved=std::move(*found);card->objects.erase(found);card->objects.insert(card->objects.begin()+index,std::move(moved));changed_();}
        return nullptr;
    }
    CardObject* object=kind=="object"?card->FindObject(objectId):nullptr;
    if(kind=="object"&&!object)throw std::runtime_error("Referenced object no longer exists.");
    if(operation=="get"){
        if(object && property=="index")return static_cast<int>(object-card->objects.data());
        if(kind=="deck"){
            if(property=="name")return deck.name;
    if(property=="script")return deck.script;
            if(property=="width")return deck.width;
    if(property=="height")return deck.height;
            if(property=="currentCardId")return deck.currentCardId;
    if(property=="cardCount")return deck.cards.size();
            if(property=="startupPlacement")return deck.startupPlacement;
        }else if(kind=="card"){
            if(property=="id")return card->id;
    if(property=="name")return card->name;
    if(property=="script")return card->script;
            if(property=="width")return deck.width;
    if(property=="height")return deck.height;
            if(property=="objectCount")return card->objects.size();
    if(property=="backgroundColor")return card->backgroundColor;
            if(property=="enterButtonId")return card->enterButtonId;
    if(property=="escapeButtonId")return card->escapeButtonId;
        }else if(object)return ObjectProperty(*object,property);
        throw std::invalid_argument("Unknown property: "+property);
    }
    if(operation=="call"){
        if(!object)throw std::invalid_argument("List methods require an object.");
        RequireList(*object);
        if(!value.is_array())throw std::invalid_argument("Expected method arguments.");
        CardObject candidate=*object;json result=nullptr;
        if(property=="getItem"){
            const int index=Integer(value.at(0),0,static_cast<int>(candidate.items.size())-1);return candidate.items[static_cast<std::size_t>(index)];
        }
        if(property=="addItem"){
            candidate.items.push_back(String(value.at(0)));result=candidate.items.size()-1;
        }else if(property=="insertItem"){
            const int index=Integer(value.at(0),0,static_cast<int>(candidate.items.size()));
            candidate.items.insert(candidate.items.begin()+index,String(value.at(1)));
            if(candidate.selectedIndex>=index)++candidate.selectedIndex;
            result=index;
        }else if(property=="removeItem"){
            const int index=Integer(value.at(0),0,static_cast<int>(candidate.items.size())-1);
            candidate.items.erase(candidate.items.begin()+index);
            if(candidate.selectedIndex==index)candidate.selectedIndex=-1;else if(candidate.selectedIndex>index)--candidate.selectedIndex;
        }else if(property=="setItem"){
            const int index=Integer(value.at(0),0,static_cast<int>(candidate.items.size())-1);candidate.items[static_cast<std::size_t>(index)]=String(value.at(1));
        }else if(property=="clearItems"){candidate.items.clear();candidate.selectedIndex=-1;}
        else throw std::invalid_argument("Unknown list method.");
        SyncSelection(candidate);
        if(candidate.items!=object->items||candidate.selectedIndex!=object->selectedIndex||candidate.text!=object->text){*object=std::move(candidate);changed_();}
        return result;
    }
    if(operation!="set")throw std::invalid_argument("Unknown model operation.");
    if(kind=="deck"){
        const json before=Access("get",kind,cardId,objectId,property);
        if(property=="name")deck.name=String(value);else if(property=="script")deck.script=String(value);
        else if(property=="width")deck.width=Integer(value,1,8192);else if(property=="height")deck.height=Integer(value,1,8192);
        else if(property=="currentCardId"){
            const auto target=String(value);if(!deck.FindCard(target))throw std::invalid_argument("Card ID does not exist.");deck.currentCardId=target;
        }else if(property=="startupPlacement"){
            const auto policy=String(value);if(policy!="default"&&policy!="previous_default"&&policy!="previous_center"&&policy!="center")throw std::invalid_argument("Invalid startup placement.");deck.startupPlacement=policy;
        }else throw std::invalid_argument("Property is read-only: "+property);
        if(before!=Access("get",kind,cardId,objectId,property))changed_();
        return nullptr;
    }
    if(kind=="card"){
        const json before=Access("get",kind,cardId,objectId,property);
        if(property=="name")card->name=String(value);else if(property=="script")card->script=String(value);
        else if(property=="width")deck.width=Integer(value,1,8192);else if(property=="height")deck.height=Integer(value,1,8192);
        else if(property=="backgroundColor")card->backgroundColor=Color(value);
        else if(property=="enterButtonId"||property=="escapeButtonId"){
            const auto target=String(value);const auto* button=card->FindObject(target);
            if(!target.empty()&&(!button||button->type!=ObjectType::Button))throw std::invalid_argument("Keyboard target must be a Button on this card.");
            if(property=="enterButtonId")card->enterButtonId=target;else card->escapeButtonId=target;
        }else throw std::invalid_argument("Property is read-only: "+property);
        if(before!=Access("get",kind,cardId,objectId,property))changed_();
        return nullptr;
    }
    if(!object)throw std::invalid_argument("Unknown model kind.");
    if(property=="checked") {
        if(card->SetChecked(objectId,Bool(value)))changed_();
        return nullptr;
    }
    if(property=="group") {
        if(card->SetRadioGroup(objectId,String(value)))changed_();
        return nullptr;
    }
    const json before=ObjectProperty(*object,property);CardObject candidate=*object;
    if(property=="name")candidate.name=String(value);else if(property=="script")candidate.script=String(value);
    else if(property=="text"||property=="selectedText"){
        const auto text=String(value);
        if(property=="selectedText"||candidate.type==ObjectType::Listbox||candidate.type==ObjectType::DropdownList){
            RequireList(candidate);const auto found=std::find(candidate.items.begin(),candidate.items.end(),text);
            if(found==candidate.items.end())throw std::invalid_argument("Selected text is not in the list.");
            candidate.selectedIndex=static_cast<int>(found-candidate.items.begin());SyncSelection(candidate);
        }else{if(candidate.type==ObjectType::InputBox&&text.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("InputBox text must be one line.");candidate.text=text;}
    }else if(property=="x")candidate.x=Number(value);else if(property=="y")candidate.y=Number(value);
    else if(property=="width"||property=="height"){
        const auto size=Number(value);if(size<1||size>8192)throw std::out_of_range("Object size must be 1..8192.");
        if(property=="width")candidate.width=size;else candidate.height=size;
    }else if(property=="visible")candidate.visible=Bool(value);else if(property=="enabled")candidate.enabled=Bool(value);
    else if(property=="imagePath")candidate.imagePath=String(value);else if(property=="resourceId")candidate.resourceId=String(value);
    else if(property=="imageSource"){
        const auto source=String(value);if(source!="file"&&source!="resource")throw std::invalid_argument("Image source must be file or resource.");candidate.imageSource=source;
    }else if(property=="items"){
        RequireList(candidate);if(!value.is_array())throw std::invalid_argument("items must be an array of strings.");
        std::vector<std::string> items;for(const auto& item:value)items.push_back(String(item));candidate.items=std::move(items);SyncSelection(candidate);
    }else if(property=="selectedIndex"){
        RequireList(candidate);candidate.selectedIndex=Integer(value,-1,static_cast<int>(candidate.items.size())-1);SyncSelection(candidate);
    }else if(property=="backgroundColor"||property=="textColor"||property=="borderColor"||property=="tintColor"){
        const auto color=value.is_null()?std::optional<DeckColor>{}:std::optional<DeckColor>{Color(value)};
        if(property=="backgroundColor")candidate.backgroundColor=color;else if(property=="textColor")candidate.textColor=color;
        else if(property=="borderColor")candidate.borderColor=color;else candidate.tintColor=color;
    }else throw std::invalid_argument("Property is read-only: "+property);
    if(before!=ObjectProperty(candidate,property)||candidate.selectedIndex!=object->selectedIndex||candidate.text!=object->text){*object=std::move(candidate);changed_();}
    return nullptr;
}
