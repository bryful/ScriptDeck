#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GuiView.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
int main(){
 Stack deck;deck.CreateNew();const auto first=deck.currentCardId;deck.AddCard("Second");deck.AddObject(first,ObjectType::Button,"A");deck.AddObject(first,ObjectType::Text,"B");
 const auto path=std::filesystem::temp_directory_path()/"scriptdeck-order-test.deck";assert(deck.Save(path));
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(1600,1600);io.DeltaTime=1.0f/60;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 GuiServices services;services.chooseFile=[&](bool){return path;};services.discardChanges=[]{return true;};services.image=[](const auto&){return ImTextureID(0);};
 LaunchOptions options;options.mode=LaunchMode::Magic;options.stackFile=path;
 {GuiView view(options,services);auto frame=[&]{ImGui::NewFrame();view.Draw();ImGui::Render();};frame();frame();
 ImGuiWindow* editor=nullptr;for(auto* window:ImGui::GetCurrentContext()->Windows)if(std::string(window->Name).find("/editor_")!=std::string::npos)editor=window;assert(editor);
 auto activate=[&](ImGuiID id){auto& g=*ImGui::GetCurrentContext();g.NavWindow=editor;g.NavNextActivateId=id;g.NavNextActivateFlags=ImGuiActivateFlags_None;frame();frame();};
 auto order=[&](const char* scope,const char* label){activate(ImHashStr(label,0,ImHashStr(scope,0,editor->ID)));};
 order("cardOrder","上へ");assert(!view.HasUnsavedChanges());
 order("cardOrder","下へ");assert(view.DeckData().cards[1].id==first);assert(view.DeckData().currentCardId==first);assert(view.HasUnsavedChanges());
 order("cardOrder","上へ");assert(view.DeckData().cards[0].id==first);
 const auto* card=view.DeckData().FindCard(first);const auto a=card->objects[0].id,b=card->objects[1].id;
 activate(ImHashStr((std::string("A##")+a).c_str(),0,ImHashStr(a.c_str(),0,editor->ID)));
 order("objectOrder","下へ");assert(view.DeckData().FindCard(first)->objects[1].id==a);
 order("objectOrder","上へ");assert(view.DeckData().FindCard(first)->objects[0].id==a);
 order("objectOrder","下へ");order("cardOrder","下へ");
 io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);frame();io.AddKeyEvent(ImGuiKey_S,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();assert(!view.HasUnsavedChanges());
 Stack saved;assert(saved.Load(path));assert(saved.cards[1].id==first);assert(saved.FindCard(first)->objects[0].id==b);assert(saved.FindCard(first)->objects[1].id==a);assert(saved.currentCardId==first);
 }
 ImGui::DestroyContext();std::filesystem::remove(path);std::cout<<"PASS: Magic card/object order, selection retained, disabled boundary and save\n";
}
