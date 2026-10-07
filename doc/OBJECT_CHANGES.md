# 今回の変更

- ScriptModel.h/.cpp: 現在のDeckをIDで解決するプロパティ接続。検索、型・値検証、項目操作、変更通知。
- ScriptEngine.h/.cpp: app.deck / app.currentCard、カード・オブジェクトのProxy、this / event.target。
- GuiView.h / GuiWindow.cpp: 実行中のデータへの接続、Magicの未保存表示、イベントの対象参照。
- ScriptDeck.vcxproj / CMakeLists.txt: 新規ソースとModelTestsの登録。
- OBJECT_API.md: API仕様と使用例。
- object-test.deck: 文字列更新、Listbox/DropdownList選択、項目追加の確認用Deck。

新しいScriptDeck.slnを使ってリビルドしてください。Magicでは実行プレビューを有効にするとスクリプトを実行できます。

確認: LinuxでQuickJS-NGを実際に実行し、検索・プロパティ・項目操作・参照寿命・不正値・イベント対象をテスト。従来のJavaScript/BuiltinTestsも成功。ImGuiのGUIテストで入力フォーカス、Enter/Escape、選択、Player/Magicの変更通知と保存を確認。Windows実機でのビルドと表示は未確認です。

## 動的オブジェクトの追加

ScriptModel.h/.cppとScriptEngine.cppに作成・削除・複製・一覧取得・順序変更を追加。削除したIDの実行中再利用を防止し、削除時のEnter/Escape設定を解除します。作成プロパティは追加前に検証します。CMakeにDynamicTestsを登録し、dynamic-test.deckを同梱しました。既存.vcxprojはScriptModel.cppをすでに含むためソース登録の変更はありません。

## Magicの並べ替えUI

GuiView.cppのカード一覧・部品一覧へ「上へ」「下へ」を追加。先頭・末尾・未選択の場合は移動を無効化します。現在カードと選択部品のIDを維持し、変更はタイトルの*と保存内容に反映します。部品一覧に描画順の説明を付けました。

## プロパティラベルの見切れ修正

GuiView.cppで文字・リスト項目・選択番号のラベルを入力欄の上へ移動し、幅いっぱいの入力欄から右へ押し出されないようにしました。

## Checkbox / RadioButton・ダイアログ・ファイルドロップ

- CardObject.h / Card.h / Stack.cpp: checked/groupとRadioButtonの排他制御、JSON保存・旧形式読み込み。
- ScriptModel.cpp: 新プロパティ、動的作成・複製時の排他制御。
- GuiView.h/.cpp: 新部品の編集・描画・changeキュー、カード領域でのドロップ判定。
- ScriptEngine.h/.cpp: change/dropFilesとイベントデータ、Open/SaveFileDialogサービス。
- FileDialogs.h/.cpp: オプション検証、WindowsのIFileOpenDialog/IFileSaveDialog。
- GuiWindow.cpp: ExplorerのWM_DROPFILES受信、複数パス取得、描画後のイベント配送。
- .vcxproj / CMakeLists.txt: 新規ファイル登録とテスト。
- DAY2_API.md / day2-test.deck: 説明と操作例。

QuickJS・ImGuiで状態・イベント・排他・保存・ドロップ領域・ダイアログの接続契約を検証。Windows実機のダイアログ表示とExplorerからの実ドロップは未確認です。
