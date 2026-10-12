# ScriptDeck for VSCode

Folder Deckのコード編集を支援するWindows用拡張です。`main.deck`のあるフォルダーを開いて使用します。

## インストール

1. VSCodeの拡張機能画面の「…」→「VSIXからのインストール」で`scriptdeck-0.1.3.vsix`を指定します。
2. 「ScriptDeck: ScriptDeck.exeを登録」を実行し、実際の`ScriptDeck.exe`を選択します。
3. `main.deck`のあるフォルダーを開きます。

選んだパスは`scriptdeck.executablePath`としてユーザー設定に保存します。既存のワークスペース設定がある場合はそちらを更新します。設定から直接パスを指定することもできます。通常Deckへの出力には、本リポジトリの`exportDeck`コマンドと`--vscode`起動オプションに対応した実行ファイルが必要です。

未設定の場合は、新規フォルダーデッキ作成・再生・Magic・エクスポート時に選択ダイアログを表示します。キャンセルすると設定を変更せず、操作を中止します。登録済みの実行ファイルを変更する場合も登録コマンドを使用してください。

## 操作

コマンドパレットから以下を実行できます。

- ScriptDeck: ScriptDeck.exeを登録 — 実行ファイルを選択・保存。登録済みのパスも変更できます。
- ScriptDeck: 新規フォルダーデッキ — 親フォルダーと名前を指定し、Magicで作成。保存後に新しいフォルダーをVSCodeで開いてください。
- ScriptDeck: 再生
- ScriptDeck: Magicで開く — 上部ボタンを保存・前・次に絞って表示。カードや部品の編集は通常どおり操作できます。
- ScriptDeck: 終了 — この拡張から起動したウィンドウに通常の終了を要求。未保存確認をキャンセルすると起動状態を維持します。
- ScriptDeck: deckへエクスポート
- ScriptDeck: deckxへエクスポート
- ScriptDeck: APIリファレンスを開く

起動・出力前にファイルを保存します。エクスプローラーのScriptDeckツリーからDeck・カード・オブジェクトのJSを開けます。main.deckの変更でツリーを更新します。複数Deckの場合は編集中のファイルから対象を判定し、それ以外は選択します。

Folder Deck配下のJavaScriptにAPI補完・ホバー説明、カード名・オブジェクト名の候補を表示します。JSだけを開いた場合も、親フォルダーの`main.deck`から対象を判定します。`fs.`や`app.`を入力すると候補が表示されます。手動で候補を開く場合は`Ctrl+Space`を使用してください。言語モードは標準のJavaScriptで、拡張子の変更は不要です。API情報は同梱リファレンスを基にしています。完全な型検査やJavaScript解析ではありません。

ScriptDeck実行中はコードを編集せず、終了後に編集し、再起動してください。拡張は起動中の編集を警告しますが、エディターをロックしません。VSCodeの再読み込み後は、以前起動したScriptDeckをウィンドウから終了してください。未信頼ワークスペースでは無効です。

## 開発・パッケージ作成

依存パッケージのインストールは不要です。Node.jsで`npm test`、Pythonで`python package.py`を実行します。VSIXはこのフォルダーに生成します。APIや文書を変更した場合は`python update-api.py`でdocsとapi.jsonを更新してください。

VSCodeのExtension Development Hostでの表示・操作は手動確認してください。
