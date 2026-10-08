#include "ScriptEngine.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "ScriptSystem.h"
#include <fstream>
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
        std::filesystem::path activeDeck=std::filesystem::current_path()/"日本語.deck";
        ScriptHost pathsHost;pathsHost.getDeckPath=[&]{return activeDeck;};
        ScriptEngine paths([](const std::string&){},pathsHost);
        paths.RunGlobal(R"JS(
            function check(value,message){if(!value)throw new Error(message);}
            check(app.getDeckPath().endsWith('日本語.deck'),'Unicode Deck path');
            check(fs.getName(app.getHomePath())==='home.deck','Home filename');
            check(fs.getParent(app.getHomePath())===app.getAppDataPath(),'Home/AppData consistency');
            check(app.getDocumentPath()===app.getDocumentsPath(),'Documents alias');
            check(app.getDocumentPath().length>0 && app.getTempPath().length>0,'Folder paths');
            check(fs.existsFile(app.getExePath()),'Executable path');
        )JS","paths.js");
        activeDeck=std::filesystem::current_path()/"changed.deck";
        paths.RunGlobal("check(fs.getName(app.getDeckPath())==='changed.deck','Live Deck path');","changed-path.js");
        activeDeck.clear();
        paths.RunGlobal("check(app.getDeckPath()==='','Unsaved Deck path');","unsaved-path.js");
        std::string clipboard;
        ScriptHost clipboardHost;
        clipboardHost.readClipboard=[&]{return clipboard;};
        clipboardHost.writeClipboard=[&](const std::string& text){clipboard=text;};
        ScriptEngine clipboardJs([](const std::string&){},clipboardHost);
        clipboardJs.RunGlobal(R"JS(
            if(app.readClipboard()!=='')throw new Error('Empty clipboard');
            app.writeClipboard('日本語😀\r\n二行目');
            if(app.readClipboard()!=='日本語😀\r\n二行目')throw new Error('Clipboard roundtrip');
            for(const bad of [12,null,{},'a\0b']) {
                let rejected=false;
                try{app.writeClipboard(bad);}catch(error){rejected=error instanceof TypeError;}
                if(!rejected)throw new Error('Invalid clipboard argument accepted');
            }
            if(app.readClipboard()!=='日本語😀\r\n二行目')throw new Error('Failed write changed clipboard');
            app.writeClipboard('');
            if(app.readClipboard()!=='')throw new Error('Clipboard clear');
        )JS","clipboard.js");
        clipboardHost.readClipboard=[]()->std::string{throw std::runtime_error("Clipboard busy");};
        clipboardHost.writeClipboard=[](const std::string&){throw std::runtime_error("Clipboard busy");};
        ScriptEngine failingClipboard([](const std::string&){},clipboardHost);
        failingClipboard.RunGlobal(R"JS(
            for(const operation of [()=>app.readClipboard(),()=>app.writeClipboard('test')]) {
                let failed=false;try{operation();}catch(error){failed=String(error).includes('Clipboard busy');}
                if(!failed)throw new Error('Clipboard host error not forwarded');
            }
        )JS","clipboard-errors.js");
        const auto systemRoot=std::filesystem::temp_directory_path()/("scriptdeck-system-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct RemoveSystemRoot {std::filesystem::path path;~RemoveSystemRoot(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} removeSystemRoot{systemRoot};
        std::filesystem::create_directories(systemRoot);
        {std::ofstream file(systemRoot/"metadata.bin",std::ios::binary);file<<"12345";}
        ScriptHost systemHost;systemHost.workingDirectory=systemRoot;
        ScriptEngine systemJs([](const std::string&){},systemHost);
        systemJs.RunGlobal(R"JS(
            function ensure(v){if(!v)throw new Error('System API assertion');}
            ensure(app.getEnv('SCRIPTDECK_MISSING_948739')===null);
            ensure(typeof app.getEnv('PATH')==='string');
            ensure(fs.getFileSize('metadata.bin')===5);
            const info=fs.getFileInfo('metadata.bin');
            ensure(info.size===5&&!info.isDirectory&&typeof info.modifiedTime==='number');
            ensure(fs.getFileTimestamp('metadata.bin')===fs.getFileTimes('metadata.bin').modifiedTime);
            ensure(fs.getFileInfo('.').isDirectory);
            for(const f of [()=>app.getEnv(''),()=>app.getEnv('a=b'),()=>fs.getFileSize('.'),()=>fs.getFileTimes('missing'),()=>app.runProcess('missing-app-948739'),()=>app.launchProcess('missing-app-948739'),()=>app.runProcess('x','bad'),()=>app.runProcess('x',[12])]) {
                let rejected=false;try{f();}catch(e){rejected=true;}ensure(rejected);
            }
        )JS","system-api.js");
#ifndef _WIN32
        systemJs.RunGlobal(R"JS(
            ensure(app.runProcess('/bin/sh',['-c',"printf '%s' \"$1\"",'argument-test','日本語 with spaces \"quoted\"'])==='日本語 with spaces \"quoted\"');
            ensure(app.runProcess('/bin/sh',['-c','printf result; exit 7'])==='result');
            ensure(app.runProcess('/bin/sh',['-c','pwd']).trim()===fs.resolvePath('.').replace(/\/$/,''));
            ensure(app.runProcess('/bin/sh',['-c','sleep 2.2; printf waited'])==='waited');
            app.launchProcess('/bin/sh',['-c','sleep 0.2; printf launched > launched.txt']);
        )JS","process-api.js");
        for(int i=0;i<100&&!std::filesystem::exists(systemRoot/"launched.txt");++i)std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Check(std::filesystem::exists(systemRoot/"launched.txt"),"Async process did not complete.");
#endif
        Check(ScriptSystem::Quote(L"")==L"\"\"","Empty Windows argument quoting.");
        Check(ScriptSystem::Quote(L"a b")==L"\"a b\"","Windows space quoting.");
        ScriptEngine fresh([&](const std::string& text) {messages.push_back(text);});fresh.RunGlobal("alert(typeof shared);", "new-deck.js");Check(messages.back()=="undefined","New runtime shares globals.");
        std::cout << "PASS: JavaScript execution, UTF-8 alert, objects/cycles, handlers/scopes, errors, promises, timeout, paths, clipboard and recovery\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
