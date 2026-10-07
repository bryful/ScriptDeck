# カード・オブジェクトAPI

PlayerとMagicの実行プレビューで利用できます。読み取りは現在のデータを返し、代入は画面へ反映されます。Magicの変更は未保存としてタイトルに `*` を付け、通常の保存対象になります。Playerの変更は実行中だけ保持し、自動保存しません。

```javascript
function mouseUp(event) {
    const message = app.currentCard.objectByName("message");
    if (message) message.text = "Test";
    // ボタン自身は this、所属カードは this.card
    this.enabled = false;
}
```

## 検索と参照

| API | 意味 |
|---|---|
| `app.currentCard` | 現在のカード。読み取り専用 |
| `app.deck` | Deckへの参照。読み取り専用 |
| `app.deck.cardById(id)` / `cardByName(name)` | カード検索 |
| `card.objectById(id)` / `objectByName(name)` | そのカード内のオブジェクト検索 |
| `object.card` | 所属カード。カード移動後も所属先を保持 |
| `this` / `event.target` | mouseUpではボタン、openCardではカード |

検索は完全一致・大文字小文字を区別し、見つからなければ `null`。同一カード内に同名オブジェクトが複数ある場合、名前検索は例外になります。IDで区別してください。カード名の重複も同様です。同じIDを別カードで使えます。同じ対象への検索は同じJS参照を返します。

オブジェクト・カードを削除した後、またはDeckを開き直した後の古い参照へのアクセスは例外になります。イベント内のローカル変数とオブジェクトの名前は別物です。名前から自動でグローバル変数を作りません。

## オブジェクトのプロパティ

| プロパティ | 型 / 読み書き |
|---|---|
| `id`, `type` | string、読み取り専用。typeはbutton / field / text / image / listbox / dropdownlist / inputbox / checkbox / radiobutton |
| `name`, `text`, `script` | string、読み書き |
| `x`, `y` | 有限のnumber、読み書き |
| `width`, `height` | number、1～8192、読み書き |
| `visible`, `enabled` | boolean、読み書き |
| `imageSource` | `"file"` または `"resource"`、読み書き |
| `imagePath`, `resourceId` | string、読み書き |
| `backgroundColor`, `textColor`, `borderColor`, `tintColor` | `[r,g,b,a]`、各値0～1、読み書き。`null`で既定色に戻す |

プロパティは既存の描画仕様に従います。例えばimagePathはImageに、textColorは文字を描く種類に適用されます。InputBoxのtextに改行を入れると例外になります。読み取り専用・未知のプロパティへの代入、不正な型や範囲は例外になり、値は変更しません。

## Listbox / DropdownList

```javascript
const list = app.currentCard.objectByName("list");
list.items = ["赤", "緑", "青"];
list.selectedIndex = 1;
alert({ count: list.itemCount, index: list.selectedIndex, text: list.selectedText });
list.addItem("白");
list.setItem(0, "RED");
```

| プロパティ / メソッド | 動作 |
|---|---|
| `items` | string配列、読み書き |
| `itemCount` | 項目数、読み取り専用 |
| `selectedIndex` | 選択番号。0始まり、`-1`で未選択。読み書き |
| `selectedText` | 選択項目の文字列、未選択は空文字。代入は最初の一致項目を選択、存在しない値は例外 |
| `text` | selectedTextと同じ読み書き |
| `getItem(index)` | 項目文字列を取得 |
| `setItem(index, text)` | 項目変更。選択中ならselectedTextも更新 |
| `addItem(text)` | 末尾へ追加し、新しい0始まりの番号を返す |
| `insertItem(index, text)` | 指定位置へ追加し、番号を返す。末尾への挿入も可能 |
| `removeItem(index)` | 項目削除。選択中の項目を削除すると未選択になる |
| `clearItems()` | 全項目削除し未選択にする |

insertItem/removeItemは選択している項目を保つよう番号を調整します。items全体の代入は現在の選択番号を保持し、範囲外なら未選択にします。項目操作はスクリプトを再実行したりmouseUpを発生させたりしません。

配列・色の読み取りはコピーです。`list.items.push("追加")`や`object.textColor[0] = 1`だけでは更新されません。配列全体を代入するか、項目操作メソッドを使ってください。

## カード / Deck

| 対象 | 読み書き可能なプロパティ | 読み取り専用 |
|---|---|---|
| Card | name, script, width, height, backgroundColor, enterButtonId, escapeButtonId | id, objectCount |
| Deck | name, script, width, height, currentCardId, startupPlacement | cardCount |

CardとDeckのwidth/heightは同じDeck全体のカードサイズです。整数1～8192で指定でき、Playerのウィンドウサイズも追従します。カードごとに異なるサイズは持ちません。カードのbackgroundColorはRGBA配列で指定してください。Enter/EscapeにはそのカードのButton ID、無効化には空文字を指定します。

`app.deck.currentCardId = "card2"`で移動し、次のイベント処理で移動先のopenCardを呼びます。参照を保持したオブジェクトの所属先は変わりません。startupPlacementはdefault / previous_default / previous_center / centerです。

scriptへの代入は保存データを変更します。実行中のハンドラは継続し、オブジェクト・カードスクリプトの変更は次回の呼び出しから適用します。Deckスクリプトはランタイム生成時だけ実行するため、変更後の再実行にはDeckを開き直すか実行プレビューを入り直してください。

## 動的作成・削除・複製

```javascript
const card = app.currentCard;
const message = card.createObject("text", {
    name: "message", text: "Test", x: 20, y: 20, width: 300, height: 40,
    textColor: [1, 0, 0, 1]
});
const button = card.createObject("button", {
    name: "deleteButton", text: "削除", x: 20, y: 80,
    script: 'function mouseUp() { this.remove(); }'
});
const copy = message.clone({ name: "messageCopy", y: 130 });
copy.bringToFront();
message.remove();
alert(message.exists); // false
```

| API | 動作 |
|---|---|
| `card.createObject(type, properties = {})` | 作成してオブジェクト参照を返す |
| `card.objects` | 現在の全オブジェクト参照の配列。配列はコピー |
| `card.objectAt(index)` | 0始まりの順番で取得。範囲外は例外 |
| `card.removeObject(objectOrId)` | 所属オブジェクトまたはIDを指定して削除。削除成功はtrue、存在しないIDはfalse |
| `object.remove()` | 自分を削除。成功はtrue、削除済みはfalse |
| `object.exists` | 存在すればtrue、削除済みならfalse。読み取り専用 |
| `object.clone(properties = {})` | 同じカードに複製。新しい参照を返し、指定プロパティで上書き |
| `object.index` | 現在の0始まりの並び順。読み取り専用 |
| `object.moveToIndex(index)` | 指定順へ移動。範囲外は例外 |
| `object.bringToFront()` | 配列末尾へ移動し、最後に描画 |
| `object.sendToBack()` | 配列先頭へ移動し、最初に描画 |

typeは `button`, `field`, `text`, `image`, `listbox`, `dropdownlist`, `inputbox`, `checkbox`, `radiobutton` のいずれかです。propertiesには既存の書き込み可能なプロパティを指定できます。作成・複製時のみ `id` も指定可能で、省略すれば自動発行します。name省略時も新しいIDになります。通常の代入によるid/typeの変更はできません。

名前の重複は許容します。IDは空文字・NULを含む文字列を禁止し、カード内で一意です。同じ実行中に使用したIDを、削除後に再利用することはできません。これにより古い参照が新しい別の部品を指すことを防ぎます。Deckを開き直すと過去の参照も破棄されます。

propertiesの検証は追加前に行い、不正値なら何も追加しません。リストではitemsを先に適用、selectedIndexを最後に適用します。複製は項目・色・スクリプトもコピーし、その後の変更は互いに独立します。作成・複製は配列末尾に追加します。全オブジェクトを削除する場合は次の形で書けます。

```javascript
for (const object of app.currentCard.objects) object.remove();
```

削除は対象カードのEnter/Escapeボタン設定も解除します。削除済み参照のexists/remove/card以外へのアクセスは例外になります。別カードのオブジェクトをremoveObjectへ渡すと例外になります。すでにキューに入った削除済みボタンのイベントは実行しません。

自分を削除したスクリプトは現在の処理を最後まで継続します。`this.card`は削除後も所属カードを返すため、新しい部品の作成などに利用できます。削除による自動イベント発生はありません。

変更は次の描画から表示されます。Magicでは作成・削除・複製・並べ替えも未保存扱いとなり、保存するとDeckに反映します。Playerでは実行中のデータを変更し、Deckへ自動保存しません。動的部品のスクリプトも既存部品と同じデッキのグローバル関数を利用できます。scriptには関数オブジェクトでなくコード文字列を指定します。

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

状態変更イベントは [SCRIPT_API.md](SCRIPT_API.md#状態変更-change) を参照してください。
