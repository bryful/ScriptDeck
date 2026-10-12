# Deck形式・Folder・deckxとExport

更新日：2026-10-12

[ドキュメント目次](readme.md) · [操作ガイド](USER_GUIDE.md)

## 形式の選び方

Player／Magicは実行・編集のモード、通常／Folder／deckxは保存形式です。

| 項目 | 通常の`.deck` | Folder | `.deckx` |
| --- | --- | --- | --- |
| 保存単位 | UTF-8 JSONファイル | フォルダ全体 | 無圧縮ZIPファイル |
| Deck名 | JSON内の`name` | フォルダ名 | 内部Deckの`name` |
| Deck本体 | 任意名の`.deck` | `main.deck`固定 | ZIP直下の`main.deck` |
| スクリプト | `script`に内蔵 | `scriptFile`で外部JS参照 | Export時に内蔵へ変換 |
| Magic | 画面・コード編集 | 画面・プロパティ編集のみ | 使用不可 |
| Player | 使用可能 | 使用可能 | 使用可能 |
| 主な用途 | 単独ファイルで制作 | VSCodeと交互に制作 | リソースをまとめて配布 |

形式変更はExportで別の出力先を作成します。通常の保存では形式を変えません。
Export元のDeck・ファイルは保持します。既存の出力先は上書きしません。

## Exportとリソース

Magicの「Export: Folder」「Export: 通常deck」「Export: deckx」を使用します。
通常→Folderでは、選択した親フォルダの下にDeck名のフォルダを作成します。
Folder→通常・deckxでは、最新の外部JSを読み込み、内蔵スクリプトへ変換します。
Folder出力時に生成するJS名が重複する場合はエラーです。

画像オブジェクトの`imagePath`で参照する外部画像を、絶対・相対パスともに
出力先の`data/`へコピーし、出力側の参照を相対パスに変更します。
同じ画像は一度だけコピーし、同名の別画像には連番を付けます。
元Deckの`data/`内の音声・データファイルも階層を保ってコピーします。
参照切れや既存出力ファイルとの衝突は、出力開始前にエラーにします。
JSコード中に記述された`data/`外のパスは自動収集・書き換えしません。

## 相対パスの基準

| 対象 | 通常・Folder | deckx |
| --- | --- | --- |
| 画像の`imagePath` | Deck本体のあるフォルダ | 展開先フォルダ |
| 外部JSの`scriptFile` | Folderの`main.deck`があるフォルダ | 外部JSを含むパッケージでは展開先 |
| `fs`、WAV、Deck切り替えなどのスクリプトAPI | 起動時のカレントフォルダ | 展開先フォルダ |
| `app.getDeckPath()` | Deck本体の絶対パス | 元のdeckxの絶対パス |

通常・Folderでは、Deckの場所とスクリプトAPIの相対パス基準が異なる場合があります。
Folderを起動する際は、必要に応じてそのフォルダをカレントフォルダにしてください。

## Folderモードと外部JavaScript

`ScriptDeck.exe --magicFolder "C:\Decks\MyDeck"` でFolder形式を画面編集します。
指定フォルダが存在しない場合は新規作成します。既存フォルダはFolder形式の
`main.deck`を含む必要があります。引数を省略すると親フォルダを選択し、その下に
`Untitled`を作成します。既存の`Untitled`は上書きしません。
`--magic`は従来の`-magic`と同じです。Playerはフォルダまたは`main.deck`を指定して起動できます。

```text
MyDeck/
  main.deck
  script/
    deck.js
    Card 1.js
    Card 1_button.js
  data/
```

Folder名がDeck名、Deckファイル名は`main.deck`固定です。
`main.deck`の`storage: "folder"`で形式を識別し、スクリプトは`scriptFile`で
`script/`内のUTF-8 JSを相対参照します。通常形式は従来どおり`script`にコードを内蔵します。

FolderのMagicは画面・オブジェクト・プロパティ専用です。コード編集ボタンと
実行プレビューは表示せず、スクリプトも実行しません。コードはScriptDeckを終了してから
VSCodeで編集し、実行確認時にPlayerを起動してください。
FolderのMagicの「VSCodeを開く」は保存してDeckフォルダを開き、ScriptDeckを終了します。自動再読み込みは行いません。
JSが不足していてもMagicで画面編集できますが、Playerでの実行はエラーになります。
通常保存はJSファイルの内容を上書きしません。

新規Deck・カード・オブジェクトの作成時に、空の場合も含めJSファイルを作成します。
既定名のファイルがある場合は上書きせずリンクします。同名のオブジェクトはコードを共有します。
ファイル名に使用できない文字は`_`に置換します。
名前変更はEnterで確定し、関連JSも改名します。改名先の衝突、参照元の不足、共有JSの改名は
エラーとなり、名前変更全体を中止します。コード中に書かれた名前やIDは書き換えません。
Deck名変更はフォルダ自体を改名します。名前変更確定時には、参照との整合性を保つため
現在の画面編集内容を含め`main.deck`も保存します。

「部品複製」は新規ID・名前を割り当て、位置をずらします。Folderでは最新のJSを
別ファイルへコピーし、コピー先が存在すれば複製を中止します。
カード・オブジェクトを削除してもJSファイルは残します。
FolderのPlayerでは実行中の`script`・`name`の変更は拒否します。

形式変更とリソース同梱は、上記の「Exportとリソース」を参照してください。

## deckx形式：ExportとPlayer再生

通常形式・Folder形式のMagicにある「Export: deckx」から、新しい`.deckx`を生成します。
元のDeckやリソースは変更しません。出力先が存在する場合はエラーです。
CLIでは`ScriptDeck.exe -run source.deck exportDeckx output.deckx`を使用できます。
Folderの外部JSは最新の内容を読み込んで内蔵し、ZIP直下の`main.deck`へ保存します。
画像と`data/`内の音声・データファイルも同梱します。

```text
Example.deckx（無圧縮ZIP）
  main.deck
  data/
    image.png
    sounds/click.wav
```

`ScriptDeck.exe "Example.deckx"`またはダブルクリックでPlayer再生します。
開くダイアログと新規作成されるHomeのOpenボタンでも`.deckx`を選択できます。
既存のカスタマイズ済みHomeは上書きしません。
Magic起動、Magicから開く操作、`app.setMagic(true)`による移行は拒否します。

Player起動時に専用の一時フォルダへ展開し、終了・Deck切替後に解放します。
画像の相対パスと、deckx内で実行するスクリプトの`fs`・音声などの相対パスは
展開先基準です。`app.getDeckPath()`は元の`.deckx`のパスを返します。
入力値などの変更は通常どおり保存・終了時の自動保存で`main.deck`へ反映し、
リソースを含む無圧縮ZIPを一時ファイルに作成して元のdeckxを置換します。
保存できない場合は元のdeckxを保持してエラーにします。保存先変更はExportを使用します。

標準ZIPのSTORE方式、UTF-8ファイル名に対応します。ZIP64・圧縮・暗号化・分割ZIPは非対応で、
ファイルサイズは512 MiBまでです。不正なパス、重複した名前、CRC不一致などは拒否します。
同じdeckxを複数のPlayerで同時編集する用途は想定していません。


専用アイコンと関連付けの登録・解除は[操作ガイド](USER_GUIDE.md#アイコンと関連付け)を参照してください。
