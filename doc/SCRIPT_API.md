# JavaScript・イベント

更新日：2026-10-08

JavaScriptエンジンはQuickJS-NGです。共有の組み込みAPIとして `app` / `fs` / `alert` を使用できます。Magicの通常の編集状態ではスクリプトを実行せず、PlayerとMagicの実行プレビューで実行します。

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
