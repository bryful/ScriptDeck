#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GuiView.h"
#include "ScriptEngine.h"
#include "FileAssociation.h"
#include "imgui_internal.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <map>
class MemoryRegistry final:public DeckAssociation::Registry {
public:
    std::map<std::pair<std::wstring,std::wstring>,DeckAssociation::Value> values;
    int writes=0,failOn=0;
    std::optional<DeckAssociation::Value> Read(const std::wstring& key,const std::wstring& name) override {
        auto i=values.find({key,name});if(i==values.end())return std::nullopt;return i->second;
    }
    void Write(const std::wstring& key,const std::wstring& name,const DeckAssociation::Value& value) override {
        if(++writes==failOn)throw std::runtime_error("Injected registry failure");
        values[{key,name}]=value;
    }
    void RemoveValue(const std::wstring& key,const std::wstring& name) override {values.erase({key,name});}
    void RemoveTree(const std::wstring& key) override {
        for(auto i=values.begin();i!=values.end();)if(i->first.first==key || i->first.first.find(key+L"\\")==0)i=values.erase(i);else ++i;
    }
};
int main()
{
    using namespace DeckAssociation;
    const std::wstring executable=L"C:\\日本語 folder\\ScriptDeck.exe";
    MemoryRegistry registry;
    registry.Write(Extension,L"",String(L"Other.Deck"));
    const auto userChoice=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.deck\\UserChoice";
    registry.Write(userChoice,L"ProgId",String(L"UserChoice.Deck"));
    registry.Write(Extension+L"\\OpenWithProgids",L"Other.Deck",Value{});
    auto original=registry.values;
    assert(Install(registry,executable));assert(Text(registry.Read(Extension,L""))==ProgId);
    assert(Text(registry.Read(TypeKey+L"\\DefaultIcon",L""))==L"\""+executable+L"\",1");
    assert(Text(registry.Read(TypeKey+L"\\shell\\open\\command",L""))==L"\""+executable+L"\" \"%1\"");
    assert(Install(registry,executable));assert(Uninstall(registry,executable));assert(registry.values==original);assert(!Uninstall(registry,executable));
    // Another ScriptDeck copy owns its registration; an older copy cannot delete it.
    Install(registry,executable);const std::wstring newer=L"D:\\New\\ScriptDeck.exe";Install(registry,newer);
    assert(!Uninstall(registry,executable));assert(Text(registry.Read(Extension,L""))==ProgId);
    assert(Uninstall(registry,newer));assert(registry.values==original);
    // Changes made by other apps after registration must survive uninstall.
    Install(registry,executable);registry.Write(Extension,L"",String(L"Changed.Deck"));
    assert(Uninstall(registry,executable));assert(Text(registry.Read(Extension,L""))==L"Changed.Deck");
    assert(Text(registry.Read(userChoice,L"ProgId"))==L"UserChoice.Deck");
    // Restore exact prior value type/data and expose machine defaults if no user value existed.
    MemoryRegistry raw;const Value unusual{2,{1,2,0,0}};raw.Write(Extension,L"",unusual);
    Install(raw,executable);Uninstall(raw,executable);assert(raw.Read(Extension,L"")==unusual);
    MemoryRegistry empty;Install(empty,executable);Uninstall(empty,executable);assert(empty.values.empty());
    MemoryRegistry casing;casing.values=original;Install(casing,executable);casing.Write(Extension,L"",String(L"scriptdeck.deck"));assert(Uninstall(casing,L"c:\\日本語 folder\\scriptdeck.EXE"));assert(casing.values==original);
    // Failure at any registration write rolls back changed values.
    for(int failure=1;failure<=16;++failure) {
        MemoryRegistry failed;failed.values=original;failed.failOn=failure;
        bool rejected=false;try{Install(failed,executable);}catch(const std::exception&){rejected=true;}
        if(rejected)assert(failed.values==original);else {failed.failOn=0;Uninstall(failed,executable);assert(failed.values==original);}
    }
    int installed=0,uninstalled=0;ScriptHost api;
    api.install=[&]{++installed;return true;};api.uninstall=[&]{++uninstalled;return false;};
    ScriptEngine native([](const std::string&){},api);
    native.RunGlobal("if(app.install()!==true||app.uninstall()!==false)throw new Error('association API');","association.js");
    assert(installed==1&&uninstalled==1);
    Stack deck;deck.CreateNew();const auto card=deck.currentCardId;
    auto& button=deck.AddObject(card,ObjectType::Button,"enter");button.x=20;button.y=20;deck.cards.front().enterButtonId=button.id;deck.cards.front().escapeButtonId=button.id;
    const auto path=std::filesystem::temp_directory_path()/("scriptdeck-v1-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".deck");assert(deck.Save(path));
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(640,480);io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    GuiServices services;services.discardChanges=[]{return true;};services.image=[](const auto&){return ImTextureID(0);};
    LaunchOptions options;options.stackFile=path;
    {
        GuiView view(options,services);auto frame=[&]{ImGui::NewFrame();view.Draw();ImGui::Render();};frame();frame();
        auto open=[&]{io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Space,true);frame();io.AddKeyEvent(ImGuiKey_Space,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();frame();};
        auto key=[&](ImGuiKey key){io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);frame();};
        auto runButton=[&]{auto* popup=ImGui::FindWindowByName("スクリプト実行");assert(popup&&popup->Active);io.AddMousePosEvent(popup->DC.CursorStartPos.x+14,popup->DC.CursorStartPos.y+ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.y+8);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
        auto focusInput=[&]{auto* popup=ImGui::FindWindowByName("スクリプト実行");assert(popup);io.AddMousePosEvent(popup->DC.CursorStartPos.x+24,popup->DC.CursorStartPos.y+12);io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
        open();assert(view.IsScriptConsoleOpen());assert(view.TakeButtonEvents().empty());
        assert(!view.QueueFileDrop({"file.txt"},ImVec2(20,20)));
        auto* stablePopup=ImGui::FindWindowByName("スクリプト実行");const auto stableSize=stablePopup->Size;
        for(int i=0;i<120;++i){frame();assert(stablePopup->Size.x==stableSize.x&&stablePopup->Size.y==stableSize.y);}
        runButton();assert(view.TakeConsoleCommands().empty()&&!view.IsScriptConsoleOpen()); // Blank closes without running.
        frame();open();
        focusInput();io.AddInputCharactersUTF8("globalThis.counter=(globalThis.counter||0)+1;");frame();
        io.AddKeyEvent(ImGuiKey_Enter,true);frame();
        assert(!view.IsScriptConsoleOpen());assert(view.TakeConsoleCommands().empty());
        io.AddKeyEvent(ImGuiKey_Enter,false);frame();
        auto commands=view.TakeConsoleCommands();assert(commands.size()==1&&commands[0].generation==view.ScriptGeneration());
        assert(!stablePopup->Active&&ImGui::GetCurrentContext()->OpenPopupStack.empty()&&ImGui::GetCurrentContext()->ActiveId==0);
        assert(view.TakeButtonEvents().empty());
        ScriptHost host;host.setMagic=[&](bool value){view.SetMagic(value);};
        host.model=std::make_shared<ScriptModel>([&]()->Stack&{return view.MutableDeckData();},[&]{return view.ScriptGeneration();},[&]{view.NotifyScriptMutation();});
        ScriptEngine engine([](const std::string&){},host);engine.RunGlobal("globalThis.shared=7;","deck.js");
        engine.RunGlobal(commands[0].code,"player-console.js");
        open();runButton();assert(!view.IsScriptConsoleOpen());assert(view.TakeConsoleCommands().empty());frame();commands=view.TakeConsoleCommands();assert(commands.size()==1);engine.RunGlobal(commands[0].code,"player-console.js");
        engine.RunGlobal("if(counter!==2||shared!==7)throw new Error('Shared runtime');","verify.js");
        assert(!view.HasUnsavedChanges());
        open();key(ImGuiKey_Escape);assert(!view.IsScriptConsoleOpen());assert(view.TakeButtonEvents().empty());
        open();assert(view.IsScriptConsoleOpen());
        // Replace the input, run app.setMagic(true), and verify the popup closes.
        focusInput();
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddInputCharactersUTF8("app.setMagic(true);");frame();runButton();assert(view.TakeConsoleCommands().empty());frame();commands=view.TakeConsoleCommands();assert(commands.size()==1);
        engine.RunGlobal(commands[0].code,"player-console.js");frame();assert(view.IsMagic()&&!view.IsScriptConsoleOpen());
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Space,true);frame();io.AddKeyEvent(ImGuiKey_Space,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();assert(!view.IsScriptConsoleOpen());
        view.SetMagic(false);frame();open();assert(view.IsScriptConsoleOpen());
        focusInput();io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddInputCharactersUTF8("alert('aaa');");frame();runButton();assert(view.TakeConsoleCommands().empty());frame();commands=view.TakeConsoleCommands();assert(commands.size()==1);
        bool alerted=false;
        ScriptEngine alerts([&](const std::string& text){assert(text=="aaa"&&!view.IsScriptConsoleOpen()&&!ImGui::FindWindowByName("スクリプト実行")->Active&&ImGui::GetCurrentContext()->OpenPopupStack.empty());alerted=true;});
        alerts.RunGlobal(commands[0].code,"player-console.js");assert(alerted);
        open();
        // Close button also hides it without touching the Deck.
        auto* popup=ImGui::FindWindowByName("スクリプト実行");auto& g=*ImGui::GetCurrentContext();g.NavWindow=popup;g.NavNextActivateId=popup->GetID("Close");frame();frame();assert(!view.IsScriptConsoleOpen());
    }
    {
        options.mode=LaunchMode::Magic;
        GuiView editing(options,services);
        auto frame=[&]{ImGui::NewFrame();editing.Draw();ImGui::Render();};
        const auto objectId=editing.DeckData().cards.front().objects.front().id;
        for(const std::string kind:{"deck","card","object"}) {
            editing.BeginScriptEditor(kind,card,objectId);
            if(kind=="card")assert(editing.DeckData().cards.front().script==DefaultCardScript());
            if(kind=="deck")assert(editing.DeckData().script.find("Deckオープン時に実行されます")!=std::string::npos);
            frame();frame();frame();
            assert(editing.IsScriptEditorOpen()&&!editing.ScriptsEnabled());
            auto* main=ImGui::FindWindowByName("ScriptDeck");
            assert(main && ImGui::GetInputTextState(main->GetID("##script-editor")));
            for(auto* window:ImGui::GetCurrentContext()->Windows)
                if(std::string(window->Name).find("ScriptDeck/editor_")==0)assert(!window->Active);
            io.AddInputCharactersUTF8("function test(){\nreturn 1;\n}");frame();
            std::string value=kind=="deck"?editing.DeckData().script:kind=="card"?editing.DeckData().cards.front().script:editing.DeckData().cards.front().objects.front().script;
            assert(value.find("function test")!=std::string::npos&&editing.HasUnsavedChanges());
            io.AddKeyEvent(ImGuiKey_Tab,true);frame();io.AddKeyEvent(ImGuiKey_Tab,false);frame();
            value=kind=="deck"?editing.DeckData().script:kind=="card"?editing.DeckData().cards.front().script:editing.DeckData().cards.front().objects.front().script;
            assert(value.find('\t')!=std::string::npos);
            io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);frame();
            io.AddKeyEvent(ImGuiKey_S,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
            Stack saved;assert(saved.Load(path)&&!editing.HasUnsavedChanges());
            editing.EndScriptEditor();frame();assert(!editing.IsScriptEditorOpen());
            editing.BeginScriptEditor(kind,card,objectId);
            const auto preserved=kind=="deck"?editing.DeckData().script:kind=="card"?editing.DeckData().cards.front().script:editing.DeckData().cards.front().objects.front().script;
            assert(preserved==value);editing.EndScriptEditor();
        }
    }
    ImGui::DestroyContext();std::filesystem::remove(path);
    std::cout<<"PASS: stable console size over 120 frames, close-before-execute/alert, shortcut/input/Run/Enter/Close/Escape, shared runtime, mode switch and card-event suppression; association commands, backup/restore, ownership, rollback and host API\n";
}
