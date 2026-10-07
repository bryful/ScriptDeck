#include "ScriptEngine.h"
#include <iostream>
#include <stdexcept>
#include <vector>
static void Check(bool passed, const char* message) { if (!passed) throw std::runtime_error(message); }
int main()
{
    try {
        std::vector<std::string> messages;
        ScriptEngine js([&](const std::string& text) { messages.push_back(text); });
        js.RunGlobal("alert('日本語'); alert(12); alert(true); alert(null); alert();", "values.js");
        Check(messages == std::vector<std::string>{"日本語","12","true","null","undefined"}, "Primitive conversion failed.");
        js.RunGlobal("alert({name:'テスト', count:3});const circular={};circular.self=circular;alert(circular);alert([1,2]);alert({n:12n});alert('a\\0b');", "objects.js");
        Check(messages[5].find("テスト") != std::string::npos, "Object conversion failed.");
        Check(messages[6].find("Circular") != std::string::npos, "Circular conversion failed.");
        Check(messages[8].find("12n") != std::string::npos, "BigInt conversion failed.");
        Check(messages[9]=="a\\0b", "Embedded NUL conversion failed.");
        js.RunGlobal("function shared(n){return n*2;}","deck.js");
        const std::string code = "let local=5;function mouseUp(event){alert(shared(local));alert(event.type);}";
        js.RunScoped(code,"button.js","mouseUp"); js.RunScoped(code,"button.js","mouseUp");
        Check(messages.back()=="mouseUp", "Button handler failed.");
        js.RunGlobal("alert(typeof local);alert(typeof app);alert(typeof fs);", "scope.js");
        Check(messages.back()=="object" && messages[messages.size()-2]=="object" && messages[messages.size()-3]=="undefined", "Scope/API isolation failed.");
        for(const std::string& bad : {std::string("let = ;"),std::string("throw new Error('TEST_ERROR')"),std::string("throw 5")}) {
            bool caught=false;try{js.RunScoped(bad,"bad.js","mouseUp");}catch(const std::exception&){caught=true;}Check(caught,"Script error was not reported.");
        }
        bool detailed=false;try{js.RunGlobal("throw new Error('DETAIL_TEST');", "detail.js");}catch(const std::exception& e){const std::string error=e.what();detailed=error.find("DETAIL_TEST")!=std::string::npos && error.find("detail.js")!=std::string::npos;}Check(detailed,"Error detail/source missing.");
        js.RunGlobal("alert('recovered');", "recovery.js"); Check(messages.back()=="recovered", "Error recovery failed.");
        js.RunGlobal("Promise.resolve().then(()=>alert('job'));", "promise.js"); Check(messages.back()=="job", "Pending job failed.");
        bool timeout=false;try{js.RunGlobal("while(true){}", "loop.js");}catch(const std::exception& e){timeout=std::string(e.what()).find("time limit")!=std::string::npos;}Check(timeout,"Loop did not time out.");
        bool builtinTimeout=false;try{js.RunGlobal("while(true){app.getArgs();}", "builtin-loop.js");}catch(const std::exception& e){builtinTimeout=std::string(e.what()).find("time limit")!=std::string::npos;}Check(builtinTimeout,"Builtin loop did not time out.");
        js.RunGlobal("alert('after-timeout');", "recovery.js"); Check(messages.back()=="after-timeout", "Timeout recovery failed.");
        ScriptEngine fresh([&](const std::string& text) {messages.push_back(text);});fresh.RunGlobal("alert(typeof shared);", "new-deck.js");Check(messages.back()=="undefined","New runtime shares globals.");
        std::cout << "PASS: JavaScript execution, UTF-8 alert, objects/cycles, handlers/scopes, errors, promises, timeout and recovery\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
