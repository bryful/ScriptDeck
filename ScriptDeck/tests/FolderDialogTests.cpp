#include "ScriptEngine.h"
#include "Utf8Path.h"
#include <stdexcept>
#include <iostream>
int main(){
    ScriptHost host;int calls=0;bool cancel=false;
    host.selectFolderDialog=[&](const FileDialogOptions& options)->std::optional<std::filesystem::path>{
        ++calls;
        if(calls==1 && options.title!="フォルダを選択")throw std::runtime_error("title");
        if(cancel)return std::nullopt;
        return PathFromUtf8("/tmp/日本語 folder");
    };
    ScriptEngine js([](const std::string&){},host);
    js.RunGlobal(R"JS(
        function check(v){if(!v)throw Error('check');}
        function rejects(f){let failed=false;try{f();}catch(e){failed=true;}check(failed);}
        check(app.selectFolderDialog({title:'フォルダを選択',initialDirectory:'.'}).endsWith('日本語 folder'));
        check(typeof app.selectFolderDialog()==='string');
        rejects(()=>app.selectFolderDialog([]));
        rejects(()=>app.selectFolderDialog(null));
        rejects(()=>app.selectFolderDialog({multiple:true}));
        rejects(()=>app.selectFolderDialog({filters:[]}));
        rejects(()=>app.selectFolderDialog({title:'a\0b'}));
        rejects(()=>app.selectFolderDialog({initialDirectory:1}));
    )JS","folder.js");
    if(calls!=2)throw std::runtime_error("Unexpected host call");
    cancel=true;js.RunGlobal("check(app.selectFolderDialog()===null);","cancel.js");
    ScriptEngine missing([](const std::string&){});
    missing.RunGlobal("let failed=false;try{app.selectFolderDialog();}catch(e){failed=true;}if(!failed)throw Error('missing service');","missing.js");
    std::cout<<"PASS: folder options, Unicode path, cancel, invalid arguments, missing host\n";
}
