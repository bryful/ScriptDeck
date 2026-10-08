#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GuiView.h"
#include "ScriptEngine.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
int main() {
    Stack deck; deck.CreateNew();
    const auto cardId=deck.currentCardId;
    const auto editorId=deck.AddObject(cardId,ObjectType::TextEditor,"codeBox").id;
    auto* editor=deck.FindCard(cardId)->FindObject(editorId);
    assert(editor->text.empty() && editor->width==400 && editor->height==240);
    assert(ParseObjectType("texteditor")==ObjectType::TextEditor);
    assert(ObjectTypeName(editor->type)=="texteditor");
    editor->text="日本語\n\talert(\"aaa\");";
    const auto path=std::filesystem::temp_directory_path()/"scriptdeck-texteditor-test.deck";
    assert(deck.Save(path)); Stack loaded; assert(loaded.Load(path));
    assert(loaded.FindCard(cardId)->FindObject(editorId)->text==editor->text);
    ScriptHost host; host.model=std::make_shared<ScriptModel>([&]()->Stack&{return loaded;},[]{return 1;},[]{});
    ScriptEngine js([](const std::string&){},host);
    js.RunGlobal(R"JS(
        const e=app.currentCard.objectByName('codeBox');
        if(e.type!=='texteditor')throw Error('type');
        e.text='日本語\n\tHello';
        const copy=e.clone({name:'copy'});
        if(copy.text!==e.text||copy.type!=='texteditor')throw Error('clone');
        const fresh=app.currentCard.createObject('texteditor',{name:'fresh'});
        if(fresh.text!==''||fresh.width!==400||fresh.height!==240)throw Error('defaults');
    )JS","texteditor.js");
    ImGui::CreateContext(); auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize=ImVec2(640,480);io.DeltaTime=1.0f/60;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    GuiServices services; LaunchOptions options;options.mode=LaunchMode::Player;options.stackFile=path;
    {
        GuiView view(options,services);
        auto frame=[&]{ImGui::NewFrame();view.Draw();ImGui::Render();};
        frame();frame();io.AddMousePosEvent(60,60);io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
        auto* state=ImGui::GetInputTextState(ImGui::GetCurrentContext()->ActiveId);
        assert(state && (state->Flags & ImGuiInputTextFlags_AllowTabInput));
        io.AddKeyEvent(ImGuiKey_Tab,true);frame();io.AddKeyEvent(ImGuiKey_Tab,false);frame();
        auto* value=view.MutableDeckData().FindCard(cardId)->FindObject(editorId);
        assert(value->text!=editor->text && view.HasUnsavedChanges());
        value->enabled=false;frame();io.AddInputCharactersUTF8("disabled");frame();
        assert(value->text.find("disabled")==std::string::npos);
        view.SetMagic(true);io.DisplaySize=ImVec2(1200,1000);frame();frame();
    }
    ImGui::DestroyContext();std::filesystem::remove(path);
    std::cout<<"PASS: TextEditor serialization, script create/clone/text, Player Tab editing, dirty state, disabled control, Magic rendering\n";
}
