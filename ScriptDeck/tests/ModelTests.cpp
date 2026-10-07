#include "ScriptEngine.h"
#include <iostream>
#include <stdexcept>
static void Check(bool ok){if(!ok)throw std::runtime_error("Native assertion failed");}
int main(){try {
    Stack deck; deck.currentCardId="c1";
    Card first;first.id="c1";first.name="First";
    CardObject message;message.id="m";message.name="message";message.type=ObjectType::Text;
    CardObject list;list.id="l";list.name="list";list.type=ObjectType::Listbox;list.items={"red","green","blue"};
    CardObject button;button.id="b";button.name="button";button.type=ObjectType::Button;
    CardObject input;input.id="i";input.name="input";input.type=ObjectType::InputBox;
    first.objects={message,list,button,input};Card second=first;second.id="c2";second.name="Second";deck.cards={first,second};
    std::uint64_t generation=1;int changes=0;
    ScriptHost host;host.model=std::make_shared<ScriptModel>([&]()->Stack&{return deck;},[&]{return generation;},[&]{++changes;});
    ScriptEngine js([](const std::string&){},host);
    js.RunGlobal(R"JS(
function check(ok){if(!ok)throw new Error('JS assertion failed');}
function rejects(fn){let caught=false;try{fn();}catch(e){caught=true;}check(caught);}
const card=app.currentCard;
const message=card.objectByName('message');
check(message===card.objectById('m') && message.card===card);
check(card.objectByName('missing')===null && app.deck.cardById('missing')===null);
message.text='Test';message.x=55;message.visible=false;message.backgroundColor=[1,0,0,1];
check(message.text==='Test' && message.x===55 && message.backgroundColor[0]===1);
message.backgroundColor=null;rejects(()=>message.id='new');rejects(()=>message.width=-1);
rejects(()=>message.x=NaN);rejects(()=>message.unknown=1);
rejects(()=>card.objectById('i').text='two\nlines');
const list=card.objectById('l');check(list.itemCount===3 && list.selectedIndex===-1);
list.selectedIndex=1;check(list.selectedText==='green' && list.text==='green');
list.insertItem(0,'zero');check(list.selectedIndex===2 && list.selectedText==='green');
list.setItem(2,'GREEN');check(list.text==='GREEN');
list.removeItem(0);check(list.selectedIndex===1);
list.selectedText='blue';check(list.selectedIndex===2);rejects(()=>list.selectedIndex=99);
list.removeItem(2);check(list.selectedIndex===-1 && list.selectedText==='');
check(list.addItem('new')===2 && list.getItem(2)==='new');
let snapshot=list.items;snapshot.push('detached');check(list.itemCount===3);
list.items=['same','same'];list.selectedIndex=1;list.selectedText='same';check(list.selectedIndex===0);
list.clearItems();check(list.itemCount===0);list.items=['red','green'];list.selectedIndex=1;
check(JSON.parse(JSON.stringify(list)).itemCount===2);
card.enterButtonId='b';rejects(()=>card.escapeButtonId='m');
app.deck.width=640;app.currentCard.height=480;check(app.deck.height===480);
app.deck.currentCardId='c2';check(app.currentCard!==card && message.card===card);
app.currentCard.objectById('m').text='Second';check(message.text==='Test');
check(app.deck.cardByName('First')===card);
)JS","model-tests.js");
    Check(deck.cards[0].objects[0].text=="Test" && deck.cards[1].objects[0].text=="Second");
    Check(deck.width==640&&deck.height==480&&changes>0);
    deck.cards[0].objects[0].text="Native";js.RunGlobal("check(message.text==='Native');","live.js");
    js.RunScoped("function mouseUp(event){check(this.id==='b' && event.target===this);this.card.objectById('m').text='From button';}","button.js","mouseUp","c1","b");
    Check(deck.cards[0].objects[0].text=="From button");
    js.RunScoped("function openCard(event){check(this.id==='c2' && event.target===this);}","card.js","openCard","c2");
    deck.cards[0].objects.push_back(message);deck.cards[0].objects.back().id="other";
    js.RunGlobal("rejects(()=>card.objectByName('message'));check(card.objectById('m')===message);","duplicate.js");
    deck.cards[0].objects.erase(deck.cards[0].objects.begin());
    js.RunGlobal("rejects(()=>message.text);","removed.js");
    deck.cards[0].objects[0].selectedIndex=1;deck.cards[0].objects[0].text="stale";
    js.RunGlobal("check(list.text==='green' && list.selectedText==='green');", "native-selection.js");
    ++generation;js.RunGlobal("rejects(()=>app.deck.name);rejects(()=>card.name);","stale.js");
    std::cout<<"ModelTests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
