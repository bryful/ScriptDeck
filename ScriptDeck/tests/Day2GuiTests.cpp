#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GuiView.h"
#include "ScriptEngine.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
int main(){
 Stack deck;deck.CreateNew();const auto cardId=deck.currentCardId;deck.AddCard("Second");
 auto add=[&](ObjectType type,const char* name,int x,int y){auto& o=deck.AddObject(cardId,type,name);o.x=x;o.y=y;o.width=180;o.height=32;return o.id;};
 const auto cb=add(ObjectType::Checkbox,"Check",20,20),a=add(ObjectType::RadioButton,"A",20,80),b=add(ObjectType::RadioButton,"B",20,140);
 deck.cards.front().FindObject(a)->group="g";deck.cards.front().FindObject(b)->group="g";deck.cards.front().SetChecked(a,true);
 const auto path=std::filesystem::temp_directory_path()/"scriptdeck-day2-gui.deck";assert(deck.Save(path));
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(640,480);io.DeltaTime=1.0f/60;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 GuiServices services;services.chooseFile=[&](bool){return path;};services.discardChanges=[]{return true;};services.image=[](const auto&){return ImTextureID(0);};
 LaunchOptions options;options.mode=LaunchMode::Player;options.stackFile=path;
 {GuiView view(options,services);
  ScriptHost host;host.model=std::make_shared<ScriptModel>([&]()->Stack&{return view.MutableDeckData();},[&]{return view.ScriptGeneration();},[&]{view.NotifyScriptMutation();});
  ScriptEngine js([](const std::string&){},host);js.RunGlobal("globalThis.events=[];","events.js");
  auto frame=[&]{ImGui::NewFrame();view.Draw();ImGui::Render();};
  auto click=[&](float x,float y){io.AddMousePosEvent(x,y);io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
  frame();frame();click(28,30);auto events=view.TakeButtonEvents();assert(events.size()==1&&events[0].handler=="change"&&events[0].checked&&events[0].buttonId==cb);assert(!view.HasUnsavedChanges());
  for(const auto& event:events)js.RunScoped("function change(event){events.push([this.id,event.checked,event.type]);}","change.js","change",event.cardId,event.buttonId,{{"checked",event.checked}});
  click(170,30);auto labelEvents=view.TakeButtonEvents();assert(labelEvents.size()==1&&!labelEvents[0].checked);
  auto* toggleRoot=ImGui::FindWindowByName("ScriptDeck");auto& nav=*ImGui::GetCurrentContext();nav.NavWindow=toggleRoot;
  nav.NavNextActivateId=ImHashStr("##toggle",0,ImHashStr(cb.c_str(),0,toggleRoot->ID));frame();frame();
  auto navEvents=view.TakeButtonEvents();assert(navEvents.size()==1&&navEvents[0].checked);
  view.MutableDeckData().cards.front().FindObject(a)->enabled=false;
  click(28,150);events=view.TakeButtonEvents();assert(events.size()==2&&events[0].buttonId==a&&!events[0].checked&&events[1].buttonId==b&&events[1].checked);
  click(28,150);assert(view.TakeButtonEvents().empty());
  view.MutableDeckData().cards.front().FindObject(cb)->enabled=false;click(28,30);assert(view.TakeButtonEvents().empty());
  js.RunGlobal("check=events[0];if(check[1]!==true||check[2]!=='change')throw new Error('Bad GUI event');","verify.js");
  const std::vector<std::string> paths={"C:\\日本\\one.png","D:\\two.txt"};
  assert(!view.QueueFileDrop(paths,ImVec2(-1,20)));assert(!view.QueueFileDrop(paths,ImVec2(640,20)));assert(!view.QueueFileDrop({},ImVec2(30,50)));
  assert(view.QueueFileDrop(paths,ImVec2(30,50)));auto drops=view.TakeFileDropEvents();assert(drops.size()==1&&drops[0].paths==paths&&drops[0].cardId==cardId&&drops[0].x==30&&drops[0].y==50);
  js.RunScoped("function dropFiles(event){if(event.paths.length!==2||this.id!=='card1'||event.x!==30)throw new Error('Bad drop');}","drop.js","dropFiles",drops[0].cardId,"",{{"paths",drops[0].paths},{"x",drops[0].x},{"y",drops[0].y}});
  view.SetMagic(true);io.DisplaySize=ImVec2(1200,1000);frame();frame();assert(!view.QueueFileDrop(paths,ImVec2(400,100)));
  auto* root=ImGui::FindWindowByName("ScriptDeck");auto& g=*ImGui::GetCurrentContext();g.NavWindow=root;g.NavNextActivateId=root->GetID("実行プレビュー");frame();frame();assert(view.ScriptsEnabled());
  ImGuiWindow* canvas=nullptr;for(auto* window:g.Windows)if(std::string(window->Name).find("/card-scroll_")!=std::string::npos&&window->Active)canvas=window;assert(canvas);
  assert(!view.QueueFileDrop(paths,ImVec2(20,10)));assert(view.QueueFileDrop(paths,ImVec2(canvas->DC.CursorStartPos.x+30,canvas->DC.CursorStartPos.y+50)));view.TakeFileDropEvents();
  js.RunGlobal("app.currentCard.objectById('"+cb+"').checked=false;app.currentCard.objectById('"+a+"').checked=true;","magic.js");assert(view.HasUnsavedChanges());assert(view.TakeButtonEvents().empty());
  io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);frame();io.AddKeyEvent(ImGuiKey_S,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
  Stack saved;assert(saved.Load(path));assert(saved.cards.front().FindObject(a)->checked&&!saved.cards.front().FindObject(b)->checked);assert(!view.HasUnsavedChanges());
 }
 ImGui::DestroyContext();std::filesystem::remove(path);std::cout<<"PASS: toggle mouse events/group snapshots, disabled controls, drop area/preview, Magic dirty/save\n";
}
