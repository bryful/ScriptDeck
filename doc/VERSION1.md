# ScriptDeck 1.0

## Playerのスクリプト入力

Playerで **Ctrl + Space** を押すと、カード上に1行のスクリプト入力ウィンドウを表示します。

- **Run** または **Enter**: 入力ウィンドウを閉じ、次のフレームが表示されてからJavaScriptを実行。
- **Close** または **Escape**: 入力ウィンドウを閉じる。
- 入力が空でもRunで閉じます。その場合はコードを実行しません。
- 入力内容はウィンドウを閉じても、同じDeckの実行中は保持します。
- 実行中のDeckと同じJavaScript環境を使うので、Deckのグローバル関数・変数と `app` / `fs` APIを利用できます。
- 表示中はカードのボタン操作、Enter／Escape、ファイルドロップを抑止します。
- Magicへ切り替えたり別Deckを開いた場合は入力ウィンドウを閉じます。Magicの編集画面ではCtrl + Spaceで開きません。

```javascript
app.setMagic(true);
```

成功時のメッセージは表示しません。スクリプトエラーは既存のエラーダイアログに表示します。入力コードはDeckに保存しません。

## アイコンと関連付け

実行ファイル用とDeck用の2種類のアイコンを `assets/scriptdeck.ico` / `assets/deck.ico` として同梱し、実行ファイルのICONリソースに組み込みます。各ICOは16／20／24／32／40／48／64／128／256pxを含みます。編集用SVGと確認用PNGも同梱しています。

```javascript
app.install();   // .deckの関連付けをユーザー単位で登録
app.uninstall(); // 自分の登録を解除
```

Ctrl + Spaceの入力欄から実行できます。

`app.install()` は実行中のEXEの場所を使って、以下を登録します。

- `.deck` のファイル種類 `ScriptDeck.Deck`。
- Deckのアイコン（EXE内の2番目のアイコン）。
- ファイルを開くコマンド `"EXEの絶対パス" "%1"`。空白・日本語を含むパスに対応。
- 「プログラムから開く」と既定のアプリ選択に使う候補情報。

登録先は `HKEY_CURRENT_USER` で、管理者権限は不要です。登録前のユーザー単位の拡張子設定を記録します。再登録時には最初の設定を保持し、EXEの場所を更新します。EXEを移動した場合は、新しい場所から `app.install()` を実行してください。

`app.uninstall()` は登録元のEXEと同じ場所から呼ばれた場合にのみ解除します。拡張子がまだScriptDeckを指していれば登録前の設定に戻します。他アプリに変更されていた場合はその設定を維持します。EXE、Deck、Home、ウィンドウ位置設定などのファイルは削除しません。

成功は `true` を返します。未登録、または別の場所のScriptDeckが登録元の場合、`uninstall` は `false` を返します。登録エラーは例外になります。成功時のダイアログは表示しません。

Windowsで既に別アプリを既定として選んでいる場合、登録だけで切り替わらないことがあります。その場合は `.deck` を右クリックし、「プログラムから開く」でScriptDeckを選んで既定に設定してください。WindowsのUserChoice設定を直接変更する処理は入れていません。

## ビルド

プロジェクト一式を更新し、Visual Studioで **Release / x64** をリビルドしてください。

EXEのバージョン情報を `1.0.0`、CMakeのプロジェクトバージョンを `1.0.0` に設定しました。DeckのJSON形式のversionは互換性維持のため従来どおり1です。

主な変更: `GuiView.h/.cpp`、`GuiWindow.cpp`、`ScriptEngine.h/.cpp`、`FileAssociation.h`、`WindowsAssociation.h`、`ResourceIds.h`、`ScriptDeck.rc`、アイコン、`ScriptDeck.vcxproj`、`CMakeLists.txt`。

## 確認状況

ヘッドレスGUIテストでサイズの長期安定性、実行前に入力ウィンドウとフォーカスを解除する順序、Ctrl + Space、1行入力、Run／Enter、Close／Escape、同じJS環境での実行、Magic切り替え、カードイベント抑止を確認します。レジストリ操作はメモリ上のテスト実装で、コマンド・アイコンパスの引用符、再登録、バックアップ復元、別EXEによる誤解除防止、他アプリの変更維持、登録失敗時の復元を確認します。

この環境ではWindows上のReleaseビルド、レジストリの実登録、Explorerのアイコン表示は未確認です。

参考: [ユーザー単位のファイル種類登録](https://learn.microsoft.com/en-us/windows/win32/shell/fa-file-types)、[関連付け変更の通知](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify)、[Windowsの既定アプリ選択](https://learn.microsoft.com/en-us/windows/apps/develop/windows-integration/default-apps-platform)。

## MagicからPlayerへ

Magicのツールバーの「Playerモードへ」で通常のPlayerに切り替えます。「実行プレビュー」とは別の機能です。編集中のDeckと未保存の変更を保持します。PlayerからMagicへ戻す場合はCtrl + Spaceで `app.setMagic(true);` を実行できます。
