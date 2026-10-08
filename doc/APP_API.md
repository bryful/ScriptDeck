# アプリケーション・入出力API

更新日：2026-10-08

PlayerとMagicの実行プレビューで利用できます。不正な引数はTypeError、操作失敗は例外になります。

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

write系・writeError・read系は必要に応じてコンソールを自動接続します。標準入出力のリダイレクトは維持します。文字列出力は文字列を渡してください。バイナリーには改行・BOM・ログを加えません。write系は出力後にフラッシュします。未保存の変更がある場合、exitは通常の終了と同じ処理を行います。Magicでは破棄確認、Playerでは現在のDeckへ自動保存します。確認のキャンセル・保存の失敗時はアプリを継続しますが、打ち切ったコードの続きへは戻りません。終了コードは0～2147483647です。

runCodeは共有グローバルの関数・変数を参照できます。呼び出し元のローカル変数を参照せず、宣言したローカル変数・関数はその実行内だけに限定します。globalThisへの明示的な変更は同じDeckの実行環境内に残ります。コードは文字列に限定します。例外は呼び出し元へ渡します。

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

標準入力の読み込みや同期WAV再生は完了までGUIスレッドで待機します。readBinaryはEOFを送るファイル／パイプ向けです。読み込み・バイト列転送は1回32MiBまでです。大きな標準入力はreadBytesの繰り返しで処理できます。JavaScriptメモリー上限は64MiB、実行時間上限は2秒です。ダイアログやネイティブ入出力を待つ時間を除外します。部品のプロパティは [OBJECT_API.md](OBJECT_API.md)、イベントは [SCRIPT_API.md](SCRIPT_API.md) を参照してください。

## Player / Magicの切り替え

```javascript
app.setMagic(true);  // Player → Magic
app.setMagic(false); // Magic → Player
```

引数はbooleanを1つ。戻り値はundefined。同じモードへの呼び出しは何もしない。
切り替えでは編集中のDeck・カード・部品と未保存状態を維持し、自動保存しません。Playerはカードサイズの固定ウィンドウ、Magicは編集UIを持つリサイズ可能ウィンドウです。



## app.setConsoleMode

app.setConsoleMode(true)で実行中に親コンソールへ接続し、親がなければ作成します。falseでScriptDeckの接続を解除し、単独で作成したコンソールを閉じます。親のcmd/PowerShellは終了しません。再度trueを呼べます。標準入力・出力・エラーを再接続し、ファイル・パイプへのリダイレクトは切り替え後も維持します。無効中の非リダイレクト出力はNULへ送ります。デッキの保存状態・Player/Magicは変えません。



## ウィンドウ操作

ウィンドウ関数はグローバル関数、または `app` のメソッドで利用できます。

```javascript
app.setTopMost(true);     // 常に最前面
alert(app.getTopMost());  // 現在の状態（boolean）
app.setTopMost(false);    // 通常の重なり順へ戻す
app.windowFront();        // 最小化を解除し、前面へ移動
```

TopMostは実行中の状態で、Deckには保存しません。`windowFront` はTopMostを変更しません。別アプリが操作中の場合、Windowsの制限によりフォーカス移動が許可されない場合があります。

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

## テキストのクリップボード

| API | 動作 |
| --- | --- |
| `app.readClipboard()` | クリップボードのテキストを文字列で取得。テキストがなければ空文字列。 |
| `app.writeClipboard(text)` | テキストを設定。引数は文字列。戻り値はundefined。空文字列で空のテキストを設定。 |

```javascript
app.writeClipboard("日本語のテキスト\r\n2行目");
const text = app.readClipboard();
alert(text);
```

WindowsのUnicodeテキスト形式を使用し、日本語・絵文字・改行に対応します。画像やファイル一覧の読み書きには対応しません。書き込みは既存のクリップボード内容を置き換えます。NUL文字を含む文字列、文字列以外、32 MiBを超えるUTF-8テキストは受け付けません。読み取りも32 MiBまでです。他のアプリが使用中など、クリップボードを開けない場合は例外になります。必要に応じてtry/catchで処理してください。



## 環境変数

`app.getEnv(name)` は環境変数の値を文字列で取得します。存在しなければnull、存在するが空なら空文字列です。WindowsではUnicodeで取得します。空の名前、`=`、NULを含む名前は例外です。変更用のAPIはありません。

```javascript
alert(app.getEnv("USERPROFILE"));
const value = app.getEnv("MY_SETTING") ?? "既定値";
```

## 外部プロセス

| API | 動作 |
| --- | --- |
| `app.launchProcess(executable, args = [])` | 起動後、終了を待たず戻る。戻り値はundefined。起動失敗は例外。 |
| `app.runProcess(executable, args = [])` | 起動して終了まで待ち、標準出力を文字列で返す。起動失敗は例外。 |

```javascript
app.launchProcess("notepad.exe", ["C:\\work\\memo.txt"]);
const result = app.runProcess("C:\\tools\\converter.exe", ["input.txt", "--json"]);
alert(result);
```

実行ファイルと引数を別々に指定してください。引数は文字列配列で、空白・引用符・日本語を含む引数も各要素で渡します。引数に手動で引用符を追加する必要はありません。シェルを自動的に介さないため、`>`、`|`、`&`などは通常の引数です。コマンドやバッチは明示的に `cmd.exe` などを呼び出してください。

```javascript
const result = app.runProcess("cmd.exe", ["/d", "/c", "echo Test"]);
```

子プロセスの作業フォルダはScriptDeck起動時のカレントフォルダ、環境変数は現在のプロセスから継承します。実行ファイル名だけならOSの検索規則で探し、相対パスを含む場合は作業フォルダを基準にします。起動したプロセスはScriptDeck終了後も独立して実行できます。

runProcessの標準出力はUTF-8として扱い、先頭のUTF-8 BOMを除去します。改行はそのまま保持します。出力先アプリもUTF-8出力に設定してください。取得上限は32 MiBです。標準エラーは取得せず破棄し、標準入力は空（EOF）です。終了コードが非ゼロでも、取得した標準出力を返します。

終了待ち中はGUIスレッドも待機します。この待機時間はJavaScriptの2秒制限から除外し、プロセス自体にタイムアウトは設定しません。Windowsではコンソール用プロセスの新規コンソールウィンドウを表示しません。GUIアプリ自身のウィンドウは表示できます。


## 関連するAPI

- Deckの新規作成・読み込み・保存・移動： [NAVIGATION_API.md](NAVIGATION_API.md)。
- カード・部品へのアクセス： [OBJECT_API.md](OBJECT_API.md)。
- ファイル選択・保存ダイアログ、サイズ・日時： [FILE_API.md](FILE_API.md)。
- `app.install()` / `app.uninstall()` によるDeck関連付け： [USER_GUIDE.md](USER_GUIDE.md)。
