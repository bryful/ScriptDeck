#include "ScriptEngine.h"
#include "Utf8Path.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
static void Check(bool ok){if(!ok)throw std::runtime_error("Native assertion failed");}
int main(){try {
 Stack deck;deck.CreateNew();deck.AddCard("Second");std::uint64_t generation=1;int changes=0;
 ScriptHost host;host.model=std::make_shared<ScriptModel>([&]()->Stack&{return deck;},[&]{return generation;},[&]{++changes;});
 const auto root=std::filesystem::current_path();host.workingDirectory=root;int opens=0,saves=0;bool cancel=false;
 host.openFileDialog=[&](const FileDialogOptions& options){
  ++opens;Check(options.title=="画像を開く"&&std::filesystem::equivalent(options.initialDirectory,root)&&options.filters.size()==2&&options.filters[0].pattern=="*.png;*.jpg");
  if(cancel)return std::vector<std::filesystem::path>{};
  if(opens==1)std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  return std::vector<std::filesystem::path>{root/PathFromUtf8("日本語.png"),root/"two.jpg"};
 };
 host.saveFileDialog=[&](const FileDialogOptions& options){++saves;Check(options.defaultName=="result.txt"&&options.defaultExtension=="txt"&&!options.multiple);return cancel?std::optional<std::filesystem::path>{}:std::optional<std::filesystem::path>{root/"result.txt"};};
 ScriptEngine js([](const std::string&){},host);
 js.RunGlobal(R"JS(
function check(ok){if(!ok)throw new Error('Assertion failed');}
function rejects(fn){let caught=false;try{fn();}catch(e){caught=true;}check(caught);}
const card=app.currentCard;
const cb=card.createObject('checkbox',{name:'Check',checked:true});check(cb.checked && cb.type==='checkbox');cb.checked=false;
const a=card.createObject('radiobutton',{name:'A',group:'g',checked:true});
const b=card.createObject('radiobutton',{name:'B',group:'g',checked:true});check(!a.checked && b.checked);
const other=card.createObject('radiobutton',{group:'other',checked:true});check(other.checked && b.checked);
a.checked=true;check(a.checked && !b.checked);a.group='other';check(!other.checked && a.checked);
app.deck.cardById('card2').createObject('radiobutton',{group:'other',checked:true});check(a.checked);
const clone=a.clone();check(clone.checked && !a.checked);clone.checked=false;check(!clone.checked);
rejects(()=>cb.group='bad');rejects(()=>b.checked=1);rejects(()=>b.group=1);
const text=card.createObject('text');rejects(()=>text.checked=true);
const count=card.objectCount;rejects(()=>card.createObject('checkbox',{checked:'bad'}));check(card.objectCount===count);
check(JSON.parse(JSON.stringify(cb)).checked===false);
const options={title:'画像を開く',initialDirectory:'.',filters:[{name:'画像',pattern:'*.png;*.jpg'},{name:'全部',pattern:'*.*'}]};
const single=app.openFileDialog(options);check(typeof single==='string' && single.endsWith('日本語.png'));
const paths=app.openFileDialog({...options,multiple:true});check(paths.length===2 && paths[1].endsWith('two.jpg'));
const save=app.saveFileDialog({defaultName:'result.txt',defaultExtension:'.txt'});check(save.endsWith('result.txt'));
rejects(()=>app.openFileDialog([]));rejects(()=>app.openFileDialog({multiple:1}));rejects(()=>app.openFileDialog({filters:[]}));
rejects(()=>app.saveFileDialog({multiple:true}));rejects(()=>app.saveFileDialog({defaultName:'../bad'}));rejects(()=>app.openFileDialog({unknown:true}));
rejects(()=>app.openFileDialog({title:'a\0b'}));
)JS","day2-api.js");
 Check(opens==2&&saves==1&&changes>0);cancel=true;
 js.RunGlobal("check(app.openFileDialog(options)===null);check(app.saveFileDialog({defaultName:'result.txt',defaultExtension:'txt'})===null);","cancel.js");
 const auto cb=deck.cards.front().FindObject("dynamicObject1");Check(cb!=nullptr);
 js.RunScoped("function change(event){check(this===event.target && event.type==='change' && event.checked===true);globalThis.changedTarget=this.id;}","checkbox.js","change","card1",cb->id,{{"checked",true}});
 js.RunScoped(R"JS(function dropFiles(event){check(this.id==='card1' && event.target===this && event.paths.length===2 && event.paths[0]==='C:\\日本\\one.png' && event.x===12);globalThis.dropCount=event.paths.length;})JS","drop.js","dropFiles","card1","",{{"paths",{"C:\\日本\\one.png","D:\\two.txt"}},{"x",12},{"y",34}});
 Stack saved;Check(saved.FromJson(deck.ToJson()));Check(saved.ToJson()==deck.ToJson());
 auto json=nlohmann::json::parse(deck.ToJson());
 for(auto& o:json["cards"][0]["objects"])if(o["type"]=="radiobutton"){o["group"]="invalid";o["checked"]=true;}
 const auto before=saved.ToJson();Check(!saved.FromJson(json.dump()));Check(saved.ToJson()==before);
 // 古いDeckにchecked/groupがなくても既定値で読み込める。
 auto legacy=nlohmann::json::parse(before);for(auto& c:legacy["cards"])for(auto& o:c["objects"]){o.erase("checked");o.erase("group");}
 Check(saved.FromJson(legacy.dump()));for(const auto& c:saved.cards)for(const auto& o:c.objects)Check(!o.checked&&o.group.empty());
 std::cout<<"PASS: toggle API/exclusivity/JSON, change/drop payload, dialog options/results/cancel and modal time budget\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
