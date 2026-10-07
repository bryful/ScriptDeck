#include "ScriptEngine.h"
#include "quickjs/quickjs.h"
#include <chrono>
#include <stdexcept>
#include <utility>
#include "Utf8Path.h"
#include "HomeDeck.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

struct ScriptEngine::Impl
{
    JSRuntime* runtime = nullptr;
    JSContext* context = nullptr;
    JSValue cardFactory = JS_UNDEFINED, objectFactory = JS_UNDEFINED;
    AlertCallback alert;
    ScriptHost host;
    ScriptFiles files;
    bool exitRequested = false;
    int exitCode = 0;
    std::chrono::steady_clock::time_point deadline;
    bool timedOut = false;
    explicit Impl(AlertCallback callback, ScriptHost services) : alert(std::move(callback)), host(std::move(services)), files(host.workingDirectory)
    {
        if (!alert) throw std::runtime_error("Alert callback is required.");
        runtime = JS_NewRuntime();
        if (!runtime) throw std::runtime_error("Cannot create JavaScript runtime.");
        JS_SetMemoryLimit(runtime, 64 * 1024 * 1024);
        JS_SetMaxStackSize(runtime, 1024 * 1024);
        JS_SetInterruptHandler(runtime, Interrupt, this);
        context = JS_NewContext(runtime);
        if (!context) { JS_FreeRuntime(runtime); runtime = nullptr; throw std::runtime_error("Cannot create JavaScript context."); }
        JS_SetContextOpaque(context, this);
    }
    ~Impl() { if (context) { JS_FreeValue(context,cardFactory); JS_FreeValue(context,objectFactory); JS_FreeContext(context); } if (runtime) JS_FreeRuntime(runtime); }
    void ResetDeadline() { deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2); }
    static int Interrupt(JSRuntime*, void* opaque)
    {
        auto& self = *static_cast<Impl*>(opaque);
        if (self.exitRequested) return 1;
        if (std::chrono::steady_clock::now() < self.deadline) return 0;
        self.timedOut = true; return 1;
    }
    struct HostTime {
        Impl& owner;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        explicit HostTime(Impl& value) : owner(value) {}
        ~HostTime() { owner.deadline += std::chrono::steady_clock::now() - start; }
    };
    static JSValue Alert(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
    {
        auto& self = *static_cast<Impl*>(JS_GetContextOpaque(ctx));
        if(self.exitRequested) return JS_ThrowInternalError(ctx,"ScriptDeck exit requested.");
        HostTime hostTime(self);
        size_t length = 0;
        const char* value = JS_ToCStringLen(ctx, &length, argc ? argv[0] : JS_UNDEFINED);
        if (!value) return JS_EXCEPTION;
        std::string text;
        try {
            text.reserve(length);
            for (size_t i = 0; i < length; ++i) {
                if (value[i] == '\0') text += "\\0";
                else text += value[i];
            }
        } catch (...) { JS_FreeCString(ctx, value); return JS_ThrowInternalError(ctx, "Cannot format alert."); }
        JS_FreeCString(ctx, value);
        try { self.alert(text); return JS_UNDEFINED; }
        catch (const std::exception& e) { return JS_ThrowInternalError(ctx, "%s", e.what()); }
        catch (...) { return JS_ThrowInternalError(ctx, "Alert failed."); }
    }
    static std::filesystem::path SystemPath(int operation)
    {
        if(operation==GetHomePath)return HomeDeckPath();
        if(operation==GetAppDataPath)return HomeDeckPath().parent_path();
        if(operation==GetTempPath)return std::filesystem::temp_directory_path();
#ifdef _WIN32
        if(operation==GetDocumentPath) {
            PWSTR directory=nullptr;
            const auto status=SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&directory);
            if(FAILED(status)){CoTaskMemFree(directory);throw std::runtime_error("Cannot locate Documents.");}
            std::filesystem::path result;
            try {result=std::filesystem::path(directory);}
            catch(...){CoTaskMemFree(directory);throw;}
            CoTaskMemFree(directory);return result;
        }
        std::wstring buffer(512,L'\0');
        for(;;) {
            const auto length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
            if(!length)throw std::runtime_error("Cannot locate executable.");
            if(length<buffer.size())return std::filesystem::path(buffer.substr(0,length));
            if(buffer.size()>=32768)throw std::runtime_error("Executable path is too long.");
            buffer.resize((std::min)(buffer.size()*2,std::size_t(32768)));
        }
#else
        if(operation==GetDocumentPath) {
            if(const char* home=std::getenv("HOME"))if(*home)return PathFromUtf8(home)/"Documents";
            throw std::runtime_error("Cannot locate Documents.");
        }
        // Linux is the supported non-Windows build/test host.
        return std::filesystem::read_symlink("/proc/self/exe");
#endif
    }
    enum Operation {
        GetHomePath, GetDeckPath, GetExePath, GetDocumentPath, GetTempPath, GetAppDataPath,
        ReadClipboard, WriteClipboard, GetArgs, Write, WriteLine, WriteError, ReadBytes, WriteBytes, ReadBinary, WriteBinary, Flush,
        OpenDialog, SaveDialog, Console, Magic, BeepOp, PlayWav, StopWav, Exit,
        ReadText, FileWriteText, AppendText, FileReadBytes, FileWriteBytes, Exists, ExistsFile, ExistsDir,
        ResolvePath, GetFiles, GetDirectories, Move, Rename, Delete,
        GetName, GetNameWithoutExt, GetExt, GetParent, GetFrame, GetNameWithoutFrame, ToWindowsPath, ToUnixPath, NextCard, PrevCard, TopCard, EndCard, GoCardIndex, GoCard, GoHome, ChangeDeck, OpenDeck, SaveDeck, SaveAsDeck, Install, Uninstall, SetTopMost, GetTopMost, WindowFront
    };
    static JSValue Argument(int argc, JSValueConst* argv, int index) { return index < argc ? argv[index] : JS_UNDEFINED; }
    static std::string Text(JSContext* ctx, JSValueConst value)
    {
        if (!JS_IsString(value)) throw std::invalid_argument("Expected a string.");
        size_t size=0;const char* text=JS_ToCStringLen(ctx,&size,value);
        if(!text) throw std::runtime_error("Cannot convert string.");
        try { std::string result(text,size);JS_FreeCString(ctx,text);return result; }
        catch(...) {JS_FreeCString(ctx,text);throw;}
    }
    static bool Boolean(JSValueConst value, bool fallback=false)
    {
        if(JS_IsUndefined(value)) return fallback;
        if(!JS_IsBool(value)) throw std::invalid_argument("Expected a boolean.");
        return JS_VALUE_GET_BOOL(value)!=0;
    }
    static std::size_t Integer(JSContext* ctx,JSValueConst value,std::size_t fallback,std::size_t maximum)
    {
        if(JS_IsUndefined(value)) return fallback;
        double number=0;
        if(!JS_IsNumber(value)||JS_ToFloat64(ctx,&number,value)<0||!std::isfinite(number)||number<0||std::floor(number)!=number||number>static_cast<double>(maximum))
            throw std::invalid_argument("Integer argument is out of range.");
        return static_cast<std::size_t>(number);
    }
    static ScriptBytes Bytes(JSContext* ctx,JSValueConst value,bool allowBuffer=false)
    {
        size_t offset=0,length=0,element=0,total=0;
        JSValue buffer=JS_UNDEFINED;
        if(allowBuffer && JS_IsArrayBuffer(value)) buffer=JS_DupValue(ctx,value);
        else {
            if(JS_GetTypedArrayType(value)!=JS_TYPED_ARRAY_UINT8) throw std::invalid_argument("Expected Uint8Array.");
            buffer=JS_GetTypedArrayBuffer(ctx,value,&offset,&length,&element);
        }
        if(JS_IsException(buffer)) throw std::runtime_error("Invalid or detached byte array.");
        auto* data=JS_GetArrayBuffer(ctx,&total,buffer);
        if(allowBuffer && JS_IsArrayBuffer(value)) length=total;
        if(JS_HasException(ctx)||offset>total||length>total-offset||(!data&&length>0)) {JS_FreeValue(ctx,buffer);throw std::runtime_error("Invalid or detached buffer.");}
        if(length>ScriptFiles::MaxReadSize){JS_FreeValue(ctx,buffer);throw std::runtime_error("Byte array exceeds 32 MiB per call.");}
        if(length==0){JS_FreeValue(ctx,buffer);return {};}
        try { ScriptBytes result(data+offset,data+offset+length);JS_FreeValue(ctx,buffer);return result; }
        catch(...) {JS_FreeValue(ctx,buffer);throw;}
    }
    static JSValue ByteArray(JSContext* ctx,const ScriptBytes& bytes)
    {
        JSValue buffer=JS_NewArrayBufferCopy(ctx,bytes.data(),bytes.size());
        if(JS_IsException(buffer)) return buffer;
        JSValue arguments[] = {buffer, JS_UNDEFINED, JS_UNDEFINED};
        JSValue result=JS_NewTypedArray(ctx,3,arguments,JS_TYPED_ARRAY_UINT8);
        JS_FreeValue(ctx,buffer);return result;
    }
    void EnsureConsole() { if(host.setConsoleMode) host.setConsoleMode(true); }
    void Output(const ScriptBytes& bytes,bool error)
    {
        EnsureConsole();
        if(host.writeOutput) host.writeOutput(bytes,error);
        else {
            FILE* stream=error?stderr:stdout;
            if(!bytes.empty()&&std::fwrite(bytes.data(),1,bytes.size(),stream)!=bytes.size()) throw std::runtime_error("Cannot write standard stream.");
            if(std::fflush(stream)!=0) throw std::runtime_error("Cannot flush standard stream.");
        }
    }
    ScriptBytes Input(std::size_t count,bool all)
    {
        EnsureConsole();
        if(host.readInput) {
            auto bytes=host.readInput(count,all);
            if(bytes.size()>ScriptFiles::MaxReadSize||(!all&&bytes.size()>count)) throw std::runtime_error("Invalid input size.");
            return bytes;
        }
        ScriptBytes result;std::uint8_t chunk[8192];
        while(all||result.size()<count) {
            const auto request=all?sizeof(chunk):(std::min)(sizeof(chunk),count-result.size());
            const auto received=std::fread(chunk,1,request,stdin);
            if(received>ScriptFiles::MaxReadSize-result.size()) throw std::runtime_error("Input exceeds 32 MiB per call.");
            result.insert(result.end(),chunk,chunk+received);
            if(received<request) {if(std::ferror(stdin))throw std::runtime_error("Cannot read standard input.");break;}
        }
        return result;
    }
    static JSValue Dispatch(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv,int operation)
    {
        auto& self=*static_cast<Impl*>(JS_GetContextOpaque(ctx));
        if(self.exitRequested) return JS_ThrowInternalError(ctx,"ScriptDeck exit requested.");
        HostTime hostTime(self);
        const auto a=Argument(argc,argv,0),b=Argument(argc,argv,1),c=Argument(argc,argv,2);
        try {
            JSValue result=JS_UNDEFINED;
            switch(operation) {
            case GetHomePath: case GetDeckPath: case GetExePath: case GetDocumentPath: case GetTempPath: case GetAppDataPath: {
                std::filesystem::path path;
                if(operation==GetDeckPath) {
                    if(!self.host.getDeckPath)throw std::runtime_error("Deck path service is unavailable.");
                    path=self.host.getDeckPath();
                } else path=SystemPath(operation);
                const auto text=path.empty()?std::string{}:PathToUtf8(std::filesystem::absolute(path).lexically_normal());
                result=JS_NewStringLen(ctx,text.data(),text.size());break;
            }
            case ReadClipboard: {
                if(!self.host.readClipboard)throw std::runtime_error("Clipboard service is unavailable.");
                const auto text=self.host.readClipboard();
                result=JS_NewStringLen(ctx,text.data(),text.size());break;
            }
            case WriteClipboard: {
                const auto text=Text(ctx,a);
                if(text.find('\0')!=std::string::npos)throw std::invalid_argument("Clipboard text cannot contain NUL.");
                if(text.size()>ScriptFiles::MaxReadSize)throw std::invalid_argument("Clipboard text exceeds 32 MiB.");
                if(!self.host.writeClipboard)throw std::runtime_error("Clipboard service is unavailable.");
                self.host.writeClipboard(text);break;
            }
            case GetArgs: {
                result=JS_NewArray(ctx);if(JS_IsException(result))return result;
                for(std::size_t i=0;i<self.host.args.size();++i)
                    if(JS_SetPropertyUint32(ctx,result,static_cast<uint32_t>(i),JS_NewStringLen(ctx,self.host.args[i].data(),self.host.args[i].size()))<0){JS_FreeValue(ctx,result);return JS_EXCEPTION;}
                break;
            }
            case OpenDialog: case SaveDialog: {
                JSValue encoded=JS_UNDEFINED;
                nlohmann::json raw=nlohmann::json::object();
                if(!JS_IsUndefined(a)) {
                    if(!JS_IsObject(a)||JS_IsArray(a))throw std::invalid_argument("File dialog options must be an object.");
                    encoded=JS_JSONStringify(ctx,a,JS_UNDEFINED,JS_UNDEFINED);
                    if(JS_IsException(encoded))return JS_EXCEPTION;
                    try {raw=nlohmann::json::parse(Text(ctx,encoded));JS_FreeValue(ctx,encoded);}
                    catch(...) {JS_FreeValue(ctx,encoded);throw;}
                }
                const auto options=ParseFileDialogOptions(raw,operation==SaveDialog,self.files);
                if(operation==OpenDialog) {
                    if(!self.host.openFileDialog)throw std::runtime_error("OpenFileDialog service is unavailable.");
                    const auto paths=self.host.openFileDialog(options);
                    if(paths.empty())result=JS_NULL;
                    else if(options.multiple) {
                        result=JS_NewArray(ctx);if(JS_IsException(result))return result;
                        for(std::size_t i=0;i<paths.size();++i) {
                            const auto path=PathToUtf8(paths[i]);
                            if(JS_SetPropertyUint32(ctx,result,static_cast<uint32_t>(i),JS_NewStringLen(ctx,path.data(),path.size()))<0){JS_FreeValue(ctx,result);return JS_EXCEPTION;}
                        }
                    } else {const auto path=PathToUtf8(paths.front());result=JS_NewStringLen(ctx,path.data(),path.size());}
                } else {
                    if(!self.host.saveFileDialog)throw std::runtime_error("SaveFileDialog service is unavailable.");
                    const auto selected=self.host.saveFileDialog(options);
                    if(!selected)result=JS_NULL;
                    else {const auto path=PathToUtf8(*selected);result=JS_NewStringLen(ctx,path.data(),path.size());}
                }
                break;
            }
            case Write: case WriteLine: case WriteError: {
                auto text=JS_IsUndefined(a)?std::string{}:Text(ctx,a);
                if(operation!=Write)text+='\n';
                self.Output(ScriptBytes(text.begin(),text.end()),operation==WriteError);break;
            }
            case ReadBytes: {
                if(JS_IsUndefined(a))throw std::invalid_argument("readBytes requires count.");
                const auto count=Integer(ctx,a,0,ScriptFiles::MaxReadSize);result=ByteArray(ctx,self.Input(count,false));break;
            }
            case ReadBinary: result=ByteArray(ctx,self.Input(0,true));break;
            case WriteBytes: case WriteBinary: {
                auto bytes=Bytes(ctx,a,operation==WriteBinary);
                const auto offset=operation==WriteBytes?Integer(ctx,b,0,bytes.size()):0;
                const auto count=operation==WriteBytes?Integer(ctx,c,bytes.size()-offset,bytes.size()-offset):bytes.size();
                self.Output(ScriptBytes(bytes.begin()+static_cast<std::ptrdiff_t>(offset),bytes.begin()+static_cast<std::ptrdiff_t>(offset+count)),false);break;
            }
            case Flush:
                if(self.host.flushOutput)self.host.flushOutput();else if(std::fflush(stdout)!=0)throw std::runtime_error("Cannot flush stdout.");break;
            case Console: case Magic: {
                if(JS_IsUndefined(a))throw std::invalid_argument("A boolean argument is required.");
                const bool enabled=Boolean(a);const auto& callback=operation==Console?self.host.setConsoleMode:self.host.setMagic;
                if(!callback)throw std::runtime_error("Host mode service is unavailable.");
                callback(enabled);break;
            }
            case BeepOp: {
                const int frequency=static_cast<int>(Integer(ctx,a,800,32767));
                const int duration=static_cast<int>(Integer(ctx,b,200,60000));
                if(frequency<37)throw std::invalid_argument("Beep frequency must be 37..32767 Hz.");
                if(!self.host.beep)throw std::runtime_error("Beep service is unavailable.");
                self.host.beep(frequency,duration);break;
            }
            case PlayWav:
                if(!self.host.playWav)throw std::runtime_error("WAV service is unavailable.");
                self.host.playWav(self.files.Resolve(Text(ctx,a)),Boolean(b));break;
            case StopWav:
                if(!self.host.stopWav)throw std::runtime_error("WAV service is unavailable.");
                self.host.stopWav();break;
            case Exit: {
                const auto code=Integer(ctx,a,0,2147483647);
                self.exitCode=static_cast<int>(code);self.exitRequested=true;
                return JS_ThrowInternalError(ctx,"ScriptDeck exit requested.");
            }
            case ReadText: {const auto text=self.files.ReadText(Text(ctx,a));result=JS_NewStringLen(ctx,text.data(),text.size());break;}
            case FileWriteText: case AppendText:
                self.files.WriteText(Text(ctx,a),Text(ctx,b),Boolean(c),operation==AppendText);break;
            case FileReadBytes: result=ByteArray(ctx,self.files.ReadBytes(Text(ctx,a)));break;
            case FileWriteBytes: self.files.WriteBytes(Text(ctx,a),Bytes(ctx,b));break;
            case Exists: case ExistsFile: case ExistsDir:
                result=JS_NewBool(ctx,self.files.Exists(Text(ctx,a),operation==Exists?0:operation==ExistsFile?1:2));break;
            case ResolvePath: {const auto path=PathToUtf8(self.files.Resolve(Text(ctx,a)));result=JS_NewStringLen(ctx,path.data(),path.size());break;}
            case GetFiles: case GetDirectories: {
                const auto paths=self.files.List(Text(ctx,a),operation==GetDirectories);
                result=JS_NewArray(ctx);if(JS_IsException(result))return result;
                for(std::size_t i=0;i<paths.size();++i)
                    if(JS_SetPropertyUint32(ctx,result,static_cast<uint32_t>(i),JS_NewStringLen(ctx,paths[i].data(),paths[i].size()))<0){JS_FreeValue(ctx,result);return JS_EXCEPTION;}
                break;
            }
            case Move:self.files.Move(Text(ctx,a),Text(ctx,b));break;
            case Rename:self.files.Rename(Text(ctx,a),Text(ctx,b));break;
            case Delete:self.files.Delete(Text(ctx,a),Boolean(b));break;
            case GetName: case GetNameWithoutExt: case GetExt: case GetParent: case GetFrame: case GetNameWithoutFrame: {
                const auto path=Text(ctx,a);
                const auto separator=path.find_last_of("/\\");
                const auto name=path.substr(separator==std::string::npos?0:separator+1);
                const auto dot=name.find_last_of('.');
                const bool hasExt=dot!=std::string::npos && dot!=0;
                const auto stem=hasExt?name.substr(0,dot):name;
                auto frameStart=stem.size();
                while(frameStart>0 && stem[frameStart-1]>='0' && stem[frameStart-1]<='9')--frameStart;
                std::string value;
                if(operation==GetName)value=name;
                else if(operation==GetNameWithoutExt)value=stem;
                else if(operation==GetExt)value=hasExt?name.substr(dot):"";
                else if(operation==GetFrame)value=stem.substr(frameStart);
                else if(operation==GetNameWithoutFrame)value=stem.substr(0,frameStart);
                else if(separator!=std::string::npos) {
                    // Preserve a drive root (C:\) or a rooted slash.
                    const auto length=separator==0?1:separator==2 && path[1]==':'?3:separator;
                    value=path.substr(0,length);
                }
                result=JS_NewStringLen(ctx,value.data(),value.size());break;
            }
            case ToWindowsPath: case ToUnixPath: {
                auto path=Text(ctx,a);
                const auto isDrive=[](char c){return (c>='A' && c<='Z') || (c>='a' && c<='z');};
                // Lexical conversion: do not resolve relative paths or touch the filesystem.
                std::replace(path.begin(),path.end(),'\\','/');
                if(operation==ToWindowsPath) {
                    if(path.size()>=2 && path[0]=='/' && isDrive(path[1]) && (path.size()==2 || path[2]=='/')) {
                        char drive=path[1];if(drive>='a' && drive<='z')drive=static_cast<char>(drive-'a'+'A');
                        path=std::string(1,drive)+":"+(path.size()==2?"/":path.substr(2));
                    }
                    std::replace(path.begin(),path.end(),'/','\\');
                } else if(path.size()>=3 && isDrive(path[0]) && path[1]==':' && path[2]=='/') {
                    char drive=path[0];if(drive>='A' && drive<='Z')drive=static_cast<char>(drive-'A'+'a');
                    path="/"+std::string(1,drive)+path.substr(2);
                }
                result=JS_NewStringLen(ctx,path.data(),path.size());break;
            }
            case NextCard: case PrevCard: case TopCard: case EndCard: case GoCardIndex: case GoCard: {
                if(!self.host.navigateCard)throw std::runtime_error("Card navigation service is unavailable.");
                nlohmann::json target=nullptr;
                if(operation==GoCardIndex) {
                    if(JS_IsUndefined(a))throw std::invalid_argument("Card index is required.");
                    target=Integer(ctx,a,0,2147483647);
                } else if(operation==GoCard)target=Text(ctx,a);
                const char* action=operation==NextCard?"next":operation==PrevCard?"previous":operation==TopCard?"top":operation==EndCard?"end":operation==GoCardIndex?"index":"nameOrId";
                result=JS_NewBool(ctx,self.host.navigateCard(action,target));break;
            }
            case GoHome:
                if(!self.host.goHome)throw std::runtime_error("Deck navigation service is unavailable.");
                self.host.goHome(Boolean(a,true));break;
            case ChangeDeck:
                if(!self.host.changeDeck)throw std::runtime_error("Deck navigation service is unavailable.");
                self.host.changeDeck(self.files.Resolve(Text(ctx,a)),Boolean(b,true));break;
            case OpenDeck: case SaveDeck: {
                const auto path=JS_IsUndefined(a)?std::filesystem::path{}:self.files.Resolve(Text(ctx,a));
                if(!JS_IsUndefined(a) && Text(ctx,a).empty())throw std::invalid_argument("Deck path is empty.");
                const auto& callback=operation==OpenDeck?self.host.openDeck:self.host.saveDeck;
                if(!callback)throw std::runtime_error("Deck file service is unavailable.");
                callback(path);break;
            }
            case SaveAsDeck:
                if(!self.host.saveAsDeck)throw std::runtime_error("Deck save dialog service is unavailable.");
                result=JS_NewBool(ctx,self.host.saveAsDeck());break;
            case Install: case Uninstall: {
                const auto& callback=operation==Install?self.host.install:self.host.uninstall;
                if(!callback)throw std::runtime_error("File association service is unavailable.");
                result=JS_NewBool(ctx,callback());break;
            }
            case SetTopMost:
                if(JS_IsUndefined(a))throw std::invalid_argument("A boolean argument is required.");
                if(!self.host.setTopMost)throw std::runtime_error("Window service is unavailable.");
                self.host.setTopMost(Boolean(a));break;
            case GetTopMost:
                if(!self.host.getTopMost)throw std::runtime_error("Window service is unavailable.");
                result=JS_NewBool(ctx,self.host.getTopMost());break;
            case WindowFront:
                if(!self.host.windowFront)throw std::runtime_error("Window service is unavailable.");
                self.host.windowFront();break;
            default:throw std::runtime_error("Unknown builtin operation.");
            }
            return result;
        } catch(const std::invalid_argument& error){return JS_ThrowTypeError(ctx,"%s",error.what());}
        catch(const std::filesystem::filesystem_error& error){
            const std::string message = "Filesystem error " + std::to_string(error.code().value()) + ": " +
                PathToUtf8(error.path1()) + (error.path2().empty() ? "" : " -> " + PathToUtf8(error.path2()));
            return JS_ThrowInternalError(ctx,"%s",message.c_str());
        }
        catch(const std::exception& error){return JS_ThrowInternalError(ctx,"%s",error.what());}
        catch(...){return JS_ThrowInternalError(ctx,"Builtin operation failed.");}
    }
    void RegisterBuiltins()
    {
        struct Entry{const char* name;int operation;int length;};
        const Entry apps[]={ {"readClipboard",ReadClipboard,0},{"writeClipboard",WriteClipboard,1},{"getHomePath",GetHomePath,0},{"getDeckPath",GetDeckPath,0},{"getExePath",GetExePath,0},
            {"getDocumentPath",GetDocumentPath,0},{"getDocumentsPath",GetDocumentPath,0},{"getTempPath",GetTempPath,0},{"getAppDataPath",GetAppDataPath,0},{"getArgs",GetArgs,0},{"openFileDialog",OpenDialog,0},{"saveFileDialog",SaveDialog,0},{"write",Write,1},{"writeLine",WriteLine,1},{"writeError",WriteError,1},
            {"readBytes",ReadBytes,1},{"writeBytes",WriteBytes,1},{"readBinary",ReadBinary,0},{"writeBinary",WriteBinary,1},
            {"install",Install,0},{"uninstall",Uninstall,0},{"goHome",GoHome,0},{"nextCard",NextCard,0},{"prevCard",PrevCard,0},{"topCard",TopCard,0},{"endCard",EndCard,0},{"goCardIndex",GoCardIndex,1},{"goCard",GoCard,1},{"changeDeck",ChangeDeck,1},{"openDeck",OpenDeck,0},{"saveDeck",SaveDeck,0},{"saveAsDeck",SaveAsDeck,0},{"setTopMost",SetTopMost,1},{"getTopMost",GetTopMost,0},{"windowFront",WindowFront,0},{"flush",Flush,0},{"setConsoleMode",Console,1},{"setMagic",Magic,1},{"beep",BeepOp,0},{"playWav",PlayWav,1},{"stopWav",StopWav,0},{"exit",Exit,0}};
        const Entry fs[]={ {"readText",ReadText,1},{"writeText",FileWriteText,2},{"appendText",AppendText,2},{"readBytes",FileReadBytes,1},{"writeBytes",FileWriteBytes,2},
            {"exists",Exists,1},{"existsFile",ExistsFile,1},{"existsDir",ExistsDir,1},{"resolvePath",ResolvePath,1},
            {"toWindowsPath",ToWindowsPath,1},{"toUnixPath",ToUnixPath,1},{"getName",GetName,1},{"getNameWithoutExt",GetNameWithoutExt,1},{"getExt",GetExt,1},{"getParent",GetParent,1},{"getFrame",GetFrame,1},{"getNameWithoutFrame",GetNameWithoutFrame,1},{"getFiles",GetFiles,1},{"getDirectories",GetDirectories,1},{"move",Move,2},{"rename",Rename,2},{"delete",Delete,1}};
        auto add=[&](const char* name,const Entry* entries,std::size_t count){
            JSValue object=JS_NewObject(context);
            if(JS_IsException(object))throw std::runtime_error("Cannot allocate API object.");
            for(std::size_t i=0;i<count;++i)if(JS_SetPropertyStr(context,object,entries[i].name,
                    JS_NewCFunctionMagic(context,Dispatch,entries[i].name,entries[i].length,JS_CFUNC_generic_magic,entries[i].operation))<0){JS_FreeValue(context,object);throw std::runtime_error("Cannot register builtin.");}
            JSValue global=JS_GetGlobalObject(context);const int status=JS_SetPropertyStr(context,global,name,object);JS_FreeValue(context,global);
            if(status<0)throw std::runtime_error("Cannot publish API object.");
        };
        add("app",apps,sizeof(apps)/sizeof(apps[0]));add("fs",fs,sizeof(fs)/sizeof(fs[0]));
        JSValue global=JS_GetGlobalObject(context);
        for(const auto& entry:fs)if(entry.operation>=GetName && entry.operation<=ToUnixPath)
            JS_SetPropertyStr(context,global,entry.name,JS_NewCFunctionMagic(context,Dispatch,entry.name,entry.length,JS_CFUNC_generic_magic,entry.operation));
        for(const auto& entry:apps)if(entry.operation>=NextCard && entry.operation<=WindowFront && entry.operation!=Install && entry.operation!=Uninstall)
            JS_SetPropertyStr(context,global,entry.name,JS_NewCFunctionMagic(context,Dispatch,entry.name,entry.length,JS_CFUNC_generic_magic,entry.operation));
        JS_FreeValue(context,global);
    }

    std::string Exception(JSContext* ctx)
    {
        // タイムアウト後でもエラーメッセージ取得を可能にする。
        ResetDeadline();
        JSValue error = JS_GetException(ctx);
        const char* message = JS_ToCString(ctx, error);
        std::string result = message ? message : "JavaScript execution failed.";
        if (message) JS_FreeCString(ctx, message);
        else { JSValue nested = JS_GetException(ctx); JS_FreeValue(ctx, nested); }
        JSValue stack = JS_GetPropertyStr(ctx, error, "stack");
        if (JS_IsString(stack)) {
            message = JS_ToCString(ctx, stack);
            if (message) {
                const std::string trace = message;
                if (trace.find(result) == 0) result = trace;
                else result += "\n" + trace;
                JS_FreeCString(ctx, message);
            }
        }
        if (JS_IsException(stack)) { JSValue nested = JS_GetException(ctx); JS_FreeValue(ctx, nested); }
        JS_FreeValue(ctx, stack); JS_FreeValue(ctx, error);
        if (timedOut) result = "Script time limit exceeded (2 seconds).\n" + result;
        return result;
    }
    static JSValue ModelAccess(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
    {
        auto& self = *static_cast<Impl*>(JS_GetContextOpaque(ctx));
        if(self.exitRequested) return JS_ThrowInternalError(ctx,"ScriptDeck exit requested.");
        HostTime hostTime(self);
        try {
            if (!self.host.model) throw std::runtime_error("Deck access is unavailable in this host.");
            if (argc != 6) throw std::invalid_argument("Invalid model arguments.");
            const auto op=Text(ctx,argv[0]),kind=Text(ctx,argv[1]),card=Text(ctx,argv[2]),id=Text(ctx,argv[3]),key=Text(ctx,argv[4]);
            const auto value=nlohmann::json::parse(Text(ctx,argv[5]));
            const auto result=self.host.model->Access(op,kind,card,id,key,value).dump();
            return JS_NewStringLen(ctx,result.data(),result.size());
        } catch (const std::out_of_range& e) { return JS_ThrowRangeError(ctx,"%s",e.what()); }
        catch (const std::invalid_argument& e) { return JS_ThrowTypeError(ctx,"%s",e.what()); }
        catch (const std::exception& e) { return JS_ThrowInternalError(ctx,"%s",e.what()); }
    }
    void Run(const std::string& code, const std::string& source, bool invoke = false, JSValueConst receiver = JS_UNDEFINED)
    {
        if (code.empty() || exitRequested) return;
        // 呼び出しごとに現在のネイティブスタック位置を基準へ更新する。
        JS_UpdateStackTop(runtime);
        timedOut = false; ResetDeadline();
        JSValue result = JS_Eval(context, code.c_str(), code.size(), source.c_str(), JS_EVAL_TYPE_GLOBAL);
        if(invoke && !JS_IsException(result)) {
            JSValue call = JS_Call(context,result,receiver,0,nullptr); JS_FreeValue(context,result); result=call;
        }
        if(exitRequested) {JS_FreeValue(context,result);JSValue ignored=JS_GetException(context);JS_FreeValue(context,ignored);return;}
        if (JS_IsException(result)) throw std::runtime_error(source + ":\n" + Exception(context));
        JS_FreeValue(context, result);
        JSContext* jobContext = nullptr;
        int jobs = 0;
        while ((jobs = JS_ExecutePendingJob(runtime, &jobContext)) > 0) {
            if (std::chrono::steady_clock::now() >= deadline) { timedOut = true; throw std::runtime_error("Script time limit exceeded (2 seconds)."); }
        }
        if(exitRequested){JSValue ignored=JS_GetException(jobContext?jobContext:context);JS_FreeValue(jobContext?jobContext:context,ignored);return;}
        if (jobs < 0) throw std::runtime_error(Exception(jobContext ? jobContext : context));
    }
};

ScriptEngine::ScriptEngine(AlertCallback alert, ScriptHost host) : impl_(std::make_unique<Impl>(std::move(alert),std::move(host)))
{
    impl_->RegisterBuiltins();
    {
        JSContext* ctx=impl_->context;
        JSValue global=JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx,global,"__scriptDeckModel",JS_NewCFunction(ctx,Impl::ModelAccess,"modelAccess",6));
        JS_FreeValue(ctx,global);
        JS_UpdateStackTop(impl_->runtime); impl_->ResetDeadline();
        const std::string code=R"MODEL((function(bridge) {
    const properties = {
        deck: ['name','script','width','height','currentCardId','cardCount','startupPlacement'],
        card: ['id','name','script','width','height','objectCount','backgroundColor','enterButtonId','escapeButtonId'],
        object: ['id','type','name','text','x','y','width','height','visible','enabled','script','imagePath','imageSource','resourceId','backgroundColor','textColor','borderColor','tintColor']
    };
    const listProperties=['items','itemCount','selectedIndex','selectedText'];
    const listMethods=['getItem','setItem','addItem','insertItem','removeItem','clearItems'];
    const cache = new Map();
    const identities = new WeakMap();
    function access(op,kind,card,id,key,value) {
        const encoded=JSON.stringify(value);
        return JSON.parse(bridge(op,kind,card,id,key,encoded === undefined ? 'null' : encoded));
    }
    function initialProperties(value) {
        if(value===null||typeof value!=='object'||Array.isArray(value))throw new TypeError('Initial properties must be an object.');
        const result=Object.create(null);
        for(const key of Reflect.ownKeys(value)) {
            const item=value[key];
            if(typeof key!=='string'||item===undefined||typeof item==='function'||typeof item==='symbol')throw new TypeError('Invalid initial property.');
            result[key]=item;
        }
        return result;
    }
    function reference(kind,card='',id='') {
        const cacheKey=JSON.stringify([kind,card,id]);
        if(cache.has(cacheKey))return cache.get(cacheKey);
        function read(key) { return access('get',kind,card,id,key); }
        function keys() {
            let result=properties[kind];
            if(kind==='object') {
                const type=read('type');
                if(['listbox','dropdownlist'].includes(type))result=result.concat(listProperties);
                if(['checkbox','radiobutton'].includes(type))result=result.concat(['checked']);
                if(type==='radiobutton')result=result.concat(['group']);
            }
            return result;
        }
        const proxy=new Proxy({}, {
            get(_,key) {
                if(typeof key !== 'string')return undefined;
                if(key==='toJSON')return function(){const data={};for(const key of keys())data[key]=read(key);return data;};
                if(key==='card'&&kind==='object')return reference('card',card);
                if(kind==='deck' && (key==='cardByName'||key==='cardById'))return function(value) {
                    const found=access('findCard',kind,card,id,key==='cardById'?'id':'name',value);
                    return found===null?null:reference('card',found);
                };
                if(kind==='card' && (key==='objectByName'||key==='objectById'))return function(value) {
                    const found=access('findObject',kind,card,id,key==='objectById'?'id':'name',value);
                    return found===null?null:reference('object',card,found);
                };
                if(kind==='card' && key==='objects')return access('objects',kind,card,id,'').map(id=>reference('object',card,id));
                if(kind==='card' && key==='objectAt')return function(index) {
                    const ids=access('objects',kind,card,id,'');
                    if(!Number.isInteger(index)||index<0||index>=ids.length)throw new RangeError('Object index out of range.');
                    return reference('object',card,ids[index]);
                };
                if(kind==='card' && key==='createObject')return function(type,properties={}) {
                    return reference('object',card,access('create',kind,card,'',type,initialProperties(properties)));
                };
                if(kind==='card' && key==='removeObject')return function(target) {
                    let targetId=target;
                    if(typeof target!=='string') {
                        const identity=identities.get(target);
                        if(!identity||identity.kind!=='object'||identity.card!==card)throw new TypeError('Expected an object on this card or its ID.');
                        targetId=identity.id;
                    }
                    const removed=access('remove',kind,card,targetId,'');
                    if(removed)cache.delete(JSON.stringify(['object',card,targetId]));
                    return removed;
                };
                if(kind==='object' && key==='exists')return access('exists',kind,card,id,'');
                if(kind==='object' && key==='index')return read('index');
                if(kind==='object' && key==='remove')return function(){const removed=access('remove',kind,card,id,'');cache.delete(cacheKey);return removed;};
                if(kind==='object' && key==='clone')return function(properties={}){return reference('object',card,access('clone',kind,card,id,'',initialProperties(properties)));};
                if(kind==='object' && key==='moveToIndex')return function(index){access('order',kind,card,id,'',index);};
                if(kind==='object' && key==='bringToFront')return function(){access('order',kind,card,id,'',reference('card',card).objectCount-1);};
                if(kind==='object' && key==='sendToBack')return function(){access('order',kind,card,id,'',0);};
                if(kind==='object' && listMethods.includes(key))return function(...args){return access('call',kind,card,id,key,args);};
                if(keys().includes(key))return read(key);
                return undefined;
            },
            set(_,key,value) {
                if(value === undefined || typeof value === 'function' || typeof value === 'symbol')throw new TypeError('Invalid property value.');
                if(typeof key!=='string'||!keys().includes(key))throw new TypeError('Unknown property: '+String(key));
                access('set',kind,card,id,key,value);return true;
            },
            ownKeys(){return keys();},
            getOwnPropertyDescriptor(_,key){if(keys().includes(key))return {enumerable:true,configurable:true};},
            defineProperty(){throw new TypeError('Use property assignment.');},
            deleteProperty(){throw new TypeError('Properties cannot be deleted.');},
            preventExtensions(){throw new TypeError('Live references cannot be frozen.');},
            setPrototypeOf(){throw new TypeError('Live references cannot change prototype.');}
        });
        identities.set(proxy,{kind,card,id});
        cache.set(cacheKey,proxy);return proxy;
    }
    Object.defineProperty(app,'deck',{get(){return reference('deck');}});
    Object.defineProperty(app,'currentCard',{get(){return reference('card',reference('deck').currentCardId);}});
    delete globalThis.__scriptDeckModel;
    return {card:(id)=>reference('card',id),object:(card,id)=>reference('object',card,id)};
})(globalThis.__scriptDeckModel)
)MODEL";
        JSValue factories=JS_Eval(ctx,code.c_str(),code.size(),"ScriptDeck-model.js",JS_EVAL_TYPE_GLOBAL);
        if(JS_IsException(factories))throw std::runtime_error(impl_->Exception(ctx));
        impl_->cardFactory=JS_GetPropertyStr(ctx,factories,"card");
        impl_->objectFactory=JS_GetPropertyStr(ctx,factories,"object");
        JS_FreeValue(ctx,factories);
    }

    impl_->Run(R"JS(
app.runCode = function(code) {
    "use strict";
    if (typeof code !== "string") throw new TypeError("runCode requires a string.");
    return eval(code);
};
)JS", "ScriptDeck-runCode.js");
    JSValue global = JS_GetGlobalObject(impl_->context);
    const int registered = JS_SetPropertyStr(impl_->context, global, "__scriptDeckAlert",
        JS_NewCFunction(impl_->context, Impl::Alert, "alert", 1));
    JS_FreeValue(impl_->context, global);
    if (registered < 0) throw std::runtime_error("Cannot register alert.");
    impl_->Run(R"JS(
(function(nativeAlert) {
    globalThis.alert = function(value) {
        let text;
        if (value !== null && typeof value === "object") {
            const seen = new WeakSet();
            try {
                text = JSON.stringify(value, function(key, item) {
                    if (typeof item === "bigint") return String(item) + "n";
                    if (typeof item === "undefined") return "[undefined]";
                    if (typeof item === "function" || typeof item === "symbol") return String(item);
                    if (item !== null && typeof item === "object") {
                        if (seen.has(item)) return "[Circular/shared reference]";
                        seen.add(item);
                    }
                    return item;
                }, 2);
            } catch (_) {
                try { text = String(value); } catch (_) { text = "[Object cannot be formatted]"; }
            }
        } else text = String(value);
        if (typeof text !== "string") text = String(value);
        // ネイティブダイアログの終端文字として扱われないよう可視化する。
        nativeAlert(text);
    };
    delete globalThis.__scriptDeckAlert;
})(globalThis.__scriptDeckAlert);
)JS", "ScriptDeck-alert.js");
}
ScriptEngine::~ScriptEngine() = default;
void ScriptEngine::RunGlobal(const std::string& code, const std::string& sourceName) { impl_->Run(code, sourceName); }
void ScriptEngine::RunScoped(const std::string& code, const std::string& sourceName, const std::string& handler,
                             const std::string& cardId, const std::string& objectId, const nlohmann::json& eventData)
{
    if (code.empty() || impl_->exitRequested) return;
    if (!eventData.is_object()) throw std::invalid_argument("Event data must be an object.");
    if (handler != "mouseUp" && handler != "openCard" && handler != "change" && handler != "dropFiles") throw std::runtime_error("Unsupported script handler.");
    JSContext* ctx=impl_->context;
    JSValue receiver=JS_UNDEFINED;
    if (!cardId.empty()) {
        JS_UpdateStackTop(impl_->runtime); impl_->timedOut=false; impl_->ResetDeadline();
        JSValue args[]={JS_NewString(ctx,cardId.c_str()),JS_NewString(ctx,objectId.c_str())};
        receiver=JS_Call(ctx,objectId.empty()?impl_->cardFactory:impl_->objectFactory,JS_UNDEFINED,objectId.empty()?1:2,args);
        for(auto value:args)JS_FreeValue(ctx,value);
        if(JS_IsException(receiver))throw std::runtime_error(impl_->Exception(ctx));
    }
    try {
        impl_->Run("(function(){\n\"use strict\";\n" + code + "\n;if(typeof " + handler + " === 'function') " +
            handler + ".call(this,{..." + eventData.dump() + ",type:'" + handler + "',target:this});\n})", sourceName,true,receiver);
    } catch (...) { JS_FreeValue(ctx,receiver); throw; }
    JS_FreeValue(ctx,receiver);
}

bool ScriptEngine::ExitRequested() const { return impl_->exitRequested; }
int ScriptEngine::ExitCode() const { return impl_->exitCode; }
void ScriptEngine::ClearExitRequest() { impl_->exitRequested = false; }
