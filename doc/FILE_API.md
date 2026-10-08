# ファイル・パスAPI

更新日：2026-10-08

PlayerとMagicの実行プレビューで利用できます。相対パスは起動時のカレントフォルダ基準です。不正な引数はTypeError、操作失敗は例外になります。

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

## OpenFileDialog / SaveFileDialog

```javascript
const path = app.openFileDialog(); // 単一ファイルの絶対パス、またはnull
const paths = app.openFileDialog({
    title: "画像を開く", multiple: true,
    initialDirectory: ".",
    filters: [
        { name: "画像", pattern: "*.png;*.jpg;*.jpeg" },
        { name: "すべて", pattern: "*.*" }
    ]
}); // パス配列、またはキャンセル時null

const destination = app.saveFileDialog({
    title: "結果を保存", defaultName: "result.txt", defaultExtension: "txt",
    filters: [{ name: "テキスト", pattern: "*.txt" }]
});
if (destination !== null) fs.writeText(destination, "Test", true); // UTF-8 BOM付き
```

| オプション | 型・動作 |
|---|---|
| title | string、ダイアログのタイトル |
| initialDirectory | string、初期フォルダ。相対パスは起動時のカレントフォルダ基準 |
| defaultName | string、初期ファイル名。フォルダはinitialDirectoryで指定 |
| defaultExtension | string、既定拡張子。先頭のドットは省略可 |
| filters | `{name, pattern}`の配列。省略時は全ファイル |
| multiple | boolean、Openのみ。既定値false |

キャンセルは両APIともnullです。Openのmultiple=trueの場合だけ戻り値が配列になります。Saveはパスを選ぶだけで、ファイルの作成・書き込みは行いません。既存ファイルへの保存先指定はWindowsの上書き確認を表示します。実際の書き込みはfs.writeText / fs.writeBytesを呼び出してください。

ダイアログ表示中はスクリプトが同期的に待ち、待っている時間はJavaScriptの2秒実行制限に含めません。選択後に処理を続けます。カレントフォルダは変更しません。引数の型・未知のオプションが不正なら例外になり、try/catchで処理できます。

Windows実装はIFileOpenDialog / IFileSaveDialogを使用しています。[MicrosoftのGetResults仕様](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileopendialog-getresults)に従って複数のパスを取得します。

## パス文字列の操作

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

## Windows / Unix形式のパス変換

`toWindowsPath(path)` / `fs.toWindowsPath(path)` と `toUnixPath(path)` / `fs.toUnixPath(path)` を追加。

```javascript
toWindowsPath("/c/work/aaa/bbb.tga"); // "C:\\work\\aaa\\bbb.tga"
toUnixPath("C:\\work\\aaa\\bbb");  // "/c/work/aaa/bbb"
```

`/c/`形式のドライブ文字をWindowsでは大文字、Unix形式では小文字にします。`/c` は `C:\\` へ変換。UNCパスは `//server/share/file` と `\\\\server\\share\\file` を相互変換します。相対パスは区切りだけ変換し、ファイル名・拡張子・日本語・末尾の区切りは維持します。ファイルアクセス、カレントディレクトリ補完、`.` / `..` の解決はしません。`/mnt/c`などのWSLマウント形式は特別扱いしません。

カード移動・Deck切り替え・Home起動については [NAVIGATION_API.md](NAVIGATION_API.md) を参照してください。

Version 1.0のPlayerスクリプト入力、アイコン、`app.install()` / `app.uninstall()` は [USER_GUIDE.md](USER_GUIDE.md) を参照してください。

アプリケーションや設定フォルダのフルパス取得は [APP_API.md](APP_API.md#アプリケーションフォルダのフルパス) を参照してください。


## ファイルサイズ・日時

| API | 戻り値 |
| --- | --- |
| `fs.getFileSize(path)` | ファイルサイズ（バイト数、number）。フォルダ指定は例外。 |
| `fs.getFileTimes(path)` | `{ createdTime, modifiedTime, accessedTime }`。 |
| `fs.getFileTimestamp(path)` | 更新日時だけを取得。getFileTimes(path).modifiedTimeと同じ。 |
| `fs.getFileInfo(path)` | `{ path, size, isDirectory, createdTime, modifiedTime, accessedTime }`。pathはフルパス、フォルダのsizeはnull。 |

日時はUTCのUnix時刻をミリ秒で返し、`new Date(value)` に渡せます。取得不能な作成日時はnullです。Windowsでは作成・更新・アクセス日時を取得します。非Windowsのテスト版は作成日時をnullとして返します。アクセス日時の更新頻度はOSとファイルシステムの設定に依存します。

```javascript
const info = fs.getFileInfo("C:\\work\\image.tga");
alert(info.size);
alert(new Date(info.modifiedTime).toLocaleString());
if (info.createdTime !== null) alert(new Date(info.createdTime));
```

存在しないパスや読み取り失敗は例外です。相対パスは起動時のカレントフォルダ基準です。サイズはJavaScriptのnumberのため、2^53を超える値の整数精度は保証しません。


## Deckファイルとの使い分け

fs.readText／writeTextはファイル内容を読み書きするだけで、現在のDeckや画面は切り替えません。Deckとして読み込む・保存するにはopenDeck／saveDeck／saveAsDeckを使用してください。詳細は [NAVIGATION_API.md](NAVIGATION_API.md) にまとめています。環境変数と外部プロセスは [APP_API.md](APP_API.md) を参照してください。
