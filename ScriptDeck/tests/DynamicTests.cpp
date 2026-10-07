#include "ScriptEngine.h"
#include <iostream>
#include <stdexcept>
static void Check(bool ok){if(!ok)throw std::runtime_error("Native assertion failed");}
int main(){try {
    Stack deck;deck.CreateNew();deck.AddCard("Second");
    int changes=0;std::uint64_t generation=1;
    ScriptHost host;host.model=std::make_shared<ScriptModel>([&]()->Stack&{return deck;},[&]{return generation;},[&]{++changes;});
    ScriptEngine js([](const std::string&){},host);
    js.RunGlobal(R"JS(
function check(ok){if(!ok)throw new Error('Assertion failed');}
function rejects(fn){let error=false;try{fn();}catch(e){error=true;}check(error);}
const c=app.currentCard;
const b=c.createObject('button',{id:'newButton',name:'Go',text:'Click',x:30,y:40,width:150,height:40,
    script:"function mouseUp(event){check(event.target===this);this.remove();const next=this.card.createObject('text',{name:'Result',text:'Deleted myself'});check(next.exists);}"});
check(b===c.objectById('newButton') && c.objectByName('Go')===b && b.exists);
check(c.objectCount===1 && c.objects[0]===b && c.objectAt(0)===b);
rejects(()=>c.objectAt(-1));rejects(()=>c.createObject('text',{id:'nul\0id'}));rejects(()=>c.createObject('bad'));rejects(()=>c.createObject('button',null));
rejects(()=>c.createObject('text',{id:'newButton'}));rejects(()=>c.createObject('text',{width:0}));
rejects(()=>c.createObject('text',{text:undefined}));rejects(()=>c.createObject('text',[]));
check(c.objectCount===1);
const types=['field','text','image','listbox','dropdownlist','inputbox'];
for(const type of types){const o=c.createObject(type);check(o.type===type && o.name===o.id);}
check(c.objectCount===7);
const list=c.createObject('listbox',{name:'List',items:['a','b'],selectedIndex:1,textColor:[1,0,0,1]});
check(list.text==='b' && list.itemCount===2);
const clone=list.clone({name:'Copy',x:300});
check(clone.id!==list.id && clone.items.join(',')==='a,b' && clone.selectedIndex===1 && clone.x===300);
clone.setItem(1,'changed');check(list.selectedText==='b');
const before=c.objectCount;rejects(()=>list.clone({items:['a'],selectedIndex:99}));check(c.objectCount===before);
list.sendToBack();check(list.index===0 && c.objectAt(0)===list);
list.bringToFront();check(list.index===c.objectCount-1);
list.moveToIndex(2);check(list.index===2);rejects(()=>list.moveToIndex(10000));
c.enterButtonId=b.id;c.escapeButtonId=b.id;
const other=app.deck.cardById('card2').createObject('button',{id:'newButton'});
rejects(()=>c.removeObject(other));check(other.exists);
const removedId=clone.id;check(c.removeObject(clone));check(!clone.exists);
check(!clone.remove() && !c.removeObject(removedId));rejects(()=>clone.text);rejects(()=>clone.text='bad');
rejects(()=>c.createObject('text',{id:removedId}));
const named=c.createObject('button',{name:'Named'});check(named.text==='Named');named.remove();
let snapshot=c.objects;snapshot.length=0;check(c.objectCount===before-1);
for(let i=0;i<200;i++){const temp=c.createObject('text');check(temp.remove());check(!temp.exists);}
check(b.text==='Click');
)JS","dynamic-tests.js");
    const auto* button=deck.FindCard("card1")->FindObject("newButton");Check(button!=nullptr);
    js.RunScoped(button->script,"self-delete.js","mouseUp","card1",button->id);
    Check(!deck.FindCard("card1")->FindObject("newButton"));
    Check(deck.FindCard("card1")->enterButtonId.empty()&&deck.FindCard("card1")->escapeButtonId.empty());
    js.RunGlobal("check(!b.exists && c.objectByName('Result').text==='Deleted myself');rejects(()=>c.createObject('button',{id:'newButton'}));","after-delete.js");
    Stack roundtrip;Check(roundtrip.FromJson(deck.ToJson()));Check(roundtrip.ToJson()==deck.ToJson());Check(changes>200);
    const auto count=deck.FindCard("card1")->objects.size();const int before=changes;
    js.RunGlobal("rejects(()=>c.createObject('inputbox',{text:'a\\nb'}));rejects(()=>c.createObject('text',{type:'button'}));", "invalid-create.js");
    Check(deck.FindCard("card1")->objects.size()==count&&changes==before);
    ++generation;js.RunGlobal("rejects(()=>c.createObject('text'));rejects(()=>b.exists);","stale.js");
    std::cout<<"DynamicTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
