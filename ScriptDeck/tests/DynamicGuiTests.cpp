#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GuiView.h"
#include "ScriptEngine.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
int main(){
 Stack s;s.CreateNew();auto& card=s.cards.front();
 auto add=[&](ObjectType t,const char* name,int x,int y,int width,int height){auto& o=s.AddObject(card.id,t,name);o.x=x;o.y=y;o.width=width;o.height=height;return o.id;};
 const auto ok=add(ObjectType::Button,"OK",20,20,100,32);const auto cancel=add(ObjectType::Button,"Cancel",160,20,100,32);
 card.enterButtonId=ok;card.escapeButtonId=cancel;card.FindObject(ok)->script="function mouseUp(event){alert(event.type);}";
 const auto field=add(ObjectType::Field,"Field",20,70,200,80);
 const auto input=add(ObjectType::InputBox,"Input",20,180,200,32);
 const auto list=add(ObjectType::Listbox,"List",300,70,160,100);
 const auto drop=add(ObjectType::DropdownList,"Drop",300,200,160,32);
 const auto image=add(ObjectType::Image,"Image",300,280,128,80);card.FindObject(image)->imageSource="resource";card.FindObject(image)->resourceId="scriptdeck.logo";
 const auto path=std::filesystem::temp_directory_path()/"scriptdeck-new-widgets.deck";assert(s.Save(path));
 Stack t;assert(t.Load(path));assert(t.ToJson()==s.ToJson());
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(640,480);io.DeltaTime=1.0f/60;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 GuiServices services;services.chooseFile=[](bool){return std::filesystem::path{};};services.discardChanges=[](){return false;};services.image=[](const auto&){return ImTextureID(0);};
 int resourceCalls=0;services.resourceImage=[&](const std::string& id){assert(id=="scriptdeck.logo");++resourceCalls;return ImTextureID(123);};
 LaunchOptions options;options.mode=LaunchMode::Player;options.stackFile=path;
 {GuiView view(options,services);
 int scriptCalls=0;ScriptHost host;host.model=std::make_shared<ScriptModel>([&]() -> Stack& {return view.MutableDeckData();},[&]{return view.ScriptGeneration();},[&]{view.NotifyScriptMutation();});
 ScriptEngine engine([&](const std::string& text){assert(text=="mouseUp");++scriptCalls;},host);
 engine.RunGlobal("app.currentCard.objectById('"+ok+"').text='Changed';", "gui-model.js");assert(view.DeckData().FindCard(card.id)->FindObject(ok)->text=="Changed");assert(view.HasUnsavedChanges());
 auto dispatch=[&](const std::vector<ButtonEvent>& events){for(const auto& event:events){const auto* c=view.DeckData().FindCard(event.cardId);const auto* b=c->FindObject(event.buttonId);engine.RunScoped(b->script,"button-test.js","mouseUp",c->id,b->id);}};
 auto frame=[&](){ImGui::NewFrame();view.Draw();ImGui::Render();};
 auto key=[&](ImGuiKey key){io.AddKeyEvent(key,true);frame();auto events=view.TakeButtonEvents();io.AddKeyEvent(key,false);frame();return events;};
 auto click=[&](float x,float y){io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
 frame();frame();assert(resourceCalls>0);
 auto events=key(ImGuiKey_Enter);assert(events.size()==1 && events[0].buttonId==ok);dispatch(events);assert(scriptCalls>0);
 events=key(ImGuiKey_Escape);assert(events.size()==1 && events[0].buttonId==cancel);
 click(40,190);auto active=ImGui::GetCurrentContext()->ActiveId;assert(active!=0);
 events=key(ImGuiKey_Enter);assert(events.empty());assert(ImGui::GetCurrentContext()->ActiveId==active);
 events=key(ImGuiKey_KeypadEnter);assert(events.empty());assert(ImGui::GetCurrentContext()->ActiveId==active);
 events=key(ImGuiKey_Escape);assert(events.empty());
 click(40,90);events=key(ImGuiKey_Enter);assert(events.empty());events=key(ImGuiKey_Escape);assert(events.empty());
 click(610,450);frame();events=key(ImGuiKey_Enter);assert(events.size()==1 && events[0].buttonId==ok);dispatch(events);assert(scriptCalls>0);
 click(50,30);events=view.TakeButtonEvents();assert(events.size()==1 && events[0].buttonId==ok);dispatch(events);assert(scriptCalls>0);
 auto* root=ImGui::FindWindowByName("ScriptDeck");ImGuiWindow* listWindow=nullptr;
 for(auto* child:root->DC.ChildWindows) if(child->Pos.x==300 && child->Pos.y==70) listWindow=child;
 assert(listWindow);click(listWindow->DC.CursorStartPos.x+10,listWindow->DC.CursorStartPos.y+ImGui::GetTextLineHeightWithSpacing()*2+5);
 events=view.TakeButtonEvents();assert(events.size()==1&&events[0].buttonId==list&&events[0].handler=="change"&&events[0].selectedIndex==2);
 assert(events[0].selectedText==view.DeckData().cards.front().FindObject(list)->items[2]);
 const auto listEvent=events[0];
 engine.RunScoped("function change(event){if(this.id!==event.target.id||event.selectedIndex!==2||event.selectedText!==this.items[2])throw new Error('List change');}","list-change.js","change",card.id,list,
   {{"selectedIndex",listEvent.selectedIndex},{"selectedText",listEvent.selectedText},{"previousSelectedIndex",listEvent.previousSelectedIndex},{"previousSelectedText",listEvent.previousSelectedText}});
 click(listWindow->DC.CursorStartPos.x+10,listWindow->DC.CursorStartPos.y+ImGui::GetTextLineHeightWithSpacing()*2+5);assert(view.TakeButtonEvents().empty());
 click(325,215);frame();auto& popupStack=ImGui::GetCurrentContext()->OpenPopupStack;assert(popupStack.Size>0 && popupStack.back().Window);
 auto* popup=popupStack.back().Window;click(popup->DC.CursorStartPos.x+10,popup->DC.CursorStartPos.y+ImGui::GetTextLineHeightWithSpacing()*2+5);
 events=view.TakeButtonEvents();assert(events.size()==1&&events[0].buttonId==drop&&events[0].selectedIndex==2&&events[0].handler=="change");
 const auto dropEvent=events[0];
 engine.RunScoped("function change(event){if(event.type!=='change'||event.selectedIndex!==2||event.selectedText!==this.selectedText)throw new Error('Dropdown change');}","dropdown-change.js","change",card.id,drop,
   {{"selectedIndex",dropEvent.selectedIndex},{"selectedText",dropEvent.selectedText},{"previousSelectedIndex",dropEvent.previousSelectedIndex},{"previousSelectedText",dropEvent.previousSelectedText}});
 engine.RunGlobal("app.currentCard.objectById('"+drop+"').selectedIndex=0;","programmatic-selection.js");assert(view.TakeButtonEvents().empty());
 engine.RunGlobal("app.currentCard.objectById('"+drop+"').selectedIndex=2;","restore-selection.js");assert(view.TakeButtonEvents().empty());
 click(40,190);io.AddInputCharactersUTF8("ABC\nDEF");frame();events=key(ImGuiKey_Enter);assert(events.empty());
 view.SetMagic(true);engine.RunGlobal("app.currentCard.backgroundColor=[0.9,0.9,0.9,1];", "magic-model.js");assert(view.HasUnsavedChanges());io.DisplaySize=ImVec2(1200,800);frame();frame();
 io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);frame();io.AddKeyEvent(ImGuiKey_S,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
 Stack saved;assert(saved.Load(path));const auto* savedCard=saved.FindCard(card.id);assert(savedCard);
 assert(savedCard->FindObject(list)->selectedIndex==2);std::cerr<<"Dropdown index="<<savedCard->FindObject(drop)->selectedIndex<<"\n";assert(savedCard->FindObject(drop)->selectedIndex==2);
 assert(savedCard->FindObject(input)->text=="ABCDEF");
 view.SetMagic(false);io.DisplaySize=ImVec2(640,480);frame();frame();
 const auto previousCount=view.DeckData().cards.front().objects.size();
 engine.RunGlobal(R"JS(
 const dynamic=app.currentCard.createObject('button',{id:'selfDelete',x:450,y:400,width:150,height:32,text:'Delete',
 script:"function mouseUp(event){alert(event.type);for(let i=0;i<40;i++)this.card.createObject('text',{x:0,y:450,text:'New'});this.remove();}"});
 )JS", "dynamic-gui.js");
 frame();frame();click(475,415);events=view.TakeButtonEvents();assert(events.size()==1 && events[0].buttonId=="selfDelete");dispatch(events);frame();
 assert(!view.DeckData().cards.front().FindObject("selfDelete"));assert(view.DeckData().cards.front().objects.size()==previousCount+40);assert(view.HasUnsavedChanges());
 view.SetMagic(true);io.DisplaySize=ImVec2(1200,800);
 engine.RunGlobal("const temp=app.currentCard.createObject('button');app.currentCard.enterButtonId=temp.id;temp.remove();", "magic-delete.js");
 assert(view.HasUnsavedChanges());assert(view.DeckData().cards.front().enterButtonId.empty());frame();frame();
 io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);frame();io.AddKeyEvent(ImGuiKey_S,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
 Stack dynamicSaved;assert(dynamicSaved.Load(path));assert(dynamicSaved.cards.front().objects.size()==previousCount+40);assert(!view.HasUnsavedChanges());

 }
 ImGui::DestroyContext();std::filesystem::remove(path);
 std::cout<<"PASS: dynamic GUI creation, self-deletion, event delivery, Magic dirty/save, Player session state and widget regression\n";
}
