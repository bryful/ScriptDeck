# 組み込み関数

PlayerとMagicの実行プレビューで利用できます。相対パスは起動時のカレントフォルダ基準です。引数の型が不正な場合はTypeError、操作失敗は例外となり、try/catchで扱えます。

## 引数・実行・出力

| API | 動作 |
|---|---|
| `app.getArgs()` | 実行ファイル、Deckパス、起動オプションを除いた文字列配列。呼ぶたびにコピーを返す |
| `JSON.parse(text)` | JavaScript標準のJSON解析 |
| `JSON.stringify(value, null, indent)` | JavaScript標準のJSON生成 |
| `alert(object)` | 文字列化してOKダイアログ表示。循環参照にも対応 |
| `app.runCode(code)` | 文字列を同期実行し、最後の式の値を返す。結果なしはundefined |
| `app.write(text)` | UTF-8文字列を標準出力へ。改行・BOMなし |
| `app.writeLine(text = "")` | UTF-8文字列とLFを標準出力へ |
| `app.writeError(text = "")` | UTF-8文字列とLFを標準エラーへ |
| `app.readBytes(count)` | 標準入力から最大countバイトをUint8Arrayで取得。EOF時は短くなる |
| `app.writeBytes(bytes, offset = 0, count = bytes.length - offset)` | Uint8Arrayの指定範囲を標準出力へ |
| `app.readBinary()` | 標準入力をEOFまでまとめて読み、Uint8Arrayで返す |
| `app.writeBinary(bytes)` | Uint8ArrayまたはArrayBuffer全体を標準出力へ |
| `app.flush()` | 標準出力をフラッシュ |
| `app.setConsoleMode(enabled)` | コンソールへの接続／切り離し。再接続可能 |
| `app.setMagic(enabled)` | trueでMagic、falseでPlayer |
| `app.exit(code = 0)` | 現在のスクリプトを打ち切り、終了コードを指定して終了要求 |

write系・writeError・read系は必要に応じてコンソールを自動接続します。標準入出力のリダイレクトは維持します。文字列出力は文字列を渡してください。バイナリーには改行・BOM・ログを加えません。write系は出力後にフラッシュします。未保存の編集がある場合、exitは通常の終了と同じ破棄確認を行います。キャンセル時はアプリを継続しますが、打ち切ったコードの続きへは戻りません。終了コードは0～2147483647です。

runCodeは共有グローバルの関数・変数を参照できます。呼び出し元のローカル変数を参照せず、宣言したローカル変数・関数はその実行内だけに限定します。globalThisへの明示的な変更は同じDeckの実行環境内に残ります。コードは文字列に限定します。例外は呼び出し元へ渡します。

## ファイル

| API | 動作 |
|---|---|
| `fs.readText(path)` | UTF-8のBOM有り／無しを自動判定し、先頭BOMを除いて返す |
| `fs.writeText(path, text, withBOM = false)` | UTF-8で作成／上書き。trueはBOM有り |
| `fs.appendText(path, text, withBOM = false)` | 追記。BOM指定は新規・空ファイルの場合だけ適用 |
| `fs.readBytes(path)` | ファイル全体をUint8Arrayとして取得 |
| `fs.writeBytes(path, bytes)` | Uint8Arrayを作成／上書き |
| `fs.exists(path)` | ファイル・フォルダいずれかが存在するか |
| `fs.existsFile(path)` | ファイルが存在するか |
| `fs.existsDir(path)` | フォルダが存在するか |
| `fs.resolvePath(path)` | 絶対パスへ変換 |
| `fs.getFiles(path)` | 直下のファイルの絶対パス配列。再帰なし・名前順 |
| `fs.getDirectories(path)` | 直下のフォルダの絶対パス配列。再帰なし・名前順 |
| `fs.move(source, destination)` | 移動先フルパスを指定してファイル／フォルダを移動。別ドライブにも対応 |
| `fs.rename(path, newName)` | 同じ親フォルダ内で名前だけ変更 |
| `fs.delete(path, recursive = false)` | ファイル／空フォルダを削除。trueならフォルダの中身も削除 |

readTextはUTF-8専用です。UTF-16や不正なUTF-8は例外にします。他のエンコーディングはreadBytesを使ってください。appendTextは既存ファイルのBOM状態を維持し、途中にBOMを挿入しません。既存内容のエンコーディング変換は行いません。

move/renameは移動先が存在すれば上書きせず例外です。別ドライブのmoveはコピー完了後に移動元を削除するため、全体としては原子的な操作ではありません。移動元の削除に失敗した場合は移動先のコピーが残り、例外を返します。親フォルダは自動作成しません。存在しない対象のdeleteも例外です。リンクの再帰削除はリンク先をたどりません。WindowsのC:relative形式は受け付けず、C:/folder/fileのような絶対パスを指定してください。

## 音声

| API | 動作 |
|---|---|
| `app.beep(frequency = 800, durationMs = 200)` | Beep音。37～32767Hz、0～60000ms。完了まで待機 |
| `app.playWav(path, wait = false)` | WAVを再生。既定は非同期、trueなら完了まで待機 |
| `app.stopWav()` | 再生中のWAVを停止 |

WAVは同時に1つで、新しい再生は前の再生を置き換えます。終了時も停止します。ファイルがない場合や再生失敗時は例外です。

## 入出力の例

```javascript
const args = app.getArgs();
try {
    const input = JSON.parse(fs.readText(args[0]));
    const result = { count: input.items.length };
    fs.writeText(args[1], JSON.stringify(result, null, 2), true);
    app.writeLine(JSON.stringify(result));
    app.exit(0);
} catch (error) {
    app.writeError(String(error));
    app.exit(1);
}
```

```javascript
const bytes = app.readBinary();
app.writeBinary(bytes);
app.exit(0);
```

```javascript
// Deck共通の関数
function twice(value) { return value * 2; }
// 部品側などから呼び出す
alert(app.runCode("twice(15)"));
```

標準入力の読み込みや同期WAV再生は完了までGUIスレッドで待機します。readBinaryはEOFを送るファイル／パイプ向けです。読み込み・バイト列転送は1回32MiBまでです。大きな標準入力はreadBytesの繰り返しで処理できます。JavaScriptメモリー上限は64MiB、実行時間上限は2秒です。ダイアログやネイティブ入出力を待つ時間を除外します。Card/ObjectのプロパティアクセスAPIやイベントの階層配送は別段階です。

## カード・オブジェクト

`app.currentCard.objectByName(name)` / `objectById(id)`、`app.deck.cardByName(name)` / `cardById(id)`とライブプロパティを公開しています。項目数・選択番号・選択文字列・項目編集の仕様はOBJECT_API.mdを参照してください。

## ファイル選択ダイアログ

app.openFileDialog(options) / app.saveFileDialog(options)を追加。キャンセルはnull、Openのmultiple:trueではパス配列を返します。詳細はDAY2_API.md。

## パス文字列とウィンドウ操作

パス関数はグローバル関数、または `fs` のメソッドで利用できます。ファイルの存在確認やパスの正規化は行いません。区切りは `\` と `/` に対応します。

| 関数 | `C:\AAA\AAA_001.tga` の結果 |
| --- | --- |
| `getName(path)` | `AAA_001.tga` |
| `getNameWithoutExt(path)` | `AAA_001` |
| `getExt(path)` | `.tga` |
| `getParent(path)` | `C:\AAA` |
| `getFrame(path)` | `001` |
| `getNameWithoutFrame(path)` | `AAA_` |

連番は拡張子を除いた名前の末尾の数字列です。数字列がない場合 `getFrame` は空文字列、`getNameWithoutFrame` は拡張子を除いた名前を返します。拡張子がない場合 `getExt` は空文字列です。`.hidden` は拡張子なしとして扱います。親がない相対名の `getParent` は空文字列、ルート直下は `C:\` や `/` を返します。

ウィンドウ関数はグローバル関数、または `app` のメソッドで利用できます。

```javascript
app.setTopMost(true);     // 常に最前面
alert(app.getTopMost());  // 現在の状態（boolean）
app.setTopMost(false);    // 通常の重なり順へ戻す
app.windowFront();        // 最小化を解除し、前面へ移動
```

TopMostは実行中の状態で、Deckには保存しません。`windowFront` はTopMostを変更しません。別アプリが操作中の場合、Windowsの制限によりフォーカス移動が許可されない場合があります。


## Windows / Unix形式のパス変換

`toWindowsPath(path)` / `fs.toWindowsPath(path)` と `toUnixPath(path)` / `fs.toUnixPath(path)` を追加。

```javascript
toWindowsPath("/c/work/aaa/bbb.tga"); // "C:\\work\\aaa\\bbb.tga"
toUnixPath("C:\\work\\aaa\\bbb");  // "/c/work/aaa/bbb"
```

`/c/`形式のドライブ文字をWindowsでは大文字、Unix形式では小文字にします。`/c` は `C:\\` へ変換。UNCパスは `//server/share/file` と `\\\\server\\share\\file` を相互変換します。相対パスは区切りだけ変換し、ファイル名・拡張子・日本語・末尾の区切りは維持します。ファイルアクセス、カレントディレクトリ補完、`.` / `..` の解決はしません。`/mnt/c`などのWSLマウント形式は特別扱いしません。

カード移動・Deck切り替え・Home起動については [NAVIGATION_API.md](NAVIGATION_API.md) を参照してください。

Version 1.0のPlayerスクリプト入力、アイコン、`app.install()` / `app.uninstall()` は [VERSION1.md](VERSION1.md) を参照してください。

## アプリケーション・フォルダのフルパス

すべて引数なしで、フルパスの文字列を返します。フォルダパスには通常、末尾の区切り文字を付けません（ドライブのルートを除く）。取得だけではファイル・フォルダを作成しません。

| API | 戻り値 |
| --- | --- |
| `app.getHomePath()` | Homeとして使用する `home.deck` のフルパス。 |
| `app.getDeckPath()` | 現在開いているDeckのフルパス。未保存の新規Deckなら空文字列。 |
| `app.getExePath()` | 実行中の `ScriptDeck.exe` のフルパス。 |
| `app.getDocumentPath()` | WindowsのDocumentsの設定先。OneDriveなどへの移動にも対応。 |
| `app.getDocumentsPath()` | `app.getDocumentPath()` の別名。 |
| `app.getTempPath()` | 現在のプロセスが使用する一時フォルダ。 |
| `app.getAppDataPath()` | ScriptDeck用の設定フォルダ。通常は `C:\Users\ユーザー名\AppData\Local\ScriptDeck`。 |

```javascript
alert({
    home: app.getHomePath(),
    deck: app.getDeckPath(),
    exe: app.getExePath(),
    documents: app.getDocumentPath(),
    temp: app.getTempPath(),
    appData: app.getAppDataPath()
});
const deckFolder = fs.getParent(app.getDeckPath());
```

`getDeckPath()` は呼び出した時点のDeckを返します。`changeDeck()`／`goHome()` はイベント処理後に反映されるため、その呼び出し直後の同じイベント内では移動前のパスです。移動先の `openCard` では移動先のパスを返します。PlayerとMagicの実行プレビューで利用できます。
