# ScriptDeck

更新日：2026-10-12

![ScriptDeck icon](ScriptDeck/assets/scriptdeck.png)

ScriptDeckは、HyperCardに着想を得たWindows向けのカード型アプリケーション作成ツールです。
カードにボタン・入力欄・画像などを配置し、JavaScriptで動作を記述します。
Playerで実行し、Magicで画面やプロパティを編集します。

## VSCode機能拡張のインストール

Folder DeckのコードをVSCodeで編集する場合は、ScriptDeck拡張をインストールしてください。

1. [ScriptDeck拡張のVSIX](vscode-scriptdeck/scriptdeck-0.1.1.vsix)を保存します。
2. VSCodeを開き、左側の「拡張機能」を選択します（`Ctrl+Shift+X`）。
3. 拡張機能画面の「…」から「VSIXからのインストール…」を選び、保存した`scriptdeck-0.1.1.vsix`を指定します。再読み込みを求められた場合は実行してください。
4. `Ctrl+Shift+P`でコマンドパレットを開き、「ScriptDeck: ScriptDeck.exeを登録」を実行して、使用する`ScriptDeck.exe`を選びます。
5. VSCodeの「ファイル」→「フォルダーを開く」で、編集する`main.deck`のあるフォルダーを開きます。

実行ファイルのパスが未設定の場合は、新規作成・再生・Magic・エクスポート時にも選択ダイアログを表示します。キャンセルすると操作を中止します。登録したパスは設定に保存され、次回から使用します。実行ファイルを移動した場合は登録コマンドで選び直してください。

コマンドパレットから再生・Magic・終了・deck/deckxへのエクスポートを操作できます。エクスプローラーのScriptDeckツリーから外部JSを開き、API補完やホバー説明を利用できます。

コードはScriptDeckを終了してから編集し、再生し直して確認してください。新しいFolder Deckは「ScriptDeck: 新規フォルダーデッキ」で作成できます。詳しくは[VSCode拡張の操作説明](vscode-scriptdeck/README.md)を参照してください。

## 保存形式と制作方法

| 形式 | 用途 | コード編集 |
| --- | --- | --- |
| `.deck` | 単独のJSONファイルで制作 | Magic内のエディタ |
| Folder | `main.deck`・外部JS・リソースを分けて制作 | ScriptDeckを終了してVSCodeで編集 |
| `.deckx` | 無圧縮ZIPへまとめて配布・再生 | Magic不可。元のDeckを編集して再Export |

MagicのExportで形式を変換し、画像と`data/`内のファイルを出力先へコピーします。
Playerでの入力などは保存対象です。deckxも内部の`main.deck`を更新して保存します。
詳細は[Deck形式・Folder・deckxとExport](doc/FILE_FORMATS.md)を参照してください。

## 主な機能

- カード・部品の作成、複製、配置、並べ替え、プロパティ編集。
- Button、Field、Text、Image、Listbox、DropdownList、InputBox、Checkbox、RadioButton、TextEditor。
- ボタン・状態変更・カード表示・ファイルドロップのJavaScriptイベント。
- 部品の検索・プロパティ操作・動的作成、カード移動とDeck切り替え。
- ファイル／フォルダ選択、UTF-8・バイナリー入出力、WAV再生、外部プロセス起動。
- PlayerのCtrl+Spaceによるコード実行、`.deck`と`.deckx`のアイコン・関連付け。

## 起動例

`ScriptDeck.exe`は、実際のEXEのパスに置き換えてください。

```bat
rem HomeをPlayerで開く
ScriptDeck.exe

rem 通常Deckを編集（-magicも使用可）
ScriptDeck.exe --magic example.deck

rem Folderを画面編集。コードはVSCodeで編集する
ScriptDeck.exe --magicFolder "C:\Decks\MyDeck"

rem Folder・deckxをPlayerで開く
ScriptDeck.exe "C:\Decks\MyDeck"
ScriptDeck.exe example.deckx

rem GUIを開かずdeckxへExport・検証
ScriptDeck.exe -run example.deck exportDeckx example.deckx
ScriptDeck.exe -run example.deckx validate
```

通常起動でDeck未指定の場合は`%LOCALAPPDATA%\ScriptDeck\home.deck`を開きます。
Homeがなければ内蔵データから作成し、既存Homeは上書きしません。
Player終了時は変更を自動保存し、保存失敗時は終了を止めます。Magicは未保存の破棄確認を表示します。
`-run`は情報取得・JSON出力・検証・コピー・deck/deckxへのExport用で、DeckのJavaScriptは実行しません。

## ドキュメントとサンプル

使い方・APIの入口は[ドキュメント目次](doc/readme.md)です。

- [操作ガイド](doc/USER_GUIDE.md)：起動、Magicの編集、保存、アイコンと関連付け。
- [Deck形式・Export](doc/FILE_FORMATS.md)：通常・Folder・deckx、外部JS、リソース同梱、相対パス。
- [JavaScript・イベント](doc/SCRIPT_API.md)：コードの実行タイミングとイベント。
- [変更履歴](doc/HISTORY.md)：実装と検証の記録。

サンプルDeckは`sampleDeck/`にあります。ビルド・テスト・拡張のパッケージ作成については[開発ガイド](doc/DEVELOPMENT.md)を参照してください。

## ライセンス

ScriptDeck本体は **MIT License** で公開します。著作権表示は `Copyright (c) 2026 bryful` です。[LICENSE](LICENSE) を参照してください。

同梱する第三者コード・データは、それぞれのライセンスに従います。

| コンポーネント | ライセンス |
| --- | --- |
| Dear ImGui 1.91.9b | MIT |
| Dear ImGui同梱のstb、ProggyClean | MIT（stbはMITの選択肢を使用） |
| Dear ImGuiの日本語字形範囲の元データ | CC BY 4.0 |
| QuickJS-NG 0.16.2 | MIT |
| QuickJS-NG同梱のUnicodeデータ | Unicode License V3 |
| nlohmann/json 3.12.0 | MIT |

著作権表示とライセンス全文・帰属情報は [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) にまとめています。バイナリーを配布する場合も、`LICENSE` と `THIRD_PARTY_NOTICES.md` を同梱してください。
