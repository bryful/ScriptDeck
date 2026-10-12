#include "Stack.h"
#include "DeckArchive.h"
#include "ScriptEngine.h"
#include "LaunchOptions.h"
#include "GuiView.h"
#include "Utf8Path.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>
namespace {
void Check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
std::string Read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Check(rejected,"Expected rejection");}
}
int main(){
 auto root=std::filesystem::temp_directory_path()/("scriptdeck-folder-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(root);
 try {
  auto launch=ParseCommandLine(std::vector<std::string>{"--magicFolder",PathToUtf8(root/"Example")});
  Check(launch.folderMode&&launch.mode==LaunchMode::Magic&&launch.error.empty(),"CLI Folder mode");
  Check(ParseCommandLine(std::vector<std::string>{"--magic"}).mode==LaunchMode::Magic,"CLI alias");
  GuiView view(launch,{});Check(view.DeckData().folderMode&&!view.ScriptsEnabled(),"Folder Magic disables scripts");
  Reject([&]{view.BeginScriptEditor("deck");});
  auto& deck=view.MutableDeckData();auto id=deck.cards.front().id;
  auto button=deck.AddObject(id,ObjectType::Button,"Run").id;
  auto script=root/"Example"/PathFromUtf8(deck.FindCard(id)->FindObject(button)->scriptFile);
  {std::ofstream out(script);out<<"function mouseUp(){ alert('external'); }\n";}
  view.SaveDeck();Check(Read(script).find("external")!=std::string::npos,"Save preserves external JS");
  auto json=deck.ToJson();Check(json.find("scriptFile")!=std::string::npos&&json.find("\"script\"")==std::string::npos,"Only external refs serialized");
  Stack player;Check(player.Load(root/"Example"),player.LastError());Check(player.FindCard(id)->FindObject(button)->script.find("external")!=std::string::npos,"Load external code");
  auto duplicate=deck.DuplicateObject(id,button).id;
  Check(deck.FindCard(id)->FindObject(duplicate)->script.find("external")!=std::string::npos,"Duplicate latest code");
  auto before=deck.FindCard(id)->name;
  {std::ofstream out(root/"Example/script/Collision.js");out<<"preserve";}
  Reject([&]{deck.Rename("card",id,{},"Collision");});Check(deck.FindCard(id)->name==before&&std::filesystem::exists(script),"Conflict is atomic");
  deck.Rename("card",id,{},"Renamed");Check(std::filesystem::exists(root/"Example/script/Renamed_Run.js"),"Card rename moves object JS");
  deck.Rename("object",id,button,"Go");Check(std::filesystem::exists(root/"Example/script/Renamed_Go.js"),"Object rename");
  deck.Rename("deck",{},{},"Moved");Check(std::filesystem::exists(root/"Moved/main.deck"),"Deck rename moves folder");
  Check(deck.Save(root/"Moved/main.deck"),deck.LastError());
  deck.Export(root/"ordinary.deck",false);Stack ordinary;Check(ordinary.Load(root/"ordinary.deck"),ordinary.LastError());
  Check(!ordinary.folderMode&&ordinary.FindCard(id)->FindObject(button)->script.find("external")!=std::string::npos,"Export embeds latest code");
  ordinary.Export(root/"RoundTrip",true);Stack roundtrip;Check(roundtrip.Load(root/"RoundTrip"),roundtrip.LastError());
  Check(roundtrip.folderMode&&roundtrip.name=="RoundTrip","Export Folder roundtrip");
  auto missing=root/"RoundTrip"/PathFromUtf8(roundtrip.scriptFile);std::filesystem::remove(missing);
  Stack editor;Check(editor.Load(root/"RoundTrip",false),editor.LastError());Check(!player.Load(root/"RoundTrip"),"Player rejects missing JS");
  auto same=root/"Moved/script/Renamed_Go.js";auto object=deck.AddObject(id,ObjectType::Button,"Go");
  Check(object.script.find("external")!=std::string::npos&&Read(same).find("external")!=std::string::npos,"New object links existing script");
  Reject([&]{deck.Export(root/"DuplicateNames",true);});
  Check(!std::filesystem::exists(root/"DuplicateNames"),"Export conflicts preflight");
  Check(!deck.Save(root/"wrong.deck"),"Save cannot convert format");
  auto invalid=nlohmann::json::parse(deck.ToJson());invalid["scriptFile"]="../escape.js";
  Stack bad;Check(!bad.FromJson(invalid.dump()),"Reject escaping script paths");
  // Export resources in both directions without modifying source decks/files.
  const auto sourceRoot=root/"AssetSource";
  std::filesystem::create_directories(sourceRoot/"data/sounds");
  std::filesystem::create_directories(root/"external");
  const auto japanese=PathFromUtf8("画像.png");
  {std::ofstream f(sourceRoot/japanese,std::ios::binary);f<<"relative-image";}
  {std::ofstream f(root/"external"/japanese,std::ios::binary);f<<"absolute-image";}
  {std::ofstream f(sourceRoot/"data/sounds/click.wav",std::ios::binary);f<<"sound";}
  Stack assetSource;assetSource.CreateNew("Assets");const auto assetCard=assetSource.currentCardId;
  const auto image1=assetSource.AddObject(assetCard,ObjectType::Image,"Relative").id;
  const auto image2=assetSource.AddObject(assetCard,ObjectType::Image,"Absolute").id;
  const auto image3=assetSource.AddObject(assetCard,ObjectType::Image,"Shared").id;
  assetSource.FindCard(assetCard)->FindObject(image1)->imagePath=PathToUtf8(japanese);
  assetSource.FindCard(assetCard)->FindObject(image2)->imagePath=PathToUtf8(root/"external"/japanese);
  assetSource.FindCard(assetCard)->FindObject(image3)->imagePath=PathToUtf8(japanese);
  Check(assetSource.Save(sourceRoot/"source.deck"),assetSource.LastError());
  Check(assetSource.Load(sourceRoot/"source.deck"),assetSource.LastError());
  const auto originalJson=assetSource.ToJson();
  assetSource.Export(root/"AssetFolder",true);
  Stack bundled;Check(bundled.Load(root/"AssetFolder"),bundled.LastError());
  auto resourcePath=[&](const Stack& value,const std::string& objectId){return PathFromUtf8(value.FindCard(assetCard)->FindObject(objectId)->imagePath);};
  const auto relativeImage=resourcePath(bundled,image1),absoluteImage=resourcePath(bundled,image2);
  Check(relativeImage!=absoluteImage,"Same-name different images receive unique names");
  Check(resourcePath(bundled,image3)==relativeImage,"Shared images keep one copy");
  Check(Read(root/"AssetFolder"/relativeImage)=="relative-image","Relative image copied");
  Check(Read(root/"AssetFolder"/absoluteImage)=="absolute-image","Absolute image copied with Unicode filename");
  Check(Read(root/"AssetFolder/data/sounds/click.wav")=="sound","Other data files keep hierarchy");
  Check(assetSource.ToJson()==originalJson&&std::filesystem::exists(sourceRoot/japanese)&&std::filesystem::exists(root/"external"/japanese),"Export preserves source references and files");
  std::filesystem::create_directories(root/"Standalone");
  bundled.Export(root/"Standalone/exported.deck",false);
  Stack standalone;Check(standalone.Load(root/"Standalone/exported.deck"),standalone.LastError());
  Check(Read(root/"Standalone"/resourcePath(standalone,image1))=="relative-image","Folder to ordinary copies images");
  Check(Read(root/"Standalone/data/sounds/click.wav")=="sound","Folder to ordinary copies other files");
  // Removing original resources must not break exported references.
  std::filesystem::remove(sourceRoot/japanese);std::filesystem::remove(root/"external"/japanese);
  Check(Read(root/"AssetFolder"/relativeImage)=="relative-image","Export remains independent of source");
  Reject([&]{assetSource.Export(root/"MissingAsset",true);});
  Check(!std::filesystem::exists(root/"MissingAsset"),"Missing resource fails before output creation");
  std::filesystem::create_directories(root/"Conflicting/data");
  {std::ofstream f(root/"Conflicting/data"/relativeImage.filename());f<<"existing-file";}
  Reject([&]{bundled.Export(root/"Conflicting/exported.deck",false);});
  Check(!std::filesystem::exists(root/"Conflicting/exported.deck")&&Read(root/"Conflicting/data"/relativeImage.filename())=="existing-file","Existing destination is never silently reused or overwritten");
  // deckx is a standard STORE ZIP; Player persists main.deck while preserving assets.
  Check(DeckZip::Crc("123456789")==0xcbf43926u,"ZIP CRC reference");
  const auto package=root/PathFromUtf8("配布.deckx");bundled.ExportDeckx(package);
  auto zipFiles=DeckZip::Decode(DeckZip::ReadFile(package));
  Check(zipFiles.count("main.deck")&&zipFiles.count("data/sounds/click.wav"),"deckx packs main and resources");
  Check(nlohmann::json::parse(zipFiles.at("main.deck")).value("storage",std::string{})!="folder","Folder export embeds scripts in main.deck");
  Check(nlohmann::json::parse(zipFiles.at("main.deck")).at("name")==bundled.name,"deckx retains Deck name");
  const auto assetsBefore=zipFiles;std::filesystem::path extractedRoot;
  {
   LaunchOptions packageOptions;packageOptions.stackFile=package;
   GuiView packageView(packageOptions,{});
   Check(packageView.DeckData().deckxMode&&packageView.DeckPath()==package&&packageView.ScriptsEnabled(),"deckx Player uses archive path");
   extractedRoot=packageView.DeckData().folderRoot;
   Check(Read(extractedRoot/resourcePath(packageView.DeckData(),image1))=="relative-image","deckx extracts image");
   Reject([&]{packageView.SetMagic(true);});Check(!packageView.IsMagic(),"deckx cannot enter Magic");
   ScriptHost packageHost;packageHost.workingDirectory=extractedRoot;
   ScriptEngine packageScripts([](const std::string&){},packageHost);
   packageScripts.RunGlobal("if(fs.readText('data/sounds/click.wav')!=='sound')throw new Error('Archive resource');","package-test.js");
   packageView.MutableDeckData().FindCard(assetCard)->FindObject(image1)->text="persisted-value";
   packageView.NotifyScriptMutation();Check(packageView.CanClose(),"deckx automatically saves Player changes");
   auto changed=DeckZip::Decode(DeckZip::ReadFile(package));
   for(const auto& entry:assetsBefore)if(entry.first!="main.deck")Check(changed.at(entry.first)==entry.second,"deckx save preserves all assets");
   Stack reopened;Check(reopened.Load(package),reopened.LastError());
   Check(reopened.FindCard(assetCard)->FindObject(image1)->text=="persisted-value","deckx data persists after reopening");
   Reject([&]{packageView.SaveDeck(root/"converted.deck");});
   packageOptions.mode=LaunchMode::Magic;Reject([&]{GuiView rejected(packageOptions,{});});
   LaunchOptions normalOptions;normalOptions.mode=LaunchMode::Magic;normalOptions.stackFile=root/"ordinary.deck";
   GuiView normalMagic(normalOptions,{});Reject([&]{normalMagic.RequestDeckChange(package,false);});
  }
  Check(!std::filesystem::exists(extractedRoot),"deckx workspace is cleaned after closing");
  // VSCode receives the entire Folder only after main.deck has been saved.
  LaunchOptions vscodeOptions;vscodeOptions.mode=LaunchMode::Magic;vscodeOptions.folderMode=true;vscodeOptions.stackFile=root/"AssetFolder";
  bool vscodeOpened=false;GuiServices vscodeServices;
  vscodeServices.openVSCode=[&](const auto& folder){
      Check(folder==root/"AssetFolder","VSCode receives Deck folder");
      Stack saved;Check(saved.Load(folder,false),saved.LastError());
      Check(saved.FindCard(assetCard)->FindObject(image1)->text=="saved-before-vscode","Save before launching editor");
      vscodeOpened=true;
  };
  GuiView vscodeView(vscodeOptions,vscodeServices);
  vscodeView.MutableDeckData().FindCard(assetCard)->FindObject(image1)->text="saved-before-vscode";
  vscodeView.NotifyScriptMutation();vscodeView.OpenInVSCode();
  Check(vscodeOpened&&!vscodeView.HasUnsavedChanges(),"VSCode handoff saves Folder edits");
  Reject([&]{view.OpenInVSCode();}); // No launcher service configured.
  LaunchOptions plainOptions;plainOptions.stackFile=root/"ordinary.deck";plainOptions.mode=LaunchMode::Magic;
  GuiView plainEditor(plainOptions,vscodeServices);Reject([&]{plainEditor.OpenInVSCode();});
  GuiServices cancelServices;cancelServices.chooseFolder=[]{return std::filesystem::path{};};
  GuiView cancelView(plainOptions,cancelServices);
  Check(!cancelView.RequestNewFolderDeck()&&!cancelView.HasPendingDeckChange(),"Folder picker cancellation leaves current Deck unchanged");
  GuiServices selectServices;selectServices.chooseFolder=[&]{return root;};
  GuiView selectedView(plainOptions,selectServices);
  Check(selectedView.RequestNewFolderDeck()&&selectedView.ApplyPendingDeckChange(),"Folder picker creates Untitled");
  Check(selectedView.DeckPath()==root/"Untitled/main.deck"&&selectedView.IsMagic(),"Folder picker enters Magic");
  // Corrupt, compressed, duplicate and escaping entries are rejected before extraction.
  auto validZip=DeckZip::Encode({{"main.deck",ordinary.ToJson()},{"aaaaa.txt","content"}});
  auto unsafe=validZip;std::size_t match=0;
  while((match=unsafe.find("aaaaa.txt",match))!=std::string::npos){unsafe.replace(match,9,"../ab.txt");match+=9;}
  Reject([&]{DeckZip::Decode(unsafe);});
  auto corrupt=validZip;corrupt[corrupt.find("content")]='X';Reject([&]{DeckZip::Decode(corrupt);});
  auto compressed=validZip;auto centralAt=compressed.find(std::string("PK\x01\x02",4));compressed[centralAt+10]=8;
  Reject([&]{DeckZip::Decode(compressed);});
  auto duplicateZip=DeckZip::Encode({{"one.txt","one"},{"two.txt","two"}});match=0;
  while((match=duplicateZip.find("two.txt",match))!=std::string::npos){duplicateZip.replace(match,7,"one.txt");match+=7;}
  Reject([&]{DeckZip::Decode(duplicateZip);});
  Reject([&]{DeckZip::Encode({{"UPPER.txt","one"},{"upper.txt","two"}});});
  Reject([&]{DeckZip::Decode(validZip.substr(0,validZip.size()-1));});
  const auto noMain=root/"no-main.deckx";DeckZip::WriteAtomic(noMain,DeckZip::Encode({{"other.txt","x"}}),false);
  Stack invalidPackage;Check(!invalidPackage.Load(noMain),"deckx requires main.deck");
  Reject([&]{bundled.ExportDeckx(package);});Check(DeckZip::Decode(DeckZip::ReadFile(package)).count("main.deck"),"deckx Export does not overwrite existing packages");
  std::filesystem::remove_all(root);std::cout<<"PASS Folder format, CLI, external preservation, duplication, rename rollback, export and missing scripts\n";
 }catch(const std::exception& error){std::error_code ignored;std::filesystem::remove_all(root,ignored);std::cerr<<error.what()<<std::endl;return 1;}
}
