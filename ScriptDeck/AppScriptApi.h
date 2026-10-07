#pragma once
#include "GuiView.h"

// JavaScriptのappオブジェクトへ公開するネイティブAPI。
// エンジン導入時にapp.setMagic(boolean)をこのメソッドへバインドする。
// UIスレッドで呼び出す。引数はJavaScript側でbooleanであることを検証する。
class AppScriptApi
{
public:
    static constexpr const char* SetMagicScriptName = "app.setMagic";
    explicit AppScriptApi(GuiView& view) : view_(view) {}
    static constexpr const char* SetConsoleModeScriptName = "app.setConsoleMode";
    void setConsoleMode(bool enabled) { view_.SetConsoleMode(enabled); }
    void setMagic(bool enabled) { view_.SetMagic(enabled); }
private:
    GuiView& view_;
};
