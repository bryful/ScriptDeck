# 組み込み関数実装版

これまで一覧にしたapp / fs / alert / JSON / runCodeを実装しました。全関数と仕様はBUILTINS.mdを参照してください。

今回もプロジェクト一式を展開し、同梱のScriptDeck.slnからビルドしてください。ScriptIO.h / ScriptIO.cppを追加し、WAV用のwinmm.libをリンクしています。

確認は `ScriptDeck.exe builtin-test.deck "hello"` で行えます。引数・runCode・BOMファイル・標準出力・Beep・Magic切り替えをそれぞれボタンで試せます。「BOMファイル」ボタンは起動フォルダへScriptDeck_test_bom.txt / ScriptDeck_test_plain.txtを作成／上書きします。Magicで開く場合は `ScriptDeck.exe -magic builtin-test.deck "hello"` とし、実行プレビューを有効にしてください。

GUI起動のDeck指定後の引数はすべてスクリプトへ渡します。オプションを使う場合はDeckより前に指定します。Deckの後の--は区切りとして一度除去します。その後の引数は加工しません。-runによる従来のinfo/dump/validate/copyは維持しています。

検証: 実エンジンで複数引数・JSON・BOM有り無し・追記・バイナリーファイル・fs操作・別ファイルシステムへの移動・runCodeのスコープ・終了要求を確認。実際のstdin/stdoutパイプで全256バイトやNUL/CRLFの一致を確認。モード・コンソール・音声はホストコールバックとの接続をテスト。Windowsのコンソール再接続・WAV/Beep音・GUI切り替えは実機未確認です。

変更: ScriptEngine.h / ScriptEngine.cpp / GuiView.cpp / GuiWindow.cpp / LaunchOptions.h / LaunchOptions.cpp / ScriptDeck.vcxproj / CMakeLists.txt。新規: ScriptIO.h / ScriptIO.cpp / BUILTINS.md / builtin-test.deck / tests/BuiltinTests.cpp。これまでのalert確認デッキとテストも同梱しています。

以下は過去版の記録です。現行の組み込みAPIについてはBUILTINS.mdを参照してください。

---

# JavaScript実行・alert確認版

今回の版はQuickJS-NG v0.16.2を同梱し、PlayerおよびMagicの「実行プレビュー」でJavaScriptを実行できます。ネイティブAPIの公開はalert(object)だけです。JSON.parse/stringifyなどJavaScript標準機能は利用できます。app / fs / runCodeはまだ公開していません。

## 最初の確認

1. ZIPを新しいフォルダへ展開し、同梱のScriptDeck.slnをVisual Studioで開いてDebug x64をビルドします。QuickJSはC11としてコンパイルするため、今回は.cppだけの上書きではなくプロジェクト設定も更新してください。DLLの追加配置は不要です。
2. `ScriptDeck.exe alert-test.deck`でPlayerを起動します。「alert(object)」ボタンをクリックすると、日本語・配列・クリック回数を含むダイアログを表示します。Enterも同じボタンを実行します。
3. `ScriptDeck.exe -magic alert-test.deck`で編集できます。部品を選び「部品スクリプト」を編集・保存し、「実行プレビュー」を有効にしてボタンを押してください。プレビュー解除・再開で実行環境をリセットします。

部品スクリプトは `alert("動作しました");` と直接書くか、`function mouseUp(event) { alert({type: event.type}); }` と書けます。前者は押下ごとに全体を実行し、後者は全体を評価した後にmouseUpを呼びます。両方を書くと両方実行されます。

デッキスクリプトは実行環境作成時に一度グローバルで実行します。カードスクリプトはカードに入るときに独立スコープで実行し、openCard関数があれば呼びます。部品は押下ごとの独立スコープで評価します。デッキで定義した共通関数・globalThisの値は同じ実行環境内で共有します。新規・開く・プレビュー終了で実行環境を作り直します。Fieldの変更イベントやイベントの階層配送はまだ実装していません。

文字列・数値・真偽値・null・undefinedは文字列化し、オブジェクト・配列は整形JSONで表示します。循環参照は表示用のマーカーへ変換します。表示を閉じるまで待機します。構文／実行エラーはソース名付きのエラーダイアログへ表示し、アプリは継続します。

無限ループ対策として実行時間2秒・メモリー64MiB・JSスタック1MiBを設定しています。alertを待つ時間は制限に含めません。外部モジュールやファイル入出力は未公開です。

検証: 実際のエンジンで日本語alert、JSON表示、循環参照、mouseUp、スコープ、構文／実行エラー、Promise、無限ループ中断後の復帰を確認。ImGuiのクリック・Enterからボタンイベントを取り出してエンジンへ配送するテストと既存CLI回帰テストも通過。WindowsのVisual Studioビルド・ダイアログの実機確認は未実施です。

新規ファイル: ScriptEngine.h / ScriptEngine.cpp / quickjsフォルダ / tests/ScriptEngineTests.cpp / alert-test.deck。変更ファイル: GuiView.h / GuiView.cpp / GuiWindow.cpp / LaunchOptions.cpp / ScriptDeck.vcxproj / CMakeLists.txt。実行エンジンのMITライセンスはquickjs/LICENSEに同梱。

以下はこれまでの変更記録です。スクリプトの実装状況は上記を参照してください。

---

﻿# ScriptDeck GUI版

Windows用のDear ImGui + Win32 + DirectX 11によるGUIです。依存ソースを同梱しているため追加ダウンロードは不要です。

## Visual Studio 2026で起動

1. ZIPを新しいフォルダへ展開します。
2. ScriptDeck.slnを開きます。
3. Debug / x64を選んでビルドします。
4. F5で起動します。デバッグ引数は「-magic sample.deck」に設定済みです。

プロジェクトはv145、C++20、Unicode、/utf-8、プリコンパイル済みヘッダー不使用に設定済みです。
F5の引数はプロジェクトのプロパティ → デバッグ → コマンド引数から変更できます。
F5以外でexeを起動する場合も、sample.deckを含む展開フォルダから実行するかファイルの絶対パスを渡してください。

```cmd
bin\x64\Debug\ScriptDeck.exe sample.deck
bin\x64\Debug\ScriptDeck.exe -magic sample.deck
bin\x64\Debug\ScriptDeck.exe -magic
bin\x64\Debug\ScriptDeck.exe -run sample.deck info
bin\x64\Debug\ScriptDeck.exe -run sample.deck dump
```

通常起動: カードだけを表示する固定サイズのウィンドウ。タイトルバーはデッキ名のみ。ボタン、入力欄、文字、画像の表示。カード外のツールバー、デッキ名・カード名表示、スクロール枠はありません。デッキ未指定時は %LOCALAPPDATA%\ScriptDeck\home.deck を開きます。初回は内蔵リソースから作成します。
-magic: 編集画面。カード・部品の追加／削除、ドラッグ移動、右下ハンドルでサイズ変更、プロパティ編集、保存／別名保存。
実行プレビュー: 編集パネルを隠してPlayerと同じ部品表示を確認できます。プレビュー中の入力やカード移動は現在のデッキへ反映されます。保存すると反映後の状態が保存されます。

## 操作

- 部品の追加: 左側のbutton / field / text / image。
- 部品の選択: キャンバス上、または左側の部品一覧をクリック。
- 位置変更: 部品をドラッグ。プロパティの「位置」から数値入力も可能。
- サイズ変更: 選択した部品の右下の青い四角をドラッグ。
- Field/Text/Buttonの文字変更: 左側の「文字」欄。
- 画像: 「画像を選択」または画像パスを指定。PNG/JPEG/BMP/GIF等、Windows Imaging Componentが読める画像の先頭フレームを表示します。表示サイズに合わせて伸縮します。
- 画像パスは絶対パス、または保存済みデッキのフォルダからの相対パス。未保存デッキは起動時のカレントフォルダ基準です。別の場所へ別名保存するときは相対パスの参照先に注意してください。
- 非表示の部品も編集モードでは選択・編集できます。Player/プレビューでは非表示になります。
- MagicではCtrl+O: 開く。Ctrl+N: 新規（Magicのみ）。Ctrl+S: 保存（Magicのみ）。Ctrl+Shift+S: 別名保存（Magicのみ）。
- 閉じる／新規／別のデッキを開く際は未保存変更の破棄を確認します。保存する場合は確認をキャンセルして「保存」を実行してください。
- 日本語表示にWindows付属のメイリオを使用します。

画像はパス単位でキャッシュします。外部で同じ画像を更新した場合はアプリを再起動してください。
カード・部品の削除にはUndoはありません。操作後に保存しなければ元のファイルは変更されません。

## 現段階の範囲

GUIとJSONデータの編集・保存・読み込みを実装しています。
JavaScript/QuickJSの実行、ExternalObject、音声、イベント処理は未実装です。
ボタンを押すとクリックした部品名が上部に表示されます。部品・カード・デッキのスクリプトは編集・保存できますが実行されません。
Playerでの文字入力はそのセッション中のみ保持し、Playerには保存機能がありません。カード移動は将来のスクリプトAPIで制御する予定です。現段階では移動用の追加ボタンは表示しません。

JSONは従来と同じformat="ScriptDeck", version=1のUTF-8 .deckです。
Stack → Card → CardObjectの階層、ID検索、型・重複ID・サイズ・現在カード参照の検証を維持しています。
未知のJSON項目は再保存には引き継がれません。読み込み失敗時は既存データを保持します。
保存は同じ場所の.tmpへ書いてから置換します。失敗時は復旧用に.tmpを残します。同時保存には対応しません。
CLIのinfo / dump / validate / copyは維持しています。main等のJavaScript関数呼び出しはまだありません。

## 既存のVisual Studioプロジェクトへ組み込む場合

今回のScriptDeckApp.cppも含めてソースを差し替えます。GuiView.cpp / GuiWindow.cppを追加します。
imgui.cpp / imgui_draw.cpp / imgui_tables.cpp / imgui_widgets.cpp、imgui/misc/cpp/imgui_stdlib.cpp、imgui/backends/imgui_impl_win32.cpp / imgui_impl_dx11.cppもビルド対象へ追加します。
追加のインクルードディレクトリに「imgui」「imgui/backends」「imgui/misc/cpp」を設定してください。
リンクするライブラリはd3d11.lib / d3dcompiler.lib / dxgi.lib / windowscodecs.lib / ole32.lib / comdlg32.lib / dwmapi.libです。
C++17またはC++20、/utf-8、Unicode、プリコンパイル済みヘッダー不使用にしてください。
自動生成されたScriptDeck.cppは除外し、main.cppだけを入口にしてください。
GUIモードでは単独起動時の専用コンソールを解放します。端末から呼び出した場合の端末は維持します。

## 検証と環境

LinuxのC++20でGUI描画部分のコンパイル、ImGuiのPlayer/Magic描画フレーム、キーボードからの開く／新規／保存／別名保存、JSON往復、未保存変更の確認、CLI動作を検証しています。
Windows側のウィンドウ、DirectX、ファイルダイアログ、WIC画像読込、IMEの実機操作はこの環境では未確認です。
Windows以外では既存のテキスト版とCLIのみをビルドします。

同梱依存: Dear ImGui 1.91.9b（MIT、imgui/LICENSE.txt）、nlohmann/json 3.12.0（MIT、json.hpp内に記載）。

## ImGuiが見つからない場合

imguiフォルダはScriptDeck.vcxprojと同じフォルダに必要です。ZIPはフォルダ構成を維持したまま新しい場所に展開して同梱のScriptDeck.slnを開いてください。ソースファイルだけのコピーではビルドできません。既存プロジェクトへ移す場合もimguiフォルダ全体とnlohmannフォルダを一緒にコピーしてください。

今回のプロジェクトはソース・ヘッダー・追加インクルードパスをMSBuildProjectDirectoryから指定します。NOMINMAXの再定義とCard::FindObjectの引数名による隠蔽警告も修正済みです。

## Player表示の修正

カード左上=(0,0)、表示領域=Stackのwidth×heightです。タイトルバー・OSの外枠の分だけ外寸は大きくなります。サイズ変更枠と最大化を無効にし、DPIの異なるモニターへ移動しても表示領域のピクセル数を保つようにしています。Playerには状態メッセージや開発情報を表示しません。読み込みエラーは独立したダイアログに表示します。Magicの編集UIは維持しています。

既存プロジェクトへ適用する場合、今回変更したのはGuiView.h / GuiView.cpp / GuiWindow.cppです。この3ファイルを同じソースフォルダへ上書きしてください。

検証: Playerの表示領域640×480、余白0、入力欄の配置20×70・サイズ400×80、カード外の子パネルなし、全体スクロールなしをImGui描画テストで確認しました。Magicの開く・新規・保存・別名保存・変更破棄確認も通過しています。Win32側の固定サイズ・DPI・タイトル変更の実機操作は未確認です。

## Playerの起動位置・実行中のサイズ変更

Magicの左上に「Player起動位置」を追加しました。選択後はデッキを保存してください。
- デフォルト: Windowsが決める位置。サイズはデッキ保存時のwidth/height。
- 前回実行時の位置・サイズ（デフォルト）: 前回のPlayer終了時の状態を復元し、記録がなければデフォルト位置へ戻ります。
- 前回実行時の位置・サイズ（画面中央）: 前回のPlayer終了時の状態を復元し、記録がなければ画面中央へ配置します。復元先のモニターがない場合は現在の作業領域へ調整します。
- 画面中央: 起動先モニターの作業領域中央。サイズはデッキ保存時のwidth/height。

設定はstartupPlacement（default / previous_default / previous_center / center）として.deckに保存します。古い.deckは設定省略=defaultとして読み込めます。Magic自身の編集ウィンドウ配置を変更する設定ではありません。
前回の記録は%LOCALAPPDATA%\ScriptDeck\WindowState内にデッキの絶対パスから生成したキーで保存し、Playerから.deck本体は上書きしません。同じデッキを同時実行した場合は最後に終了したものの状態を使用します。記録が壊れている場合は復元せず、保存できない場合はその回の記録が残りません。

C++ではGuiView::SetCardSize(width, height)を実行中に呼べます（UIスレッドで呼んでください）。範囲は各1～8192ピクセルです。呼び出すとStack内のサイズが更新され、次の描画ループでWin32ウィンドウの表示領域とDirectX描画バッファも追従します。手動のウィンドウサイズ変更禁止は維持します。サイズはStack共通で、全カードに適用されます。現在はJavaScriptエンジンが未実装なのでJavaScriptからこのAPIを呼ぶ機能はまだありません。

この更新を既存プロジェクトへ適用する場合はStack.h / Stack.cpp / GuiView.h / GuiView.cpp / GuiWindow.cppを上書きし、WindowState.hを同じフォルダへ追加してください。ビルド対象となる新しい.cppはありません。

検証:3種類の設定のJSON保存・復元、旧.deck互換、前回状態の保存・復元、不正記録の拒否、実行中の640×480→800×600変更後の描画と範囲チェック、CLIテストが通過しています。Windowsの配置・モニター・DirectX連動の実機確認は未実施です。

## 起動位置の4種類と前回記録の分離

現在の選択肢は以下の4種類です。
1. デフォルト: Windows標準の位置＋デッキに保存されたカードサイズ。
2. 前回実行時の位置・サイズ（デフォルト）: 記録があれば復元。なければ1と同じ。
3. 前回実行時の位置・サイズ（画面中央）: 記録があれば復元。なければ画面中央＋デッキに保存されたカードサイズ。
4. 画面中央: 毎回画面中央＋デッキに保存されたカードサイズ。

.deckに保存するのは起動方式（startupPlacement）と設計時のカードサイズだけです。Playerの前回実行時の位置・サイズは.deckへ書き戻さず、%LOCALAPPDATA%\ScriptDeck\WindowStateのデッキごとのJSON設定ファイルへ保存します。記録がない場合や読み込みに失敗した場合に、2と3のフォールバックを適用します。

JSON上の方式はdefault / previous_default / previous_center / center。旧方式previousはprevious_defaultとして引き継ぎます。
この更新だけ適用する場合はStack.cpp / GuiView.cpp / GuiWindow.cpp / WindowState.hを上書きしてください。

## app.setMagicの準備実装

スクリプトのAPI名はapp.setMagic(true) / app.setMagic(false)で固定します。
AppScriptApi.hにネイティブの受け口AppScriptApi::setMagic(bool)、GuiViewに実際のSetMagic(bool)を実装しています。Windowsホストはモード変更を監視してウィンドウ枠・表示サイズ・タイトルを更新します。

同じデッキを保持してMagicの編集UIとPlayerのカードのみ表示を切り替えます。切り替えだけでは保存せず、未保存状態を保持します。Magicの未保存変更がある場合はPlayerへ移っても終了時に破棄確認します。Magicのウィンドウ外寸・位置をセッション中に保持し、戻ると復元します。Playerの前回状態にはMagicのウィンドウサイズを混ぜません。
現在はJavaScriptエンジンがないため、JavaScriptコードからの呼び出しはまだできません。エンジン導入時の登録先・引数・戻り値・エラー・寿命の仕様をSCRIPT_API.mdに記載しました。

この更新の上書き対象はGuiView.h / GuiView.cpp / GuiWindow.cpp、新規追加はAppScriptApi.hです。新たな.cpp追加はありません。

テスト:AppScriptApi::setMagicからの両方向切り替え、同じモードへの呼び出し、描画、デッキとサイズの保持、未保存変更の破棄確認が通過。Win32側のウィンドウ切り替えの実機確認は未実施です。

## 追加部品とカードキー属性

Magicの追加ボタンにlistbox / dropdownlist / inputboxを追加しました。
- Listbox: 複数の項目を常時表示し、1項目を選択する。
- DropdownList: 開いたリストから1項目を選択する。
- InputBox: 1行専用。Return/テンキーEnterを無視して入力フォーカスを維持する。貼り付けによる改行も入力しない。

Listbox/DropdownListの項目はプロパティで1行1項目として編集します。選択番号は0から始まり、-1は未選択です。選択するとselectedIndexとtext（選択文字列）が変わります。Playerの選択はそのセッション中のみで、保存機能はありません。
カード名の下にEnterボタン / Escapeボタンを追加しました。「指定なし」または同じカード内のButtonを選べます。Field/InputBoxのフォーカス中とDropdown展開中はカードキー配送を抑止します。非表示・無効のButtonは配送しません。対応Buttonを削除するとカード側の指定も解除します。
キーによる押下とマウスクリックは同じネイティブボタンイベントキューに入ります。JavaScriptの実行はまだありません。

## 内蔵画像

Imageの「画像ソース」で外部ファイル／内蔵リソースを選べます。内蔵リソースにはScriptDeckロゴを同梱しました。画像バイナリはScriptDeck.exeのRCDATAへ組み込み、.deckにはimageSource="resource"とresourceIdだけを保存します。外部画像ファイルがなくても内蔵画像は表示できます。
追加するときはResourceIds.hに数値ID、ScriptDeck.rcにRCDATAのファイル定義、ImageResources.hにデッキで使う文字列ID・表示名・数値IDを追加して再ビルドしてください。デッキの画像をexeへ取り込む編集機能はありません。

今回の更新はResourceCompileが必要なので、同梱のScriptDeck.slnでビルドする方法が確実です。既存プロジェクトへ取り込む場合はCardObject.h / Card.h / Stack.cpp / GuiView.h / GuiView.cpp / GuiWindow.cppを上書きし、ImageResources.h / ResourceIds.h / ScriptDeck.rcを追加してください。ScriptDeck.rcはリソースコンパイルの対象にし、assets/logo.pngも同じ構成でコピーしてください。以前のヘッダーと更新版.cppを混在させないでください。

検証: Listbox/DropdownListの選択操作と保存、InputBoxの貼り付け改行除去、新部品のJSON保存・読み込み・描画、InputBoxでのReturn/テンキーEnter後のフォーカス保持、Field/InputBoxフォーカス時のEnter/Escape抑止、フォーカス解除後の復帰、マウスとキー押下のイベント一致、内蔵リソースIDの描画ルーティング、CLI回帰テストが通過しています。Windowsのリソースコンパイル・WICメモリ画像デコード・実機操作は未確認です。

## カードとオブジェクトの色

Magicのカード属性に「カード背景色」を追加しました。選択した部品のプロパティでは「色」を開くと、背景色・文字色・枠線色を個別に指定できます。Imageは文字色の代わりに「画像の着色」を指定します。チェックを外すと標準色に戻り、Aで不透明度を調整できます。Text/Imageの背景も指定可能です。Listbox/DropdownListは項目・展開リストにも指定色を適用し、選択・ホバー時は背景を明るくして区別します。Magicの選択枠とサイズ変更ハンドルは編集用の青色を維持します。

JSONはRGBA各0～1の4要素配列です。カードはbackgroundColor、オブジェクトはbackgroundColor / textColor / borderColor / tintColorを保存します。オブジェクトのnullは標準色を表します。旧デッキで属性が省略されている場合も従来の色で読み込みます。

今回の上書き対象はCard.h / CardObject.h / Stack.cpp / GuiView.cppです。新規のソースファイル・プロジェクト設定変更はありません。ZIPには以前の更新も含むプロジェクト一式を同梱しています。

検証: 色の保存・読み込み、旧デッキ互換、不正なRGBAの拒否、Player/Magicの描画色、前回追加した部品・キー操作の回帰テストを確認しました。Windowsの実機表示は未確認です。

## Magicの未保存表示

保存成功のメッセージは表示しません。Magicのネイティブタイトルは「デッキ名 - Magic」で、未保存の変更がある場合だけ末尾に「 *」を表示します。保存成功で消え、保存キャンセル・失敗時は残ります。新規デッキも保存前は「*」付きです。今回の上書き対象はGuiView.h / GuiView.cpp / GuiWindow.cppです。

## 必要時だけコンソールを使用

WindowsではGUIアプリとして起動し、Player/Magicではコンソールを開きません。-runと--helpの場合だけ親コンソールへ接続し、親がなくリダイレクトもなければコンソールを作成します。標準出力・標準エラー・標準入力のリダイレクトを保持します。GUI起動エラーはメッセージボックスで表示します。

今回の変更はmain.cpp / ScriptDeck.vcxproj / CMakeLists.txtです。既存VSプロジェクトはリンカー→システム→サブシステムをWindowsへ変更し、追加の依存ファイルにshell32.libを追加してください。エントリポイントは未指定のままにします。同梱の.slnなら設定済みです。

Windowsサブシステムのexeはcmdで待機されない場合があります。バッチ処理で終了待ちが必要な場合は start /wait "" ScriptDeck.exe -run sample.deck dump > output.json を使用してください。Windowsでの親コンソール接続・リダイレクトの実機検証は未実施です。

## app.setConsoleMode

app.setConsoleMode(true)で実行中に親コンソールへ接続し、親がなければ作成します。falseでScriptDeckの接続を解除し、単独で作成したコンソールを閉じます。親のcmd/PowerShellは終了しません。再度trueを呼べます。標準入力・出力・エラーを再接続し、ファイル・パイプへのリダイレクトは切り替え後も維持します。無効中の非リダイレクト出力はNULへ送ります。デッキの保存状態・Player/Magicは変えません。

C++のAppScriptApi::setConsoleMode(bool)は実装済みです。UIスレッドで呼びます。JavaScriptエンジン導入時はapp.setConsoleModeへバインドし、引数をbooleanに限定してください。ネイティブの失敗は例外です。JavaScript実行エンジン自体は未実装なので、現在はデッキに書いたJavaScriptから呼ぶことはできません。

上書きはmain.cpp / GuiView.h / GuiView.cpp / GuiWindow.cpp / AppScriptApi.h、新規追加はConsoleMode.hです。Windowsでの実コンソール切り替えは実機未確認です。

## Magicの前回ウィンドウ復元

Magicの位置・通常時のサイズ・最大化状態を%LOCALAPPDATA%\ScriptDeck\WindowState\magic.jsonへ保存し、次回復元します。デッキに依存しないMagic共通設定です。設定がない、または読み込めない場合は最大化で起動します。最大化終了でも通常サイズを保持します。最小化状態は復元せず、最小化前の通常／最大化へ戻します。モード切り替え時にもMagic状態を保存します。上書き対象はGuiWindow.cpp / WindowState.hです。Windowsでの実機動作は未確認です。

## alertのWindowsスタックエラー修正

alert内の正規表現replaceを廃止し、終端文字の可視化は長さ付きUTF-8を受け取るC++処理へ移しました。各実行前にQuickJSのスタック基準を更新し、JSスタック上限を1MiBに設定します。変更対象はScriptEngine.cppだけです。プロジェクトのネイティブスタック予約8MiBは維持してください。

## カード・オブジェクトのJavaScript操作

OBJECT_API.mdに検索・プロパティの読み書き・Listbox/DropdownList操作を掲載しています。ScriptModel.h/.cppを追加したため、今回のScriptDeck.sln / .vcxprojでビルドしてください。

## 動的オブジェクト

card.createObject / removeObject、object.remove / clone、一覧取得と並べ替えを追加。仕様はOBJECT_API.md、動作例はdynamic-test.deckです。通常のPlayer、またはMagicの実行プレビューで確認してください。

## Magicの並べ替え

カード一覧・部品一覧で対象を選び、「上へ」「下へ」で順番を変更できます。選択対象とIDを保持し、変更は未保存扱いになります。部品は一覧の下ほど後に描画されます。保存するとカード順・部品順がDeckファイルへ反映されます。

## Checkbox / RadioButton・ファイル選択・ドロップ

DAY2_API.mdを参照してください。day2-test.deckにGUI・スクリプトの操作例を収録しました。FileDialogs.h/.cppを追加したため、更新済み.vcxprojでリビルドしてください。

カード移動・Deck切り替え・Home起動については [NAVIGATION_API.md](NAVIGATION_API.md) を参照してください。

Version 1.0のPlayerスクリプト入力、アイコン、`app.install()` / `app.uninstall()` は [VERSION1.md](VERSION1.md) を参照してください。
