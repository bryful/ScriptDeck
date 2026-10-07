#include "ScriptEngine.h"
#include "LaunchOptions.h"
#include "Utf8Path.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    const auto root=std::filesystem::temp_directory_path()/ ("scriptdeck-builtins-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(root/"folder"/"nested");
        std::filesystem::create_directories(root/"empty");
        std::filesystem::create_directories(root/"folder2");
        ScriptHost host;host.workingDirectory=root;host.args={"abc","{\"value\":12}","日本語","-magic"};
        bool topMost=false;int frontCalls=0;
        host.setTopMost=[&](bool value){topMost=value;};
        host.getTopMost=[&]{return topMost;};
        host.windowFront=[&]{++frontCalls;};
        bool console=false,magic=false;int consoleCalls=0,beeps=0,plays=0,stops=0,flushes=0;
        ScriptBytes input={0,13,10,255};std::size_t inputOffset=0;ScriptBytes output,errors;
        host.setConsoleMode=[&](bool enabled){console=enabled;++consoleCalls;};
        host.setMagic=[&](bool enabled){magic=enabled;};
        host.beep=[&](int frequency,int duration){Check(frequency==800&&duration==200,"Beep defaults failed.");++beeps;};
        host.playWav=[&](const auto& path,bool wait){Check(path==root/"sound.wav"&&wait,"WAV parameters failed.");++plays;};
        host.stopWav=[&]{++stops;};
        host.readInput=[&](std::size_t count,bool all){const auto end=all?input.size():(std::min)(input.size(),inputOffset+count);ScriptBytes result(input.begin()+static_cast<std::ptrdiff_t>(inputOffset),input.begin()+static_cast<std::ptrdiff_t>(end));inputOffset=end;return result;};
        host.writeOutput=[&](const ScriptBytes& bytes,bool error){Check(console,"Output did not enable console.");auto& target=error?errors:output;target.insert(target.end(),bytes.begin(),bytes.end());};
        host.flushOutput=[&]{++flushes;};
        std::vector<std::string> alerts;
        ScriptEngine js([&](const std::string& text){alerts.push_back(text);},host);
        auto run=[&](const std::string& code){js.RunScoped(code,"builtins-test.js","mouseUp");};
        run(R"JS(
function check(value){if(!value)throw new Error("JS assertion failed");}
check(toWindowsPath("/c/work/aaa/bbb.tga")==="C:\\work\\aaa\\bbb.tga");
check(toUnixPath("C:\\work\\aaa\\bbb")==="/c/work/aaa/bbb");
check(fs.toWindowsPath("/d")==="D:\\");
check(fs.toUnixPath("Z:/日本語/画像.tga")==="/z/日本語/画像.tga");
check(toWindowsPath("folder/file")==="folder\\file");
check(toUnixPath("\\\\server\\share\\file")==="//server/share/file");
check(toWindowsPath("//server/share/file")==="\\\\server\\share\\file");
check(toUnixPath("")==="" && toWindowsPath("")==="");
check(toWindowsPath("/c/work/")==="C:\\work\\");
check(toUnixPath(toWindowsPath("/c/work/画像.tga"))==="/c/work/画像.tga");
const path="C:\\AAA\\AAA_001.tga";
check(getName(path)==="AAA_001.tga");
check(fs.getNameWithoutExt(path)==="AAA_001");
check(getExt(path)===".tga");
check(getParent(path)==="C:\\AAA");
check(getFrame(path)==="001" && getNameWithoutFrame(path)==="AAA_");
check(getFrame("foo.tga")==="" && getNameWithoutFrame("foo.tga")==="foo");
check(getExt(".hidden")==="" && getName("日本語/画像.png")==="画像.png");
check(getParent("C:\\file.txt")==="C:\\" && getParent("/file")==="/");
check(getParent("file")==="" && getName("")==="");
setTopMost(true);check(app.getTopMost());app.setTopMost(false);check(!getTopMost());windowFront();
check(app.getArgs().length===4 && app.getArgs()[2]==="日本語");
check(JSON.parse(app.getArgs()[1]).value===12);
let args=app.getArgs();args[0]="changed";check(app.getArgs()[0]==="abc");
fs.writeText("bom.txt","日本語",true);fs.writeText("plain.txt","日本語",false);
check(fs.readText("bom.txt")==="日本語" && fs.readText("plain.txt")==="日本語");
check(fs.readBytes("bom.txt")[0]===239 && fs.readBytes("plain.txt")[0]!==239);
fs.appendText("plain.txt","追加",true);check(fs.readText("plain.txt")==="日本語追加" && fs.readBytes("plain.txt")[0]!==239);
fs.appendText("bom.txt","追加",true);check(fs.readText("bom.txt")==="日本語追加");
fs.appendText("new.txt","新規",true);check(fs.readBytes("new.txt")[0]===239);
fs.writeBytes("empty.bin",new Uint8Array());check(fs.readBytes("empty.bin").length===0);
let bytes=new Uint8Array([0,13,10,255,128]);fs.writeBytes("binary.bin",bytes.subarray(1,4));
check(JSON.stringify(Array.from(fs.readBytes("binary.bin")))==="[13,10,255]");
check(fs.exists("binary.bin") && fs.existsFile("binary.bin") && !fs.existsDir("binary.bin"));
check(fs.existsDir("folder") && !fs.existsFile("folder") && !fs.exists("missing"));
fs.writeText("folder/child.txt","child");fs.writeText("folder/nested/deep.txt","deep");
check(fs.getFiles("folder").length===1 && fs.getDirectories("folder").length===1);
check(fs.resolvePath("folder/../plain.txt").endsWith("plain.txt"));
fs.move("binary.bin","moved.bin");check(!fs.exists("binary.bin") && fs.existsFile("moved.bin"));
fs.rename("moved.bin","renamed.bin");check(fs.existsFile("renamed.bin"));
let conflict=false;try{fs.move("renamed.bin","plain.txt");}catch(e){conflict=true;}check(conflict && fs.existsFile("renamed.bin"));
let nonempty=false;try{fs.delete("folder");}catch(e){nonempty=true;}check(nonempty && fs.existsDir("folder"));
fs.delete("empty");fs.delete("folder",true);check(!fs.exists("folder"));
fs.move("new.txt","moved-text.txt");fs.rename("moved-text.txt","名前.txt");check(fs.readText("名前.txt")==="新規");
let invalid=false;try{fs.writeText("bad.txt","bad","yes");}catch(e){invalid=e instanceof TypeError;}check(invalid && !fs.exists("bad.txt"));
fs.writeBytes("invalid.txt",new Uint8Array([255]));let utf8=false;try{fs.readText("invalid.txt");}catch(e){utf8=true;}check(utf8);
let destination=false;try{fs.rename("plain.txt","../escape.txt");}catch(e){destination=true;}check(destination);
let self=false;try{fs.move("folder2","folder2/child");}catch(e){self=true;}check(self && fs.existsDir("folder2"));
app.setConsoleMode(false);app.write("A");app.writeLine("日本語");app.writeError("ERROR");
const first=app.readBytes(2);check(first.length===2 && first[0]===0 && first[1]===13);
const rest=app.readBinary();check(rest.length===2 && rest[0]===10 && rest[1]===255);
app.writeBytes(new Uint8Array([1,2,3,4]),1,2);app.writeBinary(new Uint8Array([0,255]).buffer);
app.writeBinary(new Uint8Array());app.writeBinary(new ArrayBuffer(0));check(app.readBytes(0).length===0);app.flush();
let range=false;try{app.writeBytes(new Uint8Array([1]),0,2);}catch(e){range=true;}check(range);
app.setMagic(true);app.setMagic(false);app.beep();app.playWav("sound.wav",true);app.stopWav();
alert("PASS-BUILTINS");
)JS");
        Check(alerts.back()=="PASS-BUILTINS","JS builtin script failed.");
        Check(beeps==1&&plays==1&&stops==1&&flushes==1&&!magic,"Host callbacks failed.");
        Check(console&&consoleCalls>1,"Console state failed.");
        const std::string text="A日本語\n";ScriptBytes expected(text.begin(),text.end());expected.insert(expected.end(),{2,3,0,255});
        Check(output==expected && std::string(errors.begin(),errors.end())=="ERROR\n","Standard output bytes failed.");
        js.RunGlobal("function shared(n){return n*3;}globalThis.counter=0;", "shared.js");
        run(R"JS(
let hidden=42;
if(app.runCode("shared(4)")!==12)throw new Error("Global access failed");
if(app.runCode("typeof hidden")!=="undefined")throw new Error("Caller scope leaked");
app.runCode("var isolated=10;function localOnly(){}");
if(app.runCode("typeof isolated")!=="undefined")throw new Error("Eval scope leaked");
app.runCode("globalThis.counter++");if(counter!==1)throw new Error("Global update failed");
let caught=false;try{app.runCode("throw new Error('nested')");}catch(e){caught=e.message==="nested";}if(!caught)throw new Error("Nested error failed");
)JS");
        ScriptEngine exiting([&](const std::string&){throw std::runtime_error("Code after exit executed.");},host);
        exiting.RunGlobal("app.exit(7);alert('bad');", "exit.js");Check(exiting.ExitRequested()&&exiting.ExitCode()==7,"Exit failed.");
        auto options=ParseCommandLine(std::vector<std::string>{"-magic","test.deck","abc","-run","--help","{\"x\":1}"});
        Check(options.error.empty()&&!options.showHelp&&options.mode==LaunchMode::Magic&&options.scriptArgs.size()==4,"Multiple argument parsing failed.");
        options=ParseCommandLine(std::vector<std::string>{"test.deck","--","-magic",""});Check(options.scriptArgs==std::vector<std::string>{"-magic",""},"Literal argument parsing failed.");
#ifndef _WIN32
        if(std::filesystem::is_directory("/dev/shm")) {
            const auto across=std::filesystem::path("/dev/shm")/root.filename();
            ScriptFiles files(root);files.WriteText("across.txt","cross-device",false);
            try {
                files.Move("across.txt",PathToUtf8(across));Check(!files.Exists("across.txt",0),"Cross-device source remained.");
                files.Move(PathToUtf8(across),"returned.txt");Check(files.ReadText("returned.txt")=="cross-device","Cross-device content failed.");
                std::filesystem::create_directory(root/"cross-folder");files.WriteText("cross-folder/a.txt","directory",false);
                files.Move("cross-folder",PathToUtf8(across));files.Move(PathToUtf8(across),"returned-folder");Check(files.ReadText("returned-folder/a.txt")=="directory","Cross-device folder failed.");
            }catch(...){std::error_code ignored;std::filesystem::remove_all(across,ignored);throw;}
        }
#endif
        std::filesystem::remove_all(root);
        std::cout<<"PASS: arguments, UTF-8 BOM, binary files/streams, filesystem operations, console/mode/audio callbacks, runCode scope and exit\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
