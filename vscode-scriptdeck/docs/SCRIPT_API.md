# JavaScript・イベント

更新日：2026-10-12

JavaScriptエンジンはQuickJS-NGです。共有の組み込みAPIとして `app` / `fs` / `alert` を使用できます。Magicの通常の編集状態ではスクリプトを実行せず、Playerと通常DeckのMagic実行プレビューで実行します。FolderのMagicでは実行せず、コード編集も行いません。

## スクリプトの保存場所

通常Deckは`script`にコードを内蔵し、Folderは`scriptFile`で外部JSを参照します。
FolderのJSはScriptDeckを終了してVSCodeで編集し、Playerの再起動で読み込みます。
deckxへのExportでは最新の外部JSを内蔵へ変換します。保存形式によってイベントの仕様は変わりません。
専用イベントのないField・Text・Image・InputBox・TextEditorは、新規作成・複製・FolderへのExportで外部JSを生成しません。Deck・Cardとイベントのある部品のみ生成します。既存の非イベント部品のJSは自動削除しません。
詳細は[Deck形式・Export](FILE_FORMATS.md)を参照してください。

## イベント雛形の挿入

通常Magicでは、新規Deck・Card・イベントのある部品の作成時に初期コードを設定します。既存コードが空または空白・改行のみの場合も、スクリプト編集画面を開く際に雛形を挿入します。非空のコードは変更しません。

FolderへのExport・再Exportでは、空コードに対象の雛形を挿入して出力します。Deckは説明コメント、Cardは`openCard`・`dropFiles`、Buttonは`mouseUp`、Listbox・DropdownList・Checkbox・RadioButtonは`change`です。元のDeck・JSは変更せず、専用イベントのない部品の外部JSも作成しません。保存方法を含む操作別の一覧は[操作ガイド](USER_GUIDE.md#新規作成時のスクリプト雛形)を参照してください。

## 実行タイミング

- Deckスクリプト: Player起動時、Deckの新規読み込み時、Magicの実行プレビュー開始時にランタイムを生成し、1回実行します。カード移動だけでは再実行しません。共通関数や共有変数を定義できます。
- カードスクリプト: 起動時の最初のカード、および現在のカードが切り替わった時に実行します。コード全体を評価した後、openCard関数があれば呼びます。戻ってきたカードも再実行します。
- 両方ある場合はDeckスクリプト、その後カードスクリプトの順です。Magicの通常の編集状態では実行しません。

```javascript
// Deckスクリプト: 全カードから利用できる共通関数
function showMessage(text) { alert(text); }
```

```javascript
// カードスクリプト: このカードが開いた時
function openCard(event) {
    showMessage("カードを開きました");
}
```

カードスクリプトのローカル変数・関数は、そのカードスクリプトの呼び出し内のスコープです。共有するものはDeckスクリプトに置きます。Magicへ戻って編集するとランタイムを破棄し、次のプレビュー開始時にDeckスクリプトから再実行します。

## スクリプト直下のコードとイベント関数

カード・イベントを持つ部品では、イベントの配送ごとに**スクリプト全体を評価し、その後、対応するイベント関数があれば呼び出します**。関数の外に書いた処理も、そのたびに実行されます。ハンドラーが未定義でも、スクリプト直下の処理は実行します。

| 対象 | スクリプト全体を評価するタイミング | 評価後に呼ぶ関数 |
| --- | --- | --- |
| Deck | Player起動、Deck読み込み・切り替え、通常Magicのプレビュー開始によるランタイム生成時に1回 | 専用ハンドラーなし |
| Card | 起動時の表示、別カードへの移動（戻ってきた場合も含む） | `openCard(event)` |
| Card | カード領域へのファイル・フォルダのドロップごと | `dropFiles(event)` |
| Button | クリック、Enter／Escape対象ボタンの操作ごと | `mouseUp(event)` |
| Checkbox / RadioButton | ユーザー操作による状態変更の通知ごと | `change(event)` |
| Listbox / DropdownList | ユーザー操作による選択変更の通知ごと | `change(event)` |
| Field / Text / Image / InputBox / TextEditor | 現在、専用イベントによる自動実行なし | なし |

### Button：mouseUpがなくても直下の処理はクリック時に実行

```javascript
// Buttonスクリプト：クリックするたびに表示する。
alert("直下の処理");

function mouseUp(event) {
    alert("mouseUpの処理");
}
```

この例は、クリックごとに「直下の処理」→「mouseUpの処理」の順で表示します。`mouseUp`を削除しても「直下の処理」はクリックごとに実行します。ButtonスクリプトをPlayer起動時に一括実行する仕組みではありません。

### Card：ファイルドロップ時にも直下のコードを再実行

```javascript
// Cardスクリプト：カード表示時とファイルドロップ時の両方で実行。
alert("Cardスクリプト全体を評価しました");

function openCard(event) {
    // カードを開いた時だけ行う初期化はここへ書く。
    alert("カードを開きました");
}

function dropFiles(event) {
    alert(event.paths);
}
```

ファイルドロップ時は、直下の処理→`dropFiles`の順で実行し、`openCard`は呼びません。`dropFiles`を定義していなくても、カード領域へドロップすると直下のコードは評価されます。初期化処理を直下に書くとドロップ時にも繰り返されるため、カードを開いた時だけの処理は`openCard`内へ置いてください。ボタンをクリックしただけでCardスクリプト全体を再評価することはありません。

### Deck：直下の初期化はランタイム生成時に1回

Deckスクリプトは全体をグローバルスコープで評価します。カード移動・部品操作・ファイルドロップでは再実行しません。Deckの再読み込みや切り替え、Player再起動、通常Magicのプレビュー再開始では新しいランタイムを作り、再実行します。

```javascript
// Deckスクリプト：ランタイムごとの共有状態。
globalThis.clickCount = 0;

function countClick() {
    clickCount += 1;
    return clickCount;
}
```

Card・部品スクリプトのローカル変数はイベントごとに作り直されます。前回の値を保持したい状態はDeck側の共有変数、またはオブジェクトのプロパティに保存してください。直下で例外が発生した場合は、その後のイベント関数を呼ばずエラーになります。イベント引数の`event`はハンドラー関数へ渡されるので、イベント情報を使う処理はその関数内へ書いてください。

通常Deck・Folder・deckxでこの評価順序は共通です。Magicの編集状態では実行せず、FolderのMagicには実行プレビューもありません。

## 実装済みイベント

| 対象 | ハンドラ | タイミング |
| --- | --- | --- |
| カード | `openCard(event)` | 起動時のカード表示、別カードへの移動後。 |
| Button | `mouseUp(event)` | ボタンの操作、カードのEnter／Escape対象ボタン操作。 |
| Checkbox / RadioButton | `change(event)` | ユーザーによるチェック・選択状態の変更。 |
| Listbox / DropdownList | `change(event)` | ユーザーによる別項目への選択変更。 |
| カード | `dropFiles(event)` | 表示中のカードへのファイル・フォルダのドロップ。 |

Deck専用の開始・終了ハンドラ、カード終了ハンドラは現在未実装です。Deckスクリプト全体をランタイム開始時に評価することが、Deckの初期化処理になります。組み込み関数のopenDeck(path)はイベントではありません。

新規カードにはopenCard／dropFiles、新規ButtonにはmouseUp、選択・チェック系の新規部品にはchangeの雛形を設定します。既存の空のカードスクリプトを編集すると同じカード用の雛形、空のDeckスクリプトには開始時に実行される説明コメントを挿入します。操作方法は [USER_GUIDE.md](USER_GUIDE.md) を参照してください。

## 部品のクリック mouseUp

部品スクリプトに次を定義します。`this` と `event.target` は押されたボタン、`event.type` は `"mouseUp"` です。

```javascript
function mouseUp(event) {
    this.card.objectByName("message").text = "クリックしました";
}
```

マウス操作とカードで指定したEnter／Escapeボタンは同じイベントを発生させます。入力欄にフォーカスがある場合、Dropdownのポップアップ操作中はカードのEnter／Escape操作を無視します。非表示・無効のボタンへは配送しません。

## 状態変更 change

Checkbox／RadioButtonをユーザーが変更すると、部品スクリプトのchange(event)を呼びます。

```javascript
function change(event) {
    alert({ id: this.id, checked: event.checked, group: event.group });
}
```

- event.typeは"change"、event.targetとthisは対象部品です。
- event.checkedは操作時の状態のスナップショット。event.groupはRadioButtonのグループ（Checkboxは空文字）です。
- RadioButtonは解除された部品のchange(false)、選択された部品のchange(true)の順で通知します。選択済みRadioButtonを再度押してもイベントを出しません。
- スクリプトからの代入、Magicのプロパティ編集ではchangeを自動発生させません。
- Magicの編集状態ではイベントを実行しません。Player・実行プレビューのクリックやキーボード操作で変更できます。

### Listbox / DropdownListの選択変更

ユーザーが別の項目を選ぶと、対象部品の `change(event)` を呼びます。PlayerとMagicの実行プレビューで有効です。

```javascript
function change(event) {
    this.card.objectByName("message").text = event.selectedText;
    alert({ index: event.selectedIndex, text: event.selectedText });
}
```

| プロパティ | 内容 |
| --- | --- |
| `event.type` | `"change"`。 |
| `this` / `event.target` | 変更された部品。 |
| `event.selectedIndex` | 操作後の選択番号（0始まり）。 |
| `event.selectedText` | 操作後の選択文字列。 |
| `event.previousSelectedIndex` | 操作前の選択番号。未選択は-1。 |
| `event.previousSelectedText` | 操作前の選択文字列。未選択は空文字列。 |

選択情報は操作時点のスナップショットです。同じ項目の再選択、スクリプトからのselectedIndex／selectedText／itemsなどの変更、Magicのプロパティ編集では通知しません。イベントは描画後に配送するため、ハンドラ内で部品を変更・作成・削除できます。

## カードへのファイルドロップ

カードスクリプトにdropFiles(event)を定義してください。

```javascript
function dropFiles(event) {
    const list = this.objectByName("files");
    if (list) list.items = event.paths;
    alert({ paths: event.paths, x: event.x, y: event.y });
}
```

- event.typeは"dropFiles"。this / event.targetは受け取ったカード。
- event.pathsはUTF-8の絶対パス文字列配列。複数ファイルを1回のイベントで渡します。ドロップがフォルダの場合もそのパスを渡します。
- event.x / event.yはカード左上からの座標。
- PlayerとMagicの実行プレビューで、画面に見えているカード領域へのドロップだけを受け付けます。ツールバー・編集パネル・カード外は対象外です。
- 描画の完了後にイベントを実行するため、ハンドラ内で部品を作成・削除できます。部品の上にドロップしてもカードへ通知します。
- 受信後にカードが切り替わっても、ドロップ時のカードを対象にします。Deckを開き直したり新規作成した場合は古いイベントを破棄します。
- openCardを再度呼ぶことはありません。ただし既存の部品イベントと同様、カードスクリプト全体を評価してからdropFilesを呼ぶため、カードの初期化処理はopenCard関数内に置いてください。

## スコープと文字列コードの実行

Deckスクリプトのグローバル関数・変数はカードや部品のイベントから利用できます。カード・部品スクリプト全体は各イベントの呼び出し時にローカルスコープで評価されます。部品名は自動で変数になりません。`app.currentCard.objectByName()` などで検索してください。

`app.runCode(code)` は文字列をJavaScriptとして同期実行します。Deckで定義したグローバル関数・変数にアクセスできます。呼び出し元のイベント内のローカル変数は共有しません。strict evalのため、実行したコード内の宣言は呼び出し後のグローバル環境へ追加されません。

```javascript
// Deckスクリプト
function greet(name) { return "Hello, " + name; }
```

```javascript
// 部品イベント
function mouseUp(event) {
    alert(app.runCode('greet("ScriptDeck")'));
}
```

## 実行制限とエラー

JavaScriptメモリー上限は64 MiB、実行時間上限は2秒です。ダイアログやネイティブ入出力の待機時間は除外します。スクリプトエラーはソース名と詳細を含むダイアログで表示します。組み込み関数の失敗はtry/catchでも処理できます。

カード・Deckの移動は [NAVIGATION_API.md](NAVIGATION_API.md)、部品の読み書きは [OBJECT_API.md](OBJECT_API.md)、組み込みAPIは [APP_API.md](APP_API.md) と [FILE_API.md](FILE_API.md) を参照してください。

形式ごとの保存・相対パスの一覧は[Deck形式・Export](FILE_FORMATS.md)を参照してください。
