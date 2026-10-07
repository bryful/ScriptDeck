# Checkbox / RadioButton・ファイルダイアログ・ドロップ

同梱の更新済みScriptDeck.sln / .vcxprojでリビルドしてください。新規ファイルはFileDialogs.h/.cppです。通常のPlayer、またはMagicの実行プレビューでday2-test.deckを動かせます。

## Checkbox / RadioButton

Magicの「部品を追加」にcheckbox / radiobuttonを追加しました。「チェック状態」を編集でき、RadioButtonは「グループ」も編集できます。位置・サイズ・文字・色・表示・有効状態と、動的作成・複製・削除は既存部品と同じです。

```javascript
const card = app.currentCard;
const checkbox = card.createObject("checkbox", {
    name: "option", text: "オプションを使う", checked: true,
    x: 20, y: 20, width: 260, height: 32
});
const radioA = card.createObject("radiobutton", {
    name: "radioA", text: "A", group: "choice", checked: true
});
const radioB = card.createObject("radiobutton", {
    name: "radioB", text: "B", group: "choice"
});
radioB.checked = true; // radioA.checkedはfalseになる
alert(checkbox.checked);
```

checkedはbooleanで既定値false。RadioButtonのgroupはstringで既定値空文字です。同じカード・同じgroupは最大1つだけcheckedになります。空文字同士も1グループです。別カードの同名groupとは独立しています。スクリプトでは全RadioButtonを未選択にすることもできます。非表示・無効のRadioButtonも排他処理の対象です。

チェック状態を保ったRadioButtonのgroup変更・動的作成・複製でも、移動先グループの他のRadioButtonを解除します。checked/groupはDeckに保存し、古いDeckにフィールドがなくても既定値で読み込めます。同一グループに複数のcheckedを含む不正なDeckは読み込みを拒否します。

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

## 検証範囲

LinuxでQuickJS-NGの実エンジンとImGuiの描画・入力を使い、状態・排他制御・change/dropFiles・オプション・Unicodeパス・キャンセル・ダイアログ待機時間の扱い・Magic保存を検証しました。従来のスクリプト・組み込み関数・動的作成・並べ替えも回帰テスト済みです。Windowsでの実ダイアログ表示とExplorerからの実ドロップは未確認です。
