# Player・Magicの使い方

更新日：2026-10-12

通常起動はPlayerです。保存形式と制作手順の詳細は[Deck形式・Export](FILE_FORMATS.md)を参照してください。

## 起動と編集対象

| 起動例 | 動作 |
| --- | --- |
| `ScriptDeck.exe` | AppDataのHomeをPlayerで開く |
| `ScriptDeck.exe example.deck` | 通常DeckをPlayerで開く |
| `ScriptDeck.exe MyDeck` | FolderをPlayerで開く |
| `ScriptDeck.exe example.deckx` | deckxをPlayerで開く |
| `ScriptDeck.exe --magic example.deck` | 通常DeckをMagicで編集。不存在なら作成 |
| `ScriptDeck.exe --magicFolder MyDeck` | FolderをMagicで画面編集。不在のフォルダなら作成 |

`-magic`は`--magic`の別名です。`--magic`のみの起動はHomeをMagicで開きます。
`--magicFolder`のみの場合は親フォルダを選択し、`Untitled`フォルダを作成します。
FolderのコードはScriptDeckを終了してからVSCodeで編集し、Playerを再起動して確認します。
Folderにはコード編集ボタン・実行プレビューがなく、deckxはMagicで開けません。

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
app.setMagic(true); // deckxでは使用不可
```

成功時のメッセージは表示しません。スクリプトエラーは既存のエラーダイアログに表示します。入力コードはDeckに保存しません。

## アイコンと関連付け

実行ファイル用・Deck用・deckx用の3種類のアイコンを `assets/scriptdeck.ico` / `assets/deck.ico` / `assets/deckx.ico` として同梱し、実行ファイルのICONリソースに組み込みます。deckx用ICOは16／24／32／48／64／128／256pxを含みます。編集用SVGと確認用PNGも同梱しています。

```javascript
app.install();   // .deckと.deckxの関連付けをユーザー単位で登録
app.uninstall(); // 自分の登録を解除
```

Ctrl + Spaceの入力欄から実行できます。更新後は新しいEXEから`app.install()`を再実行してください。

`app.install()` は実行中のEXEの場所を使って、以下を登録します。

- `.deck` のファイル種類 `ScriptDeck.Deck`、`.deckx`のファイル種類`ScriptDeck.Deckx`。
- Deckのアイコン（EXE内の2番目）とdeckxのアイコン（3番目）。
- ファイルを開くコマンド `"EXEの絶対パス" "%1"`。空白・日本語を含むパスに対応。
- 「プログラムから開く」と既定のアプリ選択に使う候補情報。

登録先は `HKEY_CURRENT_USER` で、管理者権限は不要です。登録前のユーザー単位の拡張子設定を記録します。再登録時には最初の設定を保持し、EXEの場所を更新します。EXEを移動した場合は、新しい場所から `app.install()` を実行してください。

`app.uninstall()` は登録元のEXEと同じ場所から呼ばれた場合にのみ解除します。拡張子がまだScriptDeckを指していれば登録前の設定に戻します。他アプリに変更されていた場合はその設定を維持します。EXE、Deck、Home、ウィンドウ位置設定などのファイルは削除しません。

成功は `true` を返します。未登録、または別の場所のScriptDeckが登録元の場合、`uninstall` は `false` を返します。登録エラーは例外になります。成功時のダイアログは表示しません。

Windowsで既に別アプリを既定として選んでいる場合、登録だけで切り替わらないことがあります。その場合は `.deck` または `.deckx` を右クリックし、「プログラムから開く」でScriptDeckを選んで既定に設定してください。WindowsのUserChoice設定を直接変更する処理は入れていません。

## ビルドと配布

ビルド手順は [ルートREADME.md](../README.md) を参照してください。バイナリー配布時はルートの `LICENSE` と `THIRD_PARTY_NOTICES.md` を同梱してください。

## MagicからPlayerへ

Magicのツールバーの「Playerモードへ」で通常のPlayerに切り替えます。「実行プレビュー」とは別の機能です。編集中のDeckと未保存の変更を保持します。通常・FolderのPlayerからMagicへ戻す場合はCtrl + Spaceで `app.setMagic(true);` を実行できます。deckxでは使用できません。FolderのMagicからPlayerへ移る際は外部JSを読み込み、参照切れがあれば切り替えを中止します。


## 終了時の未保存変更

Magicでは、従来どおり未保存の変更があると破棄確認ダイアログを表示します。

Playerでは未保存の変更を現在のDeckファイルへ自動保存して終了します。フィールド入力、選択状態、Checkbox／RadioButton、スクリプトからのプロパティ変更・部品作成・削除、カード移動も変更として記録します。変更がない場合は書き込みません。ウィンドウを閉じた場合とapp.exit()の両方に適用します。

保存に失敗した場合はエラーを表示し、終了せず未保存の内容を保持します。保存先がまだないDeckは保存ダイアログを表示します。キャンセルした場合は終了しません。Magicでの未保存内容をPlayerへ切り替えた場合も、終了時に自動保存します。

openDeck()やchangeDeck(path,false)で明示的に破棄して移動した内容は自動保存されません。保存のAPIについては [NAVIGATION_API.md](NAVIGATION_API.md) を参照してください。


## 通常Deckのスクリプト編集

プロパティ内の「部品スクリプトを編集」「カードスクリプトを編集」「Deckスクリプトを編集」を押すと、プレビュー領域をコード編集画面に切り替えます。プロパティと通常のツールバーを隠し、作業領域全体を使用します。編集中は対象切り替え・実行プレビュー・カード操作・ファイルドロップを停止します。

- Tab：タブ文字でインデント。
- Ctrl+S：「保存」と同じDeck保存。
- Ctrl+Shift+S：「別名保存」。
- 「編集を完了」：カード表示とプロパティへ戻る。ファイルへの自動保存は行いません。

編集中の文字は対象のスクリプトへ随時反映し、変更があればタイトルに `*` を表示します。編集中の終了にもMagicの未保存確認が適用されます。英数字はConsolas、日本語はMeiryoで表示します。Consolasが利用できない場合は通常のフォントを使用します。


空（空白・改行のみも含む）のカードスクリプトを編集するときは、次の雛形を自動挿入します。

```javascript
function openCard(event) {
}

function dropFiles(event) {
}
```

空のDeckスクリプトには次の説明を挿入します。

```javascript
//ここに記述したものがDeckオープン時に実行されます
```

既存のコードには追加しません。雛形を挿入した場合は未保存の変更として扱い、通常の保存操作でDeckへ保存します。


## Homeから新規Deckを作成

内蔵Homeの「新規Deck」ボタンは、新しい未保存Deckを作成してMagicへ切り替えます。作成前のHomeは保存し、新規Deckの保存先は通常の「保存」「別名保存」で指定します。スクリプトAPIは `app.newDeck()`（グローバルの `newDeck()` も使用可）です。切り替えはイベント処理後に反映します。

内蔵Homeの「新規フォルダーデッキ」は親フォルダを選択し、その下に`Untitled`を作成します。
`main.deck`と外部JSを生成してFolderのMagicへ切り替えます。キャンセル時は何も変更しません。
既存の`Untitled`は上書きせずエラーにします。Deck名は作成後にMagicで変更できます。
スクリプトAPIは`app.newFolderDeck()`です。既存のHomeは自動更新しないため、必要なら同じ呼び出しのボタンを追加してください。

Magicのツールバーには「Homeを開く」を追加しました。未保存の変更があれば従来の破棄確認を表示し、MagicのままHomeを開きます。

すでにAppDataに保存されているHomeは自動上書きしません。既存Homeへ追加する場合は、内蔵Homeと同様にボタンのmouseUpで `app.newDeck()` を呼び出してください。


カード・Deckの移動と保存APIは [NAVIGATION_API.md](NAVIGATION_API.md)、スクリプトの実行タイミングと各イベントは [SCRIPT_API.md](SCRIPT_API.md) を参照してください。


## 新規作成時のスクリプト雛形

編集画面を開く前に、新規作成の時点で次のコードを保存データへ設定します。Magicの新規作成、Homeの新規Deck、起動引数の新規Deck、スクリプトからの部品作成に共通です。

| 対象 | 初期スクリプト |
| --- | --- |
| Deck | Deckオープン時に実行することを説明するコメント。 |
| カード | 空の `openCard(event)` と `dropFiles(event)`。 |
| Button | 空の `mouseUp(event)`。 |
| Listbox / DropdownList / Checkbox / RadioButton | 空の `change(event)`。 |
| Field / InputBox / Text / Image / TextEditor | 専用イベントなし。通常Deckには説明コメントを設定し、Folderでは外部JSを生成しません。 |

通常MagicとFolderへのExportでの雛形の扱いは次のとおりです。

| 操作 | 雛形を設定する条件 | 保存先 |
| --- | --- | --- |
| 通常Magicで新規Deck・Card・部品を作成 | 作成時に対象の初期コードを設定 | 通常Deckの`script`。保存操作でファイルに反映 |
| 通常Magicで既存のスクリプト編集を開く | コードが空、または空白・改行のみなら対象の雛形を挿入 | 通常Deckの`script`。未保存の変更として扱う |
| Folderで新規作成 | Deck・Card・イベントのある部品に初期コードを設定 | `script/`内のJS。既定名のファイルがあれば既存内容を使用 |
| FolderへExport・再Export | 空、または空白・改行のみなら対象の雛形を挿入 | 出力先のJS。元のDeck・JSは変更しない |

既存Deckを読み込むだけではコードを書き換えません。非空の既存コードには雛形を追加しません。通常Magicで専用イベントのない部品を選んだ場合は、部品スクリプトの編集ボタンを表示しません。Deck・Cardとイベントのある部品は、コードが空でも編集ボタンを表示します。

部品複製は元のコードを保持します。通常Deckでは`createObject`で`script`を指定した場合はその指定を優先します。Folderでは`script`の指定・書き換えを拒否します。イベントのある部品は外部JSを使用し、専用イベントのない部品は外部JSを作成しません。


## CLIでのDeck操作

`-run`はGUIを開かずに実行し、DeckのJavaScriptは評価しません。
入力Deckには通常・Folder・deckxを指定できます。

| コマンド例 | 動作 |
| --- | --- |
| `ScriptDeck.exe -run example.deck info` | 名前・画面サイズ・カード数・部品数などをJSON出力 |
| `ScriptDeck.exe -run example.deck dump` | Deck本体をJSON出力。リソースは出力しない |
| `ScriptDeck.exe -run example.deck validate` | 読み込み・検証結果を出力 |
| `ScriptDeck.exe -run example.deck copy output.deck` | 通常Deckを新しいパスへ保存。画像のコピーや参照調整は行わない |
| `ScriptDeck.exe -run example.deck exportDeckx output.deckx` | 画像・data配下を含めdeckxへExport |

`copy`は通常Deck向けです。Folder・deckxの保存先変更にはExportを使用します。
`validate`は読み込みと形式の検証で、JavaScriptの実行や全画像の存在確認を行うものではありません。
CLIの出力先が存在する場合は上書きしません。

## オブジェクトの複製

Magicの「部品複製」で新しいID・名前を割り当て、位置を少しずらして追加します。
通常Deckでは内蔵コードを複製します。Folderでは最新の外部JSを別ファイルへコピーし、
コピー先に既存ファイルがあれば複製を中止します。カード・部品を削除しても外部JSは残します。
Folderの名前変更と保存の扱いは[Deck形式・Export](FILE_FORMATS.md)を参照してください。

## FolderからVSCodeで編集する

FolderのMagicにある「VSCodeを開く」は、現在の編集を保存し、Deckフォルダ全体を
VSCodeの新しいウィンドウで開いてScriptDeckを終了します。ボタンのツールチップにも動作を表示します。
保存またはVSCodeの起動に失敗した場合はエラーを表示し、ScriptDeckを開いたままにします。
VSCodeは標準のインストール先、App Paths登録、またはPATHの`code`から検索します。
コードを編集した後は、Folderを指定してPlayerを起動してください。

## VSCode拡張

Folder Deck用の拡張は[vscode-scriptdeck](../vscode-scriptdeck/README.md)にあります。VSCodeの拡張機能画面の「…」→「VSIXからのインストール」で`scriptdeck-0.1.3.vsix`をインストールし、`main.deck`のあるフォルダーを開いてください。

コマンドパレットの「ScriptDeck: ScriptDeck.exeを登録」で実行ファイルを選択します。選んだパスは`scriptdeck.executablePath`としてユーザー設定に保存します。既存のワークスペース設定がある場合はそちらを更新します。登録コマンドは実行ファイルの移動・更新後の変更にも使用できます。

未設定の場合は、新規Folder作成・再生・Magic・エクスポート時に実行ファイルの選択ダイアログを表示します。キャンセルすると設定を変更せず、操作を中止します。

再生・Magic・終了・新規Folder作成・deck/deckx出力・APIリファレンス表示もコマンドパレットから操作できます。Deckツリーから外部JSを開き、API補完とホバー説明を利用できます。起動・出力前にVSCodeのファイルを保存します。終了コマンドは拡張から起動したウィンドウのみ対象とし、Magicの未保存確認を維持します。

CLIで通常Deckへ変換する場合は`ScriptDeck.exe -run folder exportDeck output.deck`を使用します。外部JSを内蔵に変換し、参照リソースもコピーします。

### VSCodeから開くMagicの上部ボタン

VSCode拡張から新規Folder作成・Magic起動を行うと、上部ボタンは「保存」「< 前」「次 >」のみ表示します。カード・部品の追加、配置、プロパティ編集は従来どおり利用できます。通常起動のMagicでは従来のボタンを表示します。CLIでは`ScriptDeck.exe --magicFolder --vscode "C:\Decks\MyDeck"`で同じ表示にできます。`--vscode`はDeckパスの前に指定してください。

### 新規カード・オブジェクトの名前

Magicで追加ボタンを押すと、作成前に名前を入力するダイアログを表示します。初期値は`Card1`、`button1`、`image1`など英語の連番名です。使用済みの初期値は避けて候補を表示します。日本語の名前も入力できます。

入力された名前が同じカード内の既存オブジェクトと一致する場合は、名前を自動変更せずエラーを表示し、作成しません。カード名も既存カードと同名ならエラーです。空の名前も作成できません。エラー後は名前を修正して再試行でき、キャンセルではオブジェクトや外部JSを作成しません。既存オブジェクトの名前は変更しません。

### イベント関数の外に書いた処理

カード・部品のイベント配送では、スクリプト全体を評価してから`openCard`・`mouseUp`・`change`・`dropFiles`のうち対応する関数を呼びます。関数がなくても、直下の処理は実行されます。Cardはファイルドロップ時にも全体を評価するため、カードを開く時だけの初期化は`openCard`内へ書いてください。Deckの直下はランタイム生成時に1回実行し、カード移動や部品操作だけでは再実行しません。対象ごとの詳しいタイミングと例は[JavaScript・イベント](SCRIPT_API.md#スクリプト直下のコードとイベント関数)を参照してください。

### 外部JSを作成する部品

FolderではDeck・Cardと、Button・Checkbox・RadioButton・Listbox・DropdownListに外部JSを作成します。専用イベントのないField・Text・Image・InputBox・TextEditorは、新規作成・複製・FolderへのExportでJSを作成しません。既存ファイルは自動削除しません。詳細は[Deck形式・Export](FILE_FORMATS.md)を参照してください。

通常DeckのMagicでも、専用イベントのないField・Text・Image・InputBox・TextEditorを選択した場合は「部品スクリプトを編集」ボタンを表示しません。イベントのある部品はコードが空でも編集ボタンを表示し、Deck・Cardの編集ボタンも引き続き表示します。

### FolderへのExport時の雛形

FolderへのExportでも、コードが空または空白・改行のみの場合は、Deckの説明コメント、Cardの`openCard`・`dropFiles`、Buttonの`mouseUp`、選択・チェック系の`change`の雛形を出力先のJSに挿入します。既存の非空コードには追加・書き換えをしません。元のDeckやJSも変更しません。専用イベントのない部品は引き続き外部JSを生成しません。
