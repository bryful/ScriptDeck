カード・オブジェクトの検索、プロパティ、リスト操作は **OBJECT_API.md** を参照してください。

# 現在の実装範囲

QuickJS-NG v0.16.2でapp / fs / alertを公開しています。全関数一覧はBUILTINS.mdを参照してください。app.setMagic / app.setConsoleModeはAppScriptApiへ接続済みです。ScriptHostは引数・起動フォルダ・モード・音声・標準ストリームのホスト接続を保持します。fsの処理はScriptIO、JavaScriptへの登録とrunCodeはScriptEngineへ実装しています。

以下は既存ネイティブAPIの契約の記録です。


﻿# ScriptDeckのスクリプトAPI接続仕様

## app.setMagic

```javascript
app.setMagic(true);  // Player → Magic
app.setMagic(false); // Magic → Player
```

引数はbooleanを1つ。戻り値はundefined。同じモードへの呼び出しは何もしない。
別名のapp.setModeは公開しない。

ネイティブ実装はAppScriptApi.hのAppScriptApi::setMagic(bool)。GuiView::SetMagic(bool)を呼ぶ。
JavaScriptエンジンの初期化時にappオブジェクトを生成し、このメソッドをsetMagicとして必ず登録する。
エンジン側のバインディングは引数数とboolean型を検証し、不正な呼び出しではTypeErrorを返す。型の暗黙変換は行わない。
CLIではGUIインスタンスがないため利用不可としてスクリプトエラーにする。
AppScriptApiはGuiViewを参照するため、エンジンとAPIをGuiViewより先に破棄する。
UIスレッドで呼ぶ。スクリプトイベント処理を描画前に行い、モード変更を次の描画へ反映する。

切り替えは同じStack、Card、CardObjectを維持する。編集内容やField入力を破棄しない。
未保存状態を維持し、切り替えだけでは保存しない。Playerに切り替えても未保存変更があれば終了時に破棄確認する。
Playerはカードのみの固定サイズ、Magicは編集UIとリサイズ可能ウィンドウ。
MagicからPlayerへ切り替える時は、その時点のカードサイズと位置を使う。起動位置の復元・中央配置は再実行しない。
Player→Magicへ切り替える際、Playerの位置・サイズを別設定ファイルへ記録する。
Magicのウィンドウ位置・外寸はセッション中に保持し、次にMagicへ戻った時に復元する。

現在はネイティブAPIとウィンドウ切り替えを実装済み。QuickJS-NGとappへのバインディングは実装済み。
この仕様とAppScriptApi::SetMagicScriptNameを、スクリプト実装時の登録チェックに使う。

## ボタンイベント配送

GuiView::TakeButtonEvents()がcardId / buttonIdのキューを返してクリアする。マウスクリックとカードのEnter/Escape対象ボタンは同じPushButton経路を通る。スクリプト実装時は描画後にキューを取り出し、対象ButtonのmouseUpへ配送する。対象が非表示・無効の場合は配送しない。描画中に配列を変更するスクリプトを直接実行しない。現在のホストは描画後にキューを取り出し、部品コードの評価とmouseUpの呼び出しを実行する。

FieldまたはInputBoxの入力フォーカス中はカードのEnter/Escape配送を無効にする。Dropdownのポップアップ操作中も無効にする。InputBoxはReturn/テンキーEnterを無視し、1行テキストと入力フォーカスを維持する。

## app.setConsoleMode

app.setConsoleMode(true)で実行中に親コンソールへ接続し、親がなければ作成します。falseでScriptDeckの接続を解除し、単独で作成したコンソールを閉じます。親のcmd/PowerShellは終了しません。再度trueを呼べます。標準入力・出力・エラーを再接続し、ファイル・パイプへのリダイレクトは切り替え後も維持します。無効中の非リダイレクト出力はNULへ送ります。デッキの保存状態・Player/Magicは変えません。

C++のAppScriptApi::setConsoleMode(bool)は実装済みです。UIスレッドで呼びます。JavaScriptエンジン導入時はapp.setConsoleModeへバインドし、引数をbooleanに限定してください。ネイティブの失敗は例外です。JavaScriptのapp.setConsoleModeへバインド済みです。

上書きはmain.cpp / GuiView.h / GuiView.cpp / GuiWindow.cpp / AppScriptApi.h、新規追加はConsoleMode.hです。Windowsでの実コンソール切り替えは実機未確認です。


## 実行タイミング

- Deckスクリプト: Player起動時、Deckの新規読み込み時、Magicの実行プレビュー開始時にランタイムを生成し、1回実行します。カード移動だけでは再実行しません。共通関数や共有変数を定義できます。
- カードスクリプト: 起動時の最初のカード、および現在のカードが切り替わった時に実行します。コード全体を評価した後、openCard関数があれば呼びます。戻ってきたカードも再実行します。
- 両方ある場合はDeckスクリプト、その後カードスクリプトの順です。Magicの通常の編集状態では実行しません。

```javascript
// Deckスクリプト: 全カードから利用できる共通関数
function showMessage(text) { alert(text); }
```

```javascript
// カードスクリプト: このカードが開いた時
function openCard(event) {
    showMessage("カードを開きました");
}
```

カードスクリプトのローカル変数・関数は、そのカードスクリプトの呼び出し内のスコープです。共有するものはDeckスクリプトに置きます。Magicへ戻って編集するとランタイムを破棄し、次のプレビュー開始時にDeckスクリプトから再実行します。

## change / dropFiles

Checkbox / RadioButtonのchange(event)、カードのdropFiles(event)を追加。スクリプト全体を評価し、指定ハンドラをthis / event.targetと共に呼びます。実行プレビュー・Playerでのみ配送します。DAY2_API.mdを参照してください。

カード移動・Deck切り替え・Home起動については [NAVIGATION_API.md](NAVIGATION_API.md) を参照してください。
