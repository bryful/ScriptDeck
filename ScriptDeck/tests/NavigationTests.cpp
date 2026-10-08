#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GuiView.h"
#include "ScriptEngine.h"
#include "HomeDeck.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
int main()
{
    const auto root=std::filesystem::temp_directory_path()/PathFromUtf8("scriptdeck-navigation-日本語-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try {
        const auto home=root/"home.deck",first=root/"first.deck",second=root/"second.deck";
        auto blocked=first;blocked+=".tmp";
        EnsureHomeDeck(home);Stack homeData;assert(homeData.Load(home));assert(homeData.name=="Home");
        homeData.name="My Home";assert(homeData.Save(home));EnsureHomeDeck(home);assert(homeData.Load(home)&&homeData.name=="My Home");
        Stack source;source.CreateNew("First");assert(source.script==DefaultDeckScript());
        assert(source.cards.front().script==DefaultCardScript());
        for(const auto type:{ObjectType::Button,ObjectType::Listbox,ObjectType::DropdownList,ObjectType::Checkbox,ObjectType::RadioButton,ObjectType::Field,ObjectType::InputBox,ObjectType::Text,ObjectType::Image}) {
            Stack templates;templates.CreateNew();const auto& object=templates.AddObject(templates.currentCardId,type,"test");
            assert(object.script==DefaultObjectScript(type));
            Stack reloaded;assert(reloaded.FromJson(templates.ToJson()));assert(reloaded.cards.front().objects.front().script==object.script);
        }
        const auto firstId=source.currentCardId;
        const auto secondId=source.AddCard("Second").id,thirdId=source.AddCard("Third").id;
        source.AddObject(firstId,ObjectType::Text,"message");assert(source.Save(first));
        Stack other;other.CreateNew("Other");assert(other.Save(second));
        GuiServices services;services.discardChanges=[]{return true;};services.chooseFile=[](bool){return std::filesystem::path{};};
        LaunchOptions options;options.mode=LaunchMode::Magic;options.stackFile=first;
        GuiView view(options,services);
        ScriptHost host;host.workingDirectory=root;
        host.model=std::make_shared<ScriptModel>([&]()->Stack&{return view.MutableDeckData();},[&]{return view.ScriptGeneration();},[&]{view.NotifyScriptMutation();});
        host.navigateCard=[&](const auto& action,const auto& target){return view.NavigateCard(action,target);};
        host.changeDeck=[&](const auto& path,bool save){view.RequestDeckChange(path,save);};
        host.goHome=[&](bool save){view.RequestDeckChange(EnsureHomeDeck(home),save);};
        ScriptEngine js([](const std::string&){},host);
        js.RunGlobal(R"JS(
function check(v){if(!v)throw new Error('navigation assertion');}
function fail(f){let failed=false;try{f();}catch(e){failed=true;}check(failed);}
check(typeof prefCard==='undefined' && typeof app.prefCard==='undefined');
check(typeof changeCard==='undefined' && typeof app.changeCard==='undefined');
globalThis.oldCard=app.currentCard;
check(nextCard() && app.currentCard.name==='Second');
check(app.nextCard() && app.currentCard.name==='Third');check(!nextCard());
check(prevCard() && app.currentCard.name==='Second');check(app.prevCard());check(!prevCard());
check(endCard() && app.currentCard.name==='Third');check(!endCard());check(topCard());check(!topCard());
check(goCardIndex(1));check(goCard('Third'));check(app.goCard('card1'));
check(oldCard.id==='card1');fail(()=>goCardIndex(-1));fail(()=>goCardIndex(1.5));fail(()=>goCardIndex(3));fail(()=>goCardIndex());fail(()=>goCard('missing'));fail(()=>goCard(1));
)JS","navigation.js");
        assert(view.HasUnsavedChanges());
        view.MutableDeckData().cards[1].name="Repeated";view.MutableDeckData().cards[2].name="Repeated";
        js.RunGlobal("fail(()=>goCard('Repeated'));check(goCard('card2'));","ambiguous.js");
        const auto generation=view.ScriptGeneration();
        js.RunGlobal("app.deck.name='Saved First';changeDeck('second.deck');check(app.deck.name==='Saved First');app.deck.name='Final Saved First';fail(()=>nextCard());","switch.js");
        assert(view.HasPendingDeckChange()&&view.DeckName()=="Final Saved First");
        assert(view.ApplyPendingDeckChange());assert(view.ScriptGeneration()==generation+1&&view.DeckName()=="Other"&&!view.HasUnsavedChanges());
        Stack saved;assert(saved.Load(first)&&saved.name=="Final Saved First");
        js.RunGlobal("fail(()=>oldCard.name);","stale.js");
        view.MutableDeckData().name="Discarded Other";
        view.RequestDeckChange(first,false);assert(view.ApplyPendingDeckChange());assert(saved.Load(second)&&saved.name=="Other");
        assert(view.DeckName()=="Final Saved First");
        // Same-path default saving reloads the newly saved state.
        view.MutableDeckData().name="Same-path Saved";view.RequestDeckChange(first,true);assert(view.ApplyPendingDeckChange());assert(view.DeckName()=="Same-path Saved");
        // Malformed destination leaves current deck and disk untouched.
        const auto bad=root/"bad.deck";{std::ofstream f(bad);f<<"bad JSON";}
        const auto before=view.ScriptGeneration();bool failed=false;
        try{view.RequestDeckChange(bad,true);}catch(const std::exception&){failed=true;}
        assert(failed&&!view.HasPendingDeckChange()&&view.ScriptGeneration()==before);
        // Save failure must not switch the deck.
        {std::ofstream f(blocked);f<<"blocked";}
        view.RequestDeckChange(second,true);failed=false;
        try{view.ApplyPendingDeckChange();}catch(const std::exception&){failed=true;}
        assert(failed&&view.DeckName()=="Same-path Saved"&&view.ScriptGeneration()==before&&!view.HasPendingDeckChange());
        std::filesystem::remove(blocked);
        view.RequestDeckChange(second,false);view.CancelPendingDeckChange();assert(!view.ApplyPendingDeckChange());
        // New runtime targets the new model generation, with home API defaults.
        host.model=std::make_shared<ScriptModel>([&]()->Stack&{return view.MutableDeckData();},[&]{return view.ScriptGeneration();},[&]{view.NotifyScriptMutation();});
        ScriptEngine fresh([](const std::string&){},host);
        fresh.RunGlobal("app.deck.name='Before Home';goHome();","home.js");assert(view.ApplyPendingDeckChange());
        assert(view.DeckName()=="My Home");assert(saved.Load(first)&&saved.name=="Before Home");
        view.MutableDeckData().name="Unsaved Home";view.RequestDeckChange(second,false);assert(view.ApplyPendingDeckChange());assert(saved.Load(home)&&saved.name=="My Home");
        view.MutableDeckData().name="Unsaved Other";
        fresh.RunGlobal("goHome(false);","home-discard.js");assert(view.ApplyPendingDeckChange());
        assert(saved.Load(second)&&saved.name=="Other");
        fresh.RunGlobal("changeDeck('second.deck',false);","deck-discard.js");assert(view.ApplyPendingDeckChange());
        fresh.RunGlobal("let invalid=0;for(const f of [()=>goHome(1),()=>changeDeck('second.deck',1),()=>changeDeck()]){try{f();}catch(e){invalid++;}}if(invalid!==3)throw new Error('Bad validation');","invalid.js");
        assert(!view.HasPendingDeckChange());
        // Deck file APIs save synchronously and reload without saving dirty data.
        GuiView files(options,services);
        ScriptHost fileHost;fileHost.workingDirectory=root;
        fileHost.model=std::make_shared<ScriptModel>([&]()->Stack&{return files.MutableDeckData();},[&]{return files.ScriptGeneration();},[&]{files.NotifyScriptMutation();});
        fileHost.openDeck=[&](const auto& path){files.RequestOpenDeck(path);};
        fileHost.saveDeck=[&](const auto& path){files.SaveDeck(path);};
        bool cancel=true;
        fileHost.saveAsDeck=[&]{if(cancel)return false;files.SaveDeck(root/"保存.deck");return true;};
        ScriptEngine fileJs([](const std::string&){},fileHost);
        fileJs.RunGlobal("app.deck.name='API Saved';saveDeck();", "save-deck.js");
        assert(saved.Load(first)&&saved.name=="API Saved"&&!files.HasUnsavedChanges());
        fileJs.RunGlobal("app.deck.name='Discard Me';app.openDeck();", "reload.js");
        assert(files.DeckName()=="Discard Me"&&files.HasPendingDeckChange());
        assert(files.ApplyPendingDeckChange()&&files.DeckName()=="API Saved");
        fileHost.model=std::make_shared<ScriptModel>([&]()->Stack&{return files.MutableDeckData();},[&]{return files.ScriptGeneration();},[&]{files.NotifyScriptMutation();});
        ScriptEngine reloadedFiles([](const std::string&){},fileHost);
        reloadedFiles.RunGlobal("app.deck.name='Save Path';app.saveDeck('別名.deck');", "save-path.js");
        assert(files.DeckPath()==root/PathFromUtf8("別名.deck")&&saved.Load(files.DeckPath())&&saved.name=="Save Path");
        reloadedFiles.RunGlobal("if(saveAsDeck()!==false)throw new Error('Cancel');", "cancel-save.js");
        assert(files.DeckPath()==root/PathFromUtf8("別名.deck"));
        cancel=false;
        reloadedFiles.RunGlobal("if(app.saveAsDeck()!==true)throw new Error('Save As');", "save-as.js");
        assert(files.DeckPath()==root/PathFromUtf8("保存.deck")&&saved.Load(files.DeckPath()));
        reloadedFiles.RunGlobal("let errors=0;for(const f of [()=>openDeck('missing.deck'),()=>saveDeck(''),()=>openDeck(3),()=>saveDeck(null),()=>saveDeck('missing-folder/file.deck')]){try{f();}catch(e){errors++;}}if(errors!==5)throw new Error('Validation');", "file-errors.js");
        assert(!files.HasPendingDeckChange()&&files.DeckPath()==root/PathFromUtf8("保存.deck"));
        reloadedFiles.RunGlobal("openDeck('second.deck');", "open-path.js");assert(files.ApplyPendingDeckChange()&&files.DeckPath()==second);
        // Missing Player input errors; Magic creates the requested file, not Home.
        LaunchOptions missing;missing.stackFile=root/"new.deck";missing.mode=LaunchMode::Player;
        bool startupFailed=false;try{GuiView player(missing,services);}catch(const std::exception&){startupFailed=true;}
        assert(startupFailed&&!std::filesystem::exists(missing.stackFile));
        missing.mode=LaunchMode::Magic;GuiView created(missing,services);
        assert(created.DeckPath()==missing.stackFile&&saved.Load(missing.stackFile)&&!created.HasUnsavedChanges());
        missing.stackFile=bad;startupFailed=false;try{GuiView invalid(missing,services);}catch(const std::exception&){startupFailed=true;}
        assert(startupFailed);std::ifstream unchanged(bad);std::string content;unchanged>>content;assert(content=="bad");
        // Player exit saves changed data without prompting; Magic still prompts.
        int prompts=0,saveErrors=0;
        GuiServices closing=services;
        closing.discardChanges=[&]{++prompts;return false;};
        closing.reportSaveError=[&](const std::string&){++saveErrors;};
        LaunchOptions closeOptions;closeOptions.mode=LaunchMode::Player;closeOptions.stackFile=second;
        GuiView closingPlayer(closeOptions,closing);
        assert(closingPlayer.CanClose()&&prompts==0);
        closingPlayer.MutableDeckData().name="Exit Saved";closingPlayer.NotifyScriptMutation();
        assert(closingPlayer.HasUnsavedChanges()&&closingPlayer.CanClose());
        assert(saved.Load(second)&&saved.name=="Exit Saved"&&!closingPlayer.HasUnsavedChanges()&&prompts==0);
        closingPlayer.MutableDeckData().name="Keep On Failure";closingPlayer.NotifyScriptMutation();
        auto failedSave=second;failedSave+=".tmp";{std::ofstream f(failedSave);f<<"blocked";}
        assert(!closingPlayer.CanClose()&&saveErrors==1&&closingPlayer.HasUnsavedChanges());
        assert(saved.Load(second)&&saved.name=="Exit Saved");std::filesystem::remove(failedSave);
        closingPlayer.SetMagic(true);assert(!closingPlayer.CanClose()&&prompts==1);
        closingPlayer.SetMagic(false);assert(closingPlayer.CanClose()&&prompts==1);
        assert(saved.Load(second)&&saved.name=="Keep On Failure");
        // Embedded Home button creates an unsaved Deck and enters Magic.
        Stack embedded;assert(embedded.FromJson(EmbeddedHomeDeck()));
        const auto* newButton=embedded.cards.front().FindObject("new");assert(newButton);
        closeOptions.mode=LaunchMode::Player;GuiView newFromHome(closeOptions,services);
        ScriptHost newHost;newHost.newDeck=[&]{newFromHome.RequestNewDeck();};
        ScriptEngine newJs([](const std::string&){},newHost);
        newJs.RunScoped(newButton->script,"home-new.js","mouseUp");
        assert(newFromHome.HasPendingDeckChange());assert(newFromHome.ApplyPendingDeckChange());
        assert(newFromHome.IsMagic()&&newFromHome.DeckPath().empty()&&newFromHome.HasUnsavedChanges());
#ifndef _WIN32
        // No Deck argument resolves to AppData home, without a file dialog.
        const char* previous=std::getenv("XDG_DATA_HOME");const std::optional<std::string> backup=previous?std::optional<std::string>(previous):std::nullopt;
        setenv("XDG_DATA_HOME",root.string().c_str(),1);
        LaunchOptions empty;GuiView initial(empty,services);assert(initial.DeckPath()==root/"ScriptDeck"/"home.deck");assert(initial.DeckName()=="Home");
        initial.MutableDeckData().name="Persistent Home";assert(initial.MutableDeckData().Save(initial.DeckPath()));
        GuiView repeated(empty,services);assert(repeated.DeckName()=="Persistent Home");
        if(backup)setenv("XDG_DATA_HOME",backup->c_str(),1);else unsetenv("XDG_DATA_HOME");
#endif
        std::filesystem::remove_all(root);
        std::cout<<"PASS: card navigation, validation, home creation/preservation, deferred deck switch, save/discard, same-path reload, stale references and failure rollback\n";
    } catch(...) {std::filesystem::remove_all(root);throw;}
}
