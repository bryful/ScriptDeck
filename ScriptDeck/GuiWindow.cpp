#include "GuiView.h"
#include "WindowState.h"
#include "AppScriptApi.h"
#include "ConsoleMode.h"
#include "ScriptEngine.h"
#include <memory>
#include "ImageResources.h"
#include "WindowsAssociation.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Utf8Path.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <map>
#include <vector>
#include <stdexcept>
#include <algorithm>
using Microsoft::WRL::ComPtr;
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")
struct PendingFileDrop { std::vector<std::string> paths; POINT position{}; };
static std::vector<PendingFileDrop> pendingFileDrops;
static bool closeRequested = false;
static bool fixedPlayer = false;
static int playerWidth = 0, playerHeight = 0;
static bool PlayerWindowRect(HWND hwnd, RECT& rectangle, UINT dpi = 0)
{
    const auto user32 = GetModuleHandleW(L"user32.dll");
    using GetDpi = UINT(WINAPI*)(HWND);
    using AdjustForDpi = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const auto getDpi = reinterpret_cast<GetDpi>(GetProcAddress(user32, "GetDpiForWindow"));
    const auto adjust = reinterpret_cast<AdjustForDpi>(GetProcAddress(user32, "AdjustWindowRectExForDpi"));
    if (!dpi) dpi = getDpi ? getDpi(hwnd) : 96;
    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    return adjust ? adjust(&rectangle, style, FALSE, 0, dpi) != FALSE :
                    AdjustWindowRectEx(&rectangle, style, FALSE, 0) != FALSE;
}
static UINT resizeWidth = 0, resizeHeight = 0;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, w, l)) return 1;
    if (msg == WM_DROPFILES) {
        const auto drop=reinterpret_cast<HDROP>(w);
        try {
            PendingFileDrop event;
            if(DragQueryPoint(drop,&event.position)) {
                const auto count=DragQueryFileW(drop,0xffffffff,nullptr,0);
                for(UINT i=0;i<count;++i) {
                    const auto length=DragQueryFileW(drop,i,nullptr,0);
                    std::vector<wchar_t> path(static_cast<std::size_t>(length)+1);
                    if(DragQueryFileW(drop,i,path.data(),length+1)!=length)throw std::runtime_error("Cannot obtain dropped file path.");
                    event.paths.push_back(PathToUtf8(std::filesystem::path(path.data())));
                }
                if(!event.paths.empty())pendingFileDrops.push_back(std::move(event));
            }
        } catch(const std::exception& error) {MessageBoxW(hwnd,PathFromUtf8(error.what()).c_str(),L"ScriptDeck - File drop error",MB_OK|MB_ICONERROR);}
        catch(...) {MessageBoxW(hwnd,L"Cannot receive dropped files.",L"ScriptDeck",MB_OK|MB_ICONERROR);}
        DragFinish(drop);return 0;
    }
    if (msg == WM_SIZE) {
        if (w != SIZE_MINIMIZED) { resizeWidth = LOWORD(l); resizeHeight = HIWORD(l); }
        return 0;
    }
    if (msg == WM_DPICHANGED && fixedPlayer && playerWidth > 0) {
        const auto* suggested = reinterpret_cast<const RECT*>(l);
        RECT rectangle{0,0,playerWidth,playerHeight};
        if (PlayerWindowRect(hwnd, rectangle, HIWORD(w)))
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                rectangle.right-rectangle.left, rectangle.bottom-rectangle.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    if (msg == WM_CLOSE) { closeRequested = true; return 0; }
    if (msg == WM_SYSCOMMAND) {
        const auto command = w & 0xfff0;
        if (command == SC_KEYMENU || (fixedPlayer && (command == SC_SIZE || command == SC_MAXIMIZE))) return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
static std::filesystem::path ChooseFile(HWND hwnd, bool save, bool image = false)
{
    wchar_t path[32768] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = image ? L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0All files (*.*)\0*.*\0" :
        L"ScriptDeck (*.deck)\0*.deck\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path; ofn.nMaxFile = 32768;
    ofn.lpstrDefExt = image ? nullptr : L"deck";
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn)) return std::filesystem::path(path);
    return {};
}
class Images
{
    ID3D11Device* device_;
    ComPtr<IWICImagingFactory> factory_;
    std::map<std::filesystem::path, ComPtr<ID3D11ShaderResourceView>> cache_;
    std::map<std::string, ComPtr<ID3D11ShaderResourceView>> resources_;
    ComPtr<ID3D11ShaderResourceView> Decode(IWICBitmapDecoder* decoder)
    {
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        UINT width = 0, height = 0;
        if (!decoder || FAILED(decoder->GetFrame(0, &frame)) ||
            FAILED(frame->GetSize(&width, &height)) || !width || !height || width > 8192 || height > 8192 ||
            FAILED(factory_->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return srv;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);
        if (FAILED(converter->CopyPixels(nullptr, width*4, static_cast<UINT>(pixels.size()), pixels.data()))) return srv;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{}; data.pSysMem = pixels.data(); data.SysMemPitch = width*4;
        ComPtr<ID3D11Texture2D> texture;
        if (SUCCEEDED(device_->CreateTexture2D(&desc, &data, &texture)))
            device_->CreateShaderResourceView(texture.Get(), nullptr, &srv);
        return srv;
    }
    static ImTextureID TextureId(ID3D11ShaderResourceView* texture)
    {
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture));
    }
public:
    explicit Images(ID3D11Device* device) : device_(device)
    {
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory_));
    }
    ImTextureID Get(const std::filesystem::path& path)
    {
        auto it = cache_.find(path);
        if (it != cache_.end()) return TextureId(it->second.Get());
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<ID3D11ShaderResourceView> texture;
        if (factory_ && SUCCEEDED(factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnLoad, &decoder))) texture = Decode(decoder.Get());
        cache_.emplace(path, texture); return TextureId(texture.Get());
    }
    ImTextureID Resource(const std::string& id)
    {
        auto it = resources_.find(id);
        if (it != resources_.end()) return TextureId(it->second.Get());
        ComPtr<ID3D11ShaderResourceView> texture;
        for (const auto& definition : ImageResources) if (id == definition.id && factory_) {
            const auto module = GetModuleHandleW(nullptr);
            const auto resource = FindResourceW(module, MAKEINTRESOURCEW(definition.nativeId), MAKEINTRESOURCEW(10));
            const auto loaded = resource ? LoadResource(module, resource) : nullptr;
            auto* data = loaded ? static_cast<BYTE*>(LockResource(loaded)) : nullptr;
            const DWORD size = resource ? SizeofResource(module, resource) : 0;
            ComPtr<IWICStream> stream;
            ComPtr<IWICBitmapDecoder> decoder;
            if (data && size && SUCCEEDED(factory_->CreateStream(&stream)) &&
                SUCCEEDED(stream->InitializeFromMemory(data, size)) &&
                SUCCEEDED(factory_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
                texture = Decode(decoder.Get());
            break;
        }
        resources_.emplace(id, texture); return TextureId(texture.Get());
    }
};
static std::filesystem::path StatePath(const std::filesystem::path& deck)
{
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!length) return {};
    std::wstring directory(length, L'\0');
    const DWORD actual = GetEnvironmentVariableW(L"LOCALAPPDATA", directory.data(), length);
    if (!actual || actual >= length) return {};
    directory.resize(actual);
    if (deck.empty()) return std::filesystem::path(directory) / L"ScriptDeck" / L"WindowState" / L"magic.json";
    auto normalized = std::filesystem::weakly_canonical(deck).native();
    CharLowerBuffW(normalized.data(), static_cast<DWORD>(normalized.size()));
    const auto key = PathToUtf8(std::filesystem::path(normalized));
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : key) { hash ^= c; hash *= 1099511628211ULL; }
    return std::filesystem::path(directory) / L"ScriptDeck" / L"WindowState" /
        (std::to_wstring(hash) + L".json");
}
static void PositionPlayer(HWND hwnd, int left, int top, bool center)
{
    HMONITOR monitor = center ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) :
                               MonitorFromPoint(POINT{left, top}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{}; info.cbSize = sizeof(info);
    RECT window{};
    if (!GetMonitorInfoW(monitor, &info) || !GetWindowRect(hwnd, &window)) return;
    const int width = window.right-window.left, height = window.bottom-window.top;
    if (center) {
        left = info.rcWork.left + ((info.rcWork.right-info.rcWork.left)-width)/2;
        top = info.rcWork.top + ((info.rcWork.bottom-info.rcWork.top)-height)/2;
    }
    left = (std::max)(static_cast<int>(info.rcWork.left),
        (std::min)(left, (std::max)(static_cast<int>(info.rcWork.left), static_cast<int>(info.rcWork.right)-width)));
    top = (std::max)(static_cast<int>(info.rcWork.top),
        (std::min)(top, (std::max)(static_cast<int>(info.rcWork.top), static_cast<int>(info.rcWork.bottom)-height)));
    SetWindowPos(hwnd, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
int RunGui(const LaunchOptions& options)
{
    const auto scriptWorkingDirectory = std::filesystem::current_path();
    pendingFileDrops.clear();
    closeRequested = false; resizeWidth = resizeHeight = 0;
    playerWidth = playerHeight = 0;
    LaunchOptions launch = options;
    fixedPlayer = options.mode == LaunchMode::Player;

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); wc.lpszClassName = L"ScriptDeckWindow";
    wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(IDI_SCRIPTDECK_APP));
    wc.hIconSm=static_cast<HICON>(LoadImageW(wc.hInstance,MAKEINTRESOURCEW(IDI_SCRIPTDECK_APP),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    RegisterClassExW(&wc);
    const DWORD windowStyle = fixedPlayer ? (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) : WS_OVERLAPPEDWINDOW;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
        options.mode == LaunchMode::Magic ? L"ScriptDeck - Magic" : L"ScriptDeck",
        windowStyle, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        nullptr, nullptr, wc.hInstance, nullptr);
    if(hwnd)DragAcceptFiles(hwnd,TRUE);
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> target;
    bool imgui = false, win32 = false, dx11 = false;
    int result = 0;
    try {
        if (!hwnd) throw std::runtime_error("Cannot create ScriptDeck window.");
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = hwnd;
        desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        auto create = [&](D3D_DRIVER_TYPE driver) {
            return D3D11CreateDeviceAndSwapChain(nullptr, driver, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, &desc, &swap, &device, nullptr, &context);
        };
        if (FAILED(create(D3D_DRIVER_TYPE_HARDWARE)) && FAILED(create(D3D_DRIVER_TYPE_WARP)))
            throw std::runtime_error("Cannot initialize Direct3D 11.");
        auto renderTarget = [&] {
            ComPtr<ID3D11Texture2D> buffer;
            if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&buffer))) ||
                FAILED(device->CreateRenderTargetView(buffer.Get(), nullptr, &target)))
                throw std::runtime_error("Cannot create render target.");
        };
        renderTarget();
        IMGUI_CHECKVERSION(); ImGui::CreateContext(); imgui = true;
        auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        wchar_t windowsPath[MAX_PATH]{};
        GetWindowsDirectoryW(windowsPath, MAX_PATH);
        const auto font = std::filesystem::path(windowsPath) / L"Fonts" / L"meiryo.ttc";
        if (!std::filesystem::exists(font)) throw std::runtime_error("Japanese font meiryo.ttc was not found.");
        if (!io.Fonts->AddFontFromFileTTF(PathToUtf8(font).c_str(), 18, nullptr, io.Fonts->GetGlyphRangesJapanese()))
            throw std::runtime_error("Cannot load Japanese font.");
        if (!(win32 = ImGui_ImplWin32_Init(hwnd)) || !(dx11 = ImGui_ImplDX11_Init(device.Get(), context.Get())))
            throw std::runtime_error("Cannot initialize ImGui backends.");
        Images images(device.Get());
        GuiServices services;
        services.setConsoleMode = [](bool enabled) { ConsoleMode::Instance().SetEnabled(enabled); };
        services.chooseFile = [&](bool save) { return ChooseFile(hwnd, save); };
        services.chooseImage = [&] { return ChooseFile(hwnd, false, true); };
        services.discardChanges = [&] {
            return MessageBoxW(hwnd, L"未保存の変更があります。変更を破棄して続行しますか？",
                L"ScriptDeck", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
        };
        services.image = [&](const auto& path) { return images.Get(path); };
        for (const auto& resource : ImageResources) services.imageResources.emplace_back(resource.id, resource.name);
        services.resourceImage = [&](const auto& id) { return images.Resource(id); };
        GuiView view(launch, std::move(services));
        AppScriptApi app(view);
        // スクリプトAPIをGUIのネイティブサービスへ接続する。
        ScriptHost scriptHost;
        scriptHost.openFileDialog = [&](const FileDialogOptions& settings) {return ShowOpenFileDialog(hwnd,settings);};
        scriptHost.saveFileDialog = [&](const FileDialogOptions& settings) {return ShowSaveFileDialog(hwnd,settings);};
        scriptHost.install = [] {return DeckAssociation::InstallCurrentApplication();};
        scriptHost.uninstall = [] {return DeckAssociation::UninstallCurrentApplication();};
        scriptHost.navigateCard = [&](const std::string& action,const nlohmann::json& target) {return view.NavigateCard(action,target);};
        scriptHost.changeDeck = [&](const std::filesystem::path& path,bool save) {view.RequestDeckChange(path,save);};
        scriptHost.goHome = [&](bool save) {view.RequestHome(save);};
        scriptHost.setTopMost = [&](bool enabled) {
            if(!SetWindowPos(hwnd,enabled?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
                throw std::runtime_error("Cannot update TopMost state.");
        };
        scriptHost.getTopMost = [&] {return (GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;};
        scriptHost.windowFront = [&] {
            if(IsIconic(hwnd))ShowWindow(hwnd,SW_RESTORE);
            if(!SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
                throw std::runtime_error("Cannot bring window to front.");
            // Windows may deny activation when another process owns the foreground.
            SetForegroundWindow(hwnd);
        };
        scriptHost.args = launch.scriptArgs;
        scriptHost.workingDirectory = scriptWorkingDirectory;
        scriptHost.setMagic = [&](bool enabled) { app.setMagic(enabled); };
        scriptHost.setConsoleMode = [&](bool enabled) { app.setConsoleMode(enabled); };
        scriptHost.beep = [](int frequency, int duration) {
            if (!Beep(static_cast<DWORD>(frequency), static_cast<DWORD>(duration)))
                throw std::runtime_error("Cannot play beep.");
        };
        scriptHost.playWav = [](const std::filesystem::path& path, bool wait) {
            if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("WAV file does not exist.");
            if (!PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_NODEFAULT | (wait ? SND_SYNC : SND_ASYNC)))
                throw std::runtime_error("Cannot play WAV file.");
        };
        scriptHost.stopWav = [] { if (!PlaySoundW(nullptr, nullptr, 0)) throw std::runtime_error("Cannot stop WAV playback."); };
        const auto statePath = fixedPlayer ? StatePath(view.DeckPath()) : std::filesystem::path{};
        WindowState previous;
        const bool hasPrevious = fixedPlayer && UsesPreviousState(view.StartupPlacement()) && previous.Load(statePath);
        const auto startupAction = ResolveStartupAction(view.StartupPlacement(), hasPrevious);
        const bool restore = fixedPlayer && startupAction == StartupAction::Restore;
        if (restore) view.SetCardSize(previous.width, previous.height);
        auto syncCardSize = [&] {
            if (!fixedPlayer || (playerWidth == view.CardWidth() && playerHeight == view.CardHeight())) return;
            playerWidth = view.CardWidth(); playerHeight = view.CardHeight();
            RECT rectangle{0,0,playerWidth,playerHeight};
            if (!PlayerWindowRect(hwnd, rectangle) ||
                !SetWindowPos(hwnd, nullptr, 0, 0, rectangle.right-rectangle.left,
                              rectangle.bottom-rectangle.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
                throw std::runtime_error("Cannot update player card size.");
        };
        if (fixedPlayer) {
            playerWidth = view.CardWidth(); playerHeight = view.CardHeight();
            RECT rectangle{0, 0, playerWidth, playerHeight};
            if (!PlayerWindowRect(hwnd, rectangle))
                throw std::runtime_error("Cannot calculate player window size.");
            if (!SetWindowPos(hwnd, nullptr, 0, 0, rectangle.right-rectangle.left,
                              rectangle.bottom-rectangle.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
                throw std::runtime_error("Cannot set player window size.");
            // タイトルバーにはデッキ名だけを表示する。
            const auto title = PathFromUtf8(view.DeckName().empty() ? "ScriptDeck" : view.DeckName());
            SetWindowTextW(hwnd, title.c_str());
        }
        std::string displayedTitle;
        auto syncTitle = [&] {
            const auto requestedTitle = view.WindowTitle();
            if (requestedTitle == displayedTitle) return;
            const auto wideTitle = PathFromUtf8(requestedTitle);
            SetWindowTextW(hwnd, wideTitle.c_str());
            displayedTitle = requestedTitle;
        };
        syncTitle();
        if (restore) PositionPlayer(hwnd, previous.left, previous.top, false);
        else if (fixedPlayer && startupAction == StartupAction::Center) PositionPlayer(hwnd, 0, 0, true);
        WindowState magicState;
        const auto magicStatePath = StatePath({});
        const bool hasMagicState = magicState.Load(magicStatePath);
        bool haveMagicBounds = hasMagicState || !fixedPlayer;
        auto restoreMagic = [&] {
            WINDOWPLACEMENT placement{}; placement.length = sizeof(placement);
            if (!GetWindowPlacement(hwnd, &placement)) throw std::runtime_error("Cannot read Magic placement.");
            if (haveMagicBounds && magicState.width > 0 && magicState.height > 0) {
                placement.rcNormalPosition = {magicState.left, magicState.top,
                    magicState.left+magicState.width, magicState.top+magicState.height};
            }
            placement.flags = 0;
            placement.showCmd = magicState.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
            if (!SetWindowPlacement(hwnd, &placement)) throw std::runtime_error("Cannot restore Magic placement.");
        };
        if (!hasMagicState) {
            magicState.width = 1200; magicState.height = 800; magicState.maximized = true;
            WINDOWPLACEMENT placement{}; placement.length = sizeof(placement);
            if (GetWindowPlacement(hwnd, &placement)) {
                magicState.left = placement.rcNormalPosition.left; magicState.top = placement.rcNormalPosition.top;
            }
        }
        if (fixedPlayer) ShowWindow(hwnd, SW_SHOWDEFAULT);
        else restoreMagic();
        UpdateWindow(hwnd);
        RECT normalBounds{}; GetWindowRect(hwnd, &normalBounds);
        auto rememberMagic = [&] {
            if (fixedPlayer) return;
            WINDOWPLACEMENT placement{}; placement.length = sizeof(placement);
            if (!GetWindowPlacement(hwnd, &placement)) return;
            const RECT bounds = placement.rcNormalPosition;
            magicState.left = bounds.left; magicState.top = bounds.top;
            magicState.width = bounds.right-bounds.left; magicState.height = bounds.bottom-bounds.top;
            magicState.maximized = placement.showCmd == SW_SHOWMAXIMIZED ||
                (placement.showCmd == SW_SHOWMINIMIZED && (placement.flags & WPF_RESTORETOMAXIMIZED));
            haveMagicBounds = true;
        };
        auto saveMagic = [&] {
            if (haveMagicBounds && !magicState.Save(magicStatePath))
                OutputDebugStringW(L"ScriptDeck: could not save Magic window state.\n");
        };
        WindowState lastPlayer;
        bool havePlayerState = false;
        std::filesystem::path lastPlayerPath;
        auto rememberPlayer = [&] {
            if (!fixedPlayer || IsIconic(hwnd)) return;
            GetWindowRect(hwnd, &normalBounds);
            lastPlayer.left = normalBounds.left; lastPlayer.top = normalBounds.top;
            lastPlayer.width = view.CardWidth(); lastPlayer.height = view.CardHeight();
            lastPlayerPath = view.DeckPath(); havePlayerState = true;
        };
        auto savePlayer = [&] {
            if (!havePlayerState || lastPlayerPath.empty()) return;
            if (!lastPlayer.Save(StatePath(lastPlayerPath)))
                OutputDebugStringW(L"ScriptDeck: could not save previous window state.\n");
        };
        auto syncMode = [&] {
            const bool requestedPlayer = !view.IsMagic();
            if (requestedPlayer == fixedPlayer) return;
            if (fixedPlayer) { rememberPlayer(); savePlayer(); }
            else { rememberMagic(); saveMagic(); ShowWindow(hwnd, SW_RESTORE); }
            fixedPlayer = requestedPlayer;
            const DWORD style = fixedPlayer ? (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) : WS_OVERLAPPEDWINDOW;
            SetLastError(0);
            const LONG_PTR stateFlags = GetWindowLongPtrW(hwnd, GWL_STYLE) &
                static_cast<LONG_PTR>(WS_VISIBLE | WS_DISABLED | WS_MINIMIZE);
            const auto previousStyle = SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(style) | stateFlags);
            if (!previousStyle && GetLastError()) throw std::runtime_error("Cannot change window mode.");
            if (!SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED))
                throw std::runtime_error("Cannot update window frame.");
            if (fixedPlayer) {
                playerWidth = playerHeight = 0;
                syncCardSize();
                const auto title = PathFromUtf8(view.DeckName().empty() ? "ScriptDeck" : view.DeckName());
                SetWindowTextW(hwnd, title.c_str());
            } else {
                restoreMagic();
                SetWindowTextW(hwnd, L"ScriptDeck - Magic");
            }
        };
        std::unique_ptr<ScriptEngine> scripts;
        unsigned long long scriptGeneration = 0;
        std::string scriptCard;
        auto showScriptError = [&](const std::exception& error) {
            const auto text = PathFromUtf8(error.what()).native();
            if(GetCapture()==hwnd)ReleaseCapture();
            MessageBoxW(hwnd, text.c_str(), L"ScriptDeck - JavaScript error", MB_OK | MB_ICONERROR);
        };
        auto executeScripts = [&](const std::vector<ButtonEvent>& events) {
            if (!view.ScriptsEnabled()) { scripts.reset(); scriptCard.clear(); view.TakeFileDropEvents(); view.TakeConsoleCommands(); return; }
            const auto& deck = view.DeckData();
            if (!scripts || scriptGeneration != view.ScriptGeneration()) {
                auto activeHost = scriptHost;
                activeHost.model = std::make_shared<ScriptModel>([&]() -> Stack& { return view.MutableDeckData(); },
                    [&] { return view.ScriptGeneration(); }, [&] { view.NotifyScriptMutation(); });
                scripts = std::make_unique<ScriptEngine>([&](const std::string& message) {
                    const auto text = PathFromUtf8(message).native();
                    if(GetCapture()==hwnd)ReleaseCapture();
                    MessageBoxW(hwnd, text.c_str(), L"ScriptDeck", MB_OK | MB_ICONINFORMATION);
                }, activeHost);
                scriptGeneration = view.ScriptGeneration(); scriptCard.clear();
                try { scripts->RunGlobal(deck.script, "deck.js"); }
                catch (const std::exception& error) { view.CancelPendingDeckChange(); showScriptError(error); }
            }
            if (scripts->ExitRequested() || !view.ScriptsEnabled() || view.HasPendingDeckChange()) {view.TakeFileDropEvents();return;}
            if (scriptCard != deck.currentCardId) {
                scriptCard = deck.currentCardId;
                if (const auto* card = deck.FindCard(scriptCard)) {
                    try { scripts->RunScoped(card->script, card->id + "/card.js", "openCard", card->id); }
                    catch (const std::exception& error) { view.CancelPendingDeckChange(); showScriptError(error); }
                }
            }
            if(view.HasPendingDeckChange()){view.TakeFileDropEvents();view.TakeConsoleCommands();return;}
            if(view.HasPendingDeckChange()){view.TakeFileDropEvents();return;}
            for (const auto& event : events) {
                if (scripts->ExitRequested() || !view.ScriptsEnabled() || view.HasPendingDeckChange()) break;
                const auto* card = deck.FindCard(event.cardId);
                const auto* button = card ? card->FindObject(event.buttonId) : nullptr;
                if(event.cardId!=deck.currentCardId)continue;
                if(event.generation && event.generation!=view.ScriptGeneration())continue;
                if(!button)continue;
                if(event.handler=="mouseUp" && (button->type!=ObjectType::Button||!button->visible||!button->enabled))continue;
                if(event.handler=="change" && button->type!=ObjectType::Checkbox && button->type!=ObjectType::RadioButton)continue;
                const auto data=event.handler=="change"?nlohmann::json{{"checked",event.checked},{"group",event.group}}:nlohmann::json::object();
                try { scripts->RunScoped(button->script, card->id + "/" + button->id + ".js", event.handler, card->id, button->id,data); }
                catch (const std::exception& error) { view.CancelPendingDeckChange(); showScriptError(error); }
            }
            for(const auto& event:view.TakeFileDropEvents()) {
                if(scripts->ExitRequested()||!view.ScriptsEnabled()||view.HasPendingDeckChange())break;
                if(event.cardId!=deck.currentCardId || event.generation!=view.ScriptGeneration())continue;
                const auto* card=deck.FindCard(event.cardId);if(!card)continue;
                try {scripts->RunScoped(card->script,card->id+"/card.js","dropFiles",card->id,"",
                    {{"paths",event.paths},{"x",event.x},{"y",event.y}});}
                catch(const std::exception& error){view.CancelPendingDeckChange();showScriptError(error);}
            }
        };
        rememberPlayer();
        bool done = false;
        while (!done) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg); DispatchMessageW(&msg);
                if (msg.message == WM_QUIT) done = true;
            }
            for(const auto& event:pendingFileDrops)view.QueueFileDrop(event.paths,ImVec2(static_cast<float>(event.position.x),static_cast<float>(event.position.y)));
            pendingFileDrops.clear();
            syncMode();
            syncTitle();
            rememberPlayer();
            rememberMagic();
            if (closeRequested) { done = view.CanClose(); closeRequested = false; }
            if (done) break;
            syncCardSize();
            if (IsIconic(hwnd)) { Sleep(20); continue; }
            if (resizeWidth && resizeHeight) {
                target.Reset();
                context->OMSetRenderTargets(0, nullptr, nullptr);
                if (FAILED(swap->ResizeBuffers(0, resizeWidth, resizeHeight, DXGI_FORMAT_UNKNOWN, 0)))
                    throw std::runtime_error("Cannot resize swap chain.");
                resizeWidth = resizeHeight = 0; renderTarget();
            }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
            view.Draw();
            syncTitle();
            // 描画完了後にイベントを配送する。
            executeScripts(view.TakeButtonEvents());
            if(scripts && scripts->ExitRequested())view.CancelPendingDeckChange();
            if(view.HasPendingDeckChange()) {
                rememberPlayer();savePlayer();
                try {
                    if(view.ApplyPendingDeckChange()) {
                        scripts.reset();scriptCard.clear();
                        havePlayerState=false;
                        if(fixedPlayer) {
                            WindowState loaded;
                            const bool found=UsesPreviousState(view.StartupPlacement()) && loaded.Load(StatePath(view.DeckPath()));
                            const auto action=ResolveStartupAction(view.StartupPlacement(),found);
                            if(action==StartupAction::Restore)view.SetCardSize(loaded.width,loaded.height);
                            playerWidth=playerHeight=0;syncCardSize();
                            if(action==StartupAction::Restore)PositionPlayer(hwnd,loaded.left,loaded.top,false);
                            else if(action==StartupAction::Center)PositionPlayer(hwnd,0,0,true);
                        }
                        syncTitle();rememberPlayer();
                    }
                } catch(const std::exception& error) {showScriptError(error);}
            }
            if (scripts && scripts->ExitRequested()) {
                if (view.CanClose()) { result = scripts->ExitCode(); done = true; }
                else scripts->ClearExitRequest();
            }
            ImGui::Render();
            const float color[] = {0.1f,0.12f,0.16f,1};
            ID3D11RenderTargetView* rt = target.Get();
            context->OMSetRenderTargets(1, &rt, nullptr);
            context->ClearRenderTargetView(target.Get(), color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            const auto hr = swap->Present(1, 0);
            if (FAILED(hr)) throw std::runtime_error("Direct3D presentation failed.");
            // Execute only after the next frame (without the input popup) is presented.
            for(const auto& command:view.TakeConsoleCommands()) {
                if(!scripts || command.generation!=view.ScriptGeneration() || view.IsMagic() ||
                   view.HasPendingDeckChange() || scripts->ExitRequested())break;
                try {scripts->RunGlobal(command.code,"player-console.js");}
                catch(const std::exception& error){view.CancelPendingDeckChange();showScriptError(error);}
            }
            if (hr == DXGI_STATUS_OCCLUDED) Sleep(20);
        }
        rememberMagic();
        saveMagic();
        savePlayer();
    } catch (const std::exception& e) {
        MessageBoxA(hwnd, e.what(), "ScriptDeck error", MB_OK | MB_ICONERROR); result = 1;
    }
    PlaySoundW(nullptr, nullptr, 0);
    std::fflush(nullptr);
    if (dx11) ImGui_ImplDX11_Shutdown();
    if (win32) ImGui_ImplWin32_Shutdown();
    if (imgui) ImGui::DestroyContext();
    target.Reset(); swap.Reset(); context.Reset(); device.Reset();
    pendingFileDrops.clear();
    if (hwnd) {DragAcceptFiles(hwnd,FALSE);DestroyWindow(hwnd);}
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
#endif
