#pragma once
#include "ScriptIO.h"
#include "Utf8Path.h"
#include "nlohmann/json.hpp"
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cerrno>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#endif
namespace ScriptSystem {
inline void Validate(const std::string& value) {
    if(value.find('\0')!=std::string::npos)throw std::invalid_argument("NUL is not allowed.");
}
inline std::optional<std::string> Environment(const std::string& name) {
    Validate(name);
    if(name.empty()||name.find('=')!=std::string::npos)throw std::invalid_argument("Invalid environment variable name.");
#ifdef _WIN32
    const auto wide=PathFromUtf8(name).native();
    for(;;) {
        SetLastError(ERROR_SUCCESS);
        const auto size=GetEnvironmentVariableW(wide.c_str(),nullptr,0);
        if(!size) {
            if(GetLastError()==ERROR_ENVVAR_NOT_FOUND)return std::nullopt;
            if(GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("Cannot read environment variable.");
            return std::string{};
        }
        std::wstring value(size,L'\0');
        SetLastError(ERROR_SUCCESS);
        const auto length=GetEnvironmentVariableW(wide.c_str(),value.data(),size);
        if(length>=size)continue;
        if(!length && GetLastError()==ERROR_ENVVAR_NOT_FOUND)return std::nullopt;
        if(!length&&GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("Cannot read environment variable.");
        value.resize(length);return PathToUtf8(std::filesystem::path(value));
    }
#else
    const auto* value=std::getenv(name.c_str());return value?std::optional<std::string>(value):std::nullopt;
#endif
}
inline nlohmann::json FileInfo(const std::filesystem::path& path) {
    nlohmann::json result={{"path",PathToUtf8(path)},{"createdTime",nullptr}};
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&data))throw std::runtime_error("Cannot read file attributes: "+PathToUtf8(path));
    const auto time=[](FILETIME value)->double {
        ULARGE_INTEGER ticks;ticks.LowPart=value.dwLowDateTime;ticks.HighPart=value.dwHighDateTime;
        return static_cast<double>(ticks.QuadPart)/10000.0-11644473600000.0;
    };
    result["isDirectory"]=(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
    result["size"]=result["isDirectory"].get<bool>()?nlohmann::json(nullptr):nlohmann::json((static_cast<std::uint64_t>(data.nFileSizeHigh)<<32)|data.nFileSizeLow);
    result["createdTime"]=time(data.ftCreationTime);result["modifiedTime"]=time(data.ftLastWriteTime);result["accessedTime"]=time(data.ftLastAccessTime);
#else
    struct stat data{};
    if(stat(path.c_str(),&data)!=0)throw std::runtime_error("Cannot read file attributes: "+PathToUtf8(path));
    result["isDirectory"]=S_ISDIR(data.st_mode);
    result["size"]=S_ISREG(data.st_mode)?nlohmann::json(data.st_size):nlohmann::json(nullptr);
    result["modifiedTime"]=static_cast<double>(data.st_mtim.tv_sec)*1000+data.st_mtim.tv_nsec/1000000.0;
    result["accessedTime"]=static_cast<double>(data.st_atim.tv_sec)*1000+data.st_atim.tv_nsec/1000000.0;
#endif
    return result;
}
// Quote an argv element using the Windows C runtime parsing rules.
inline std::wstring Quote(const std::wstring& value) {
    std::wstring result=L"\"";std::size_t slashes=0;
    for(const auto c:value) {
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'"';return result;
}
inline std::string Process(const std::string& executable,const std::vector<std::string>& arguments,const std::filesystem::path& directory,bool wait) {
    Validate(executable);if(executable.empty())throw std::invalid_argument("Executable is empty.");
    for(const auto& arg:arguments)Validate(arg);
    std::string output;
#ifdef _WIN32
    struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
    auto executablePath=PathFromUtf8(executable);
    if(executablePath.has_parent_path()&&executablePath.is_relative())executablePath=directory/executablePath;
    std::wstring command=Quote(executablePath.native());
    for(const auto& arg:arguments)command+=L" "+Quote(PathFromUtf8(arg).native());
    if(command.size()>=32767)throw std::invalid_argument("Process command line is too long.");
    Handle reader,writer,input,error,process,thread;
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);
    PROCESS_INFORMATION info{};
    DWORD flags=CREATE_NO_WINDOW;
    std::vector<unsigned char> attributes;
    struct AttributeCleanup {LPPROC_THREAD_ATTRIBUTE_LIST value=nullptr;~AttributeCleanup(){if(value)DeleteProcThreadAttributeList(value);}} cleanup;
    if(wait) {
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        if(!CreatePipe(&reader.value,&writer.value,&security,0)||!SetHandleInformation(reader.value,HANDLE_FLAG_INHERIT,0))throw std::runtime_error("Cannot create stdout pipe.");
        input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        error.value=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        if(input.value==INVALID_HANDLE_VALUE||error.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot prepare process streams.");
        SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);attributes.resize(size);
        startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&size))throw std::runtime_error("Cannot initialize process attributes.");
        cleanup.value=startup.lpAttributeList;
        HANDLE handles[]={writer.value,input.value,error.value};
        if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr))throw std::runtime_error("Cannot set inherited process handles.");
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdOutput=writer.value;startup.StartupInfo.hStdInput=input.value;startup.StartupInfo.hStdError=error.value;
        flags|=EXTENDED_STARTUPINFO_PRESENT;
    } else startup.StartupInfo.cb=sizeof(STARTUPINFOW);
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,wait?TRUE:FALSE,flags,nullptr,directory.c_str(),&startup.StartupInfo,&info))throw std::runtime_error("Cannot start process (Windows error "+std::to_string(GetLastError())+").");
    process.value=info.hProcess;thread.value=info.hThread;
    if(!wait)return {};
    CloseHandle(writer.value);writer.value=nullptr;
    bool exceeded=false;
    for(;;) {
        DWORD available=0;
        if(!PeekNamedPipe(reader.value,nullptr,0,nullptr,&available,nullptr)) {
            if(GetLastError()==ERROR_BROKEN_PIPE)break;
            throw std::runtime_error("Cannot read process stdout.");
        }
        if(available) {
            char buffer[8192];DWORD received=0;
            if(!ReadFile(reader.value,buffer,(std::min)(available,DWORD(sizeof(buffer))),&received,nullptr))throw std::runtime_error("Cannot read process stdout.");
            if(received>ScriptFiles::MaxReadSize-output.size())exceeded=true;
            else if(!exceeded)output.append(buffer,received);
        } else {
            const auto state=WaitForSingleObject(process.value,10);
            if(state==WAIT_OBJECT_0) {
                DWORD remaining=0;
                if(PeekNamedPipe(reader.value,nullptr,0,nullptr,&remaining,nullptr)&&remaining)continue;
                break;
            }
            if(state==WAIT_FAILED)throw std::runtime_error("Cannot wait for process.");
        }
    }
    if(WaitForSingleObject(process.value,INFINITE)!=WAIT_OBJECT_0)throw std::runtime_error("Cannot wait for process.");
    if(exceeded)throw std::runtime_error("Process stdout exceeds 32 MiB.");
#else
    std::vector<std::string> values;values.push_back(executable);values.insert(values.end(),arguments.begin(),arguments.end());
    if(PathFromUtf8(executable).has_parent_path()&&PathFromUtf8(executable).is_relative())values[0]=PathToUtf8(directory/PathFromUtf8(executable));
    std::vector<char*> argv;for(auto& value:values)argv.push_back(value.data());argv.push_back(nullptr);
    struct Fd {int value=-1;~Fd(){if(value>=0)close(value);}} reader,writer,errReader,errWriter;
    int errors[2];if(pipe(errors)!=0)throw std::runtime_error("Cannot create process error pipe.");
    errReader.value=errors[0];errWriter.value=errors[1];fcntl(errWriter.value,F_SETFD,FD_CLOEXEC);
    if(wait){int pipefd[2];if(pipe(pipefd)!=0)throw std::runtime_error("Cannot create stdout pipe.");reader.value=pipefd[0];writer.value=pipefd[1];}
    const auto child=fork();if(child<0)throw std::runtime_error("Cannot create process.");
    if(child==0) {
        close(errReader.value);
        const auto fail=[&]{int code=errno;write(errWriter.value,&code,sizeof(code));_exit(127);};
        if(!wait){const auto grandchild=fork();if(grandchild<0)fail();if(grandchild>0)_exit(0);setsid();}
        if(chdir(directory.c_str())!=0)fail();
        int nullfd=open("/dev/null",O_RDWR);if(nullfd<0)fail();
        if(dup2(nullfd,STDIN_FILENO)<0||dup2(nullfd,STDERR_FILENO)<0)fail();
        if(wait){close(reader.value);if(dup2(writer.value,STDOUT_FILENO)<0)fail();close(writer.value);}
        else if(dup2(nullfd,STDOUT_FILENO)<0)fail();
        close(nullfd);execvp(argv[0],argv.data());fail();
    }
    close(errWriter.value);errWriter.value=-1;
    if(wait){close(writer.value);writer.value=-1;}
    int execError=0;ssize_t errorSize;
    do{errorSize=read(errReader.value,&execError,sizeof(execError));}while(errorSize<0&&errno==EINTR);
    if(errorSize!=0){int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}throw std::runtime_error("Cannot start process (errno "+std::to_string(execError)+").");}
    if(!wait){int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}return {};}
    bool exceeded=false;char buffer[8192];
    for(;;){auto count=read(reader.value,buffer,sizeof(buffer));if(count<0&&errno==EINTR)continue;if(count<=0)break;if(static_cast<std::size_t>(count)>ScriptFiles::MaxReadSize-output.size())exceeded=true;else if(!exceeded)output.append(buffer,count);}
    int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}
    if(exceeded)throw std::runtime_error("Process stdout exceeds 32 MiB.");
#endif
    if(output.size()>=3&&output.compare(0,3,"\xef\xbb\xbf")==0)output.erase(0,3);
    return output;
}
}
