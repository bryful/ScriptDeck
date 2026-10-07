#pragma once
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace DeckAssociation {
struct Value {
    std::uint32_t type=0;
    std::vector<unsigned char> bytes;
    bool operator==(const Value& other) const {return type==other.type && bytes==other.bytes;}
};
class Registry {
public:
    virtual ~Registry()=default;
    virtual std::optional<Value> Read(const std::wstring& key,const std::wstring& name)=0;
    virtual void Write(const std::wstring& key,const std::wstring& name,const Value& value)=0;
    virtual void RemoveValue(const std::wstring& key,const std::wstring& name)=0;
    virtual void RemoveTree(const std::wstring& key)=0;
};
inline Value String(const std::wstring& text) {
    Value v;v.type=1;v.bytes.resize((text.size()+1)*sizeof(wchar_t));
    std::memcpy(v.bytes.data(),text.c_str(),v.bytes.size());return v;
}
inline std::wstring Text(const std::optional<Value>& value) {
    if(!value || value->type!=1 || value->bytes.size()%sizeof(wchar_t))return {};
    std::wstring text(value->bytes.size()/sizeof(wchar_t),L'\0');
    if(!text.empty())std::memcpy(text.data(),value->bytes.data(),value->bytes.size());
    if(!text.empty() && text.back()==L'\0')text.pop_back();
    return text;
}
inline Value Number(std::uint32_t n) {Value v;v.type=4;v.bytes.resize(4);std::memcpy(v.bytes.data(),&n,4);return v;}
inline std::uint32_t Number(const std::optional<Value>& value) {
    if(!value || value->type!=4 || value->bytes.size()!=4)throw std::runtime_error("Invalid association backup.");
    std::uint32_t n;std::memcpy(&n,value->bytes.data(),4);return n;
}
inline bool SamePath(const std::wstring& a,const std::wstring& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(std::towlower(a[i])!=std::towlower(b[i]))return false;
    return true;
}
inline const std::wstring Extension=L"Software\\Classes\\.deck";
inline const std::wstring ProgId=L"ScriptDeck.Deck";
inline const std::wstring TypeKey=L"Software\\Classes\\ScriptDeck.Deck";
inline const std::wstring Metadata=L"Software\\ScriptDeck\\Association";
inline const std::wstring Capabilities=L"Software\\ScriptDeck\\Capabilities";
inline const std::wstring Registered=L"Software\\RegisteredApplications";
class Transaction {
    struct Change {std::wstring key,name;std::optional<Value> previous;};
    Registry& registry_;std::vector<Change> changes_;bool committed_=false;
public:
    explicit Transaction(Registry& registry):registry_(registry){}
    void Write(const std::wstring& key,const std::wstring& name,const Value& value) {
        changes_.push_back({key,name,registry_.Read(key,name)});registry_.Write(key,name,value);
    }
    void Commit(){committed_=true;}
    ~Transaction() {
        if(committed_)return;
        for(auto i=changes_.rbegin();i!=changes_.rend();++i)try {
            if(i->previous)registry_.Write(i->key,i->name,*i->previous);else registry_.RemoveValue(i->key,i->name);
        }catch(...){}
    }
};
inline bool Install(Registry& registry,const std::wstring& executable)
{
    if(executable.empty()||executable.find(L'"')!=std::wstring::npos)throw std::invalid_argument("Invalid executable path.");
    const auto previous=registry.Read(Extension,L"");
    Transaction tx(registry);
    // Keep the original association when registering again or moving ScriptDeck.
    if(!SamePath(Text(previous),ProgId) || !registry.Read(Metadata,L"PreviousPresent")) {
        tx.Write(Metadata,L"PreviousPresent",Number(previous && !SamePath(Text(previous),ProgId)?1:0));
        if(previous) {
            tx.Write(Metadata,L"PreviousType",Number(previous->type));
            Value data{3,previous->bytes};tx.Write(Metadata,L"PreviousData",data);
        }
    }
    tx.Write(Metadata,L"Executable",String(executable));
    tx.Write(TypeKey,L"",String(L"ScriptDeck Deck"));
    tx.Write(TypeKey+L"\\DefaultIcon",L"",String(L"\""+executable+L"\",1"));
    tx.Write(TypeKey+L"\\shell",L"",String(L"open"));
    tx.Write(TypeKey+L"\\shell\\open\\command",L"",String(L"\""+executable+L"\" \"%1\""));
    tx.Write(Extension+L"\\OpenWithProgids",ProgId,Value{});
    tx.Write(Capabilities,L"ApplicationName",String(L"ScriptDeck"));
    tx.Write(Capabilities,L"ApplicationDescription",String(L"ScriptDeck card applications"));
    tx.Write(Capabilities,L"ApplicationIcon",String(L"\""+executable+L"\",0"));
    tx.Write(Capabilities+L"\\FileAssociations",L".deck",String(ProgId));
    tx.Write(Registered,L"ScriptDeck",String(Capabilities));
    tx.Write(Extension,L"",String(ProgId));
    tx.Commit();return true;
}
inline bool Uninstall(Registry& registry,const std::wstring& executable)
{
    const auto owner=Text(registry.Read(Metadata,L"Executable"));
    if(owner.empty() || !SamePath(owner,executable))return false;
    if(SamePath(Text(registry.Read(Extension,L"")),ProgId)) {
        if(Number(registry.Read(Metadata,L"PreviousPresent"))) {
            const auto data=registry.Read(Metadata,L"PreviousData");
            if(!data || data->type!=3)throw std::runtime_error("Association backup is missing.");
            registry.Write(Extension,L"",Value{Number(registry.Read(Metadata,L"PreviousType")),data->bytes});
        }else registry.RemoveValue(Extension,L"");
    }
    // Remove only our registration. Leave .deck and other handlers in place.
    const auto offered=registry.Read(Extension+L"\\OpenWithProgids",ProgId);
    if(offered && *offered==Value{})registry.RemoveValue(Extension+L"\\OpenWithProgids",ProgId);
    if(SamePath(Text(registry.Read(Registered,L"ScriptDeck")),Capabilities))registry.RemoveValue(Registered,L"ScriptDeck");
    registry.RemoveTree(TypeKey);registry.RemoveTree(Capabilities);registry.RemoveTree(Metadata);
    return true;
}
}
