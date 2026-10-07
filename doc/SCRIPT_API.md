# JavaScript・イベント

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

## 部品のクリック mouseUp

部品スクリプトに次を定義します。`this` と `event.target` は押されたボタン、`event.type` は `"mouseUp"` です。

```javascript
function mouseUp(event) {
    this.card.objectByName("message").text = "クリックしました";
}
```

マウス操作とカードで指定したEnter／Escapeボタンは同じイベントを発生させます。入力欄にフォーカスがある場合、Dropdownのポップアップ操作中はカードのEnter／Escape操作を無視します。非表示・無効のボタンへは配送しません。

## 状態変更 change

ユーザーが状態を変更すると、部品スクリプトのchange(event)を呼びます。

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
