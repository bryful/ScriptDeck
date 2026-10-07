# カードとDeckの移動

PlayerとMagicの実行プレビューで利用できます。グローバル関数と `app` のメソッドは同じ動作です。

## 起動時のHome

Deckを指定せずに起動すると、Player／Magicともに `%LOCALAPPDATA%\ScriptDeck\home.deck` を開きます。

ファイルがなければ、実行ファイル内のRCDATAリソース `assets/home.deck` から初期Homeを保存して開きます。初期HomeにはDeckを選んで開くボタンがあります。Homeを編集・保存した後は、その内容を使います。存在するHomeが壊れている場合はエラーを表示し、自動上書きしません。

## カード移動

| 関数 | 動作 |
| --- | --- |
| `nextCard()` | 次のカード |
| `prevCard()` | 前のカード |
| `topCard()` | 先頭のカード |
| `endCard()` | 最後のカード |
| `goCardIndex(index)` | 0始まりのカード番号で移動 |
| `goCard(nameOrId)` | カードIDまたは名前で移動 |

```javascript
app.nextCard();
app.prevCard();
app.topCard();
app.endCard();
app.goCardIndex(0);
app.goCard("カード名");
app.goCard("card2");
```

現在のカード順に従います。端での移動は循環しません。移動した場合は `true`、端や現在のカードを指定した場合は `false` を返します。番号の範囲外、不正な型、存在しない名前／IDは例外です。

IDが一致するカードを優先し、なければ名前を完全一致で検索します。同名のカードが複数ある場合は例外となるのでIDを指定してください。

カード変更は即時に `app.currentCard` へ反映されます。実行中の `this` は元のカード／オブジェクトを指します。移動先のカードスクリプトは次のイベント処理フレームに実行し、`openCard(event)` を呼びます。同じカードへの移動では再実行しません。複数回連続して移動した場合、途中のカードの `openCard` は実行せず、最終的な移動先に対して実行します。Deckスクリプトはカード移動だけでは再実行しません。

## Deck移動

```javascript
goHome();                         // 現在のDeckを保存してHomeへ
goHome(false);                    // 現在のDeckの変更を破棄してHomeへ
changeDeck("C:\\work\\sample.deck");
app.changeDeck("other.deck", false);
```

`goHome(isSave=true)` は共通のHomeを開きます。Homeを削除していた場合は初期Homeを再作成します。

`changeDeck(path, isSave=true)` は指定されたDeckを開きます。相対パスの基準は既存の `fs` APIと同じ起動時のカレントディレクトリです。

`isSave` はこの呼び出しにおける移動前のDeckの自動保存指定です。`true` はPlayerでのスクリプトによる変更も含めて保存します。`false` は保存せずに切り替えます。現在のDeckに保存先がない場合（Magicの新規Deck）は、先に保存するか `false` を指定してください。自動保存では保存先選択ダイアログを表示しません。

Deck切り替えは安全に行うため、現在のスクリプト処理の終了後に反映します。呼び出し直後のコードはまだ元のDeckに対して動作します。

```javascript
changeDeck("next.deck");
// この処理中は元のDeck。切り替えは処理が正常終了した後。
```

移動先を先に読み込み検証し、現在の処理終了時点のDeckを保存して切り替えます。同じファイルへの移動では、保存後の内容を再読み込みします。移動先の読み込み失敗は呼び出し時の例外です。自動保存に失敗した場合はエラーダイアログを表示し、現在のDeckを維持します。スクリプトが例外で終了した場合、予約された切り替えを取り消します。

同じ処理中に複数のDeck切り替えを予約することや、予約後のカード移動は例外です。Deck切り替え後は旧Deckのイベントを破棄し、JavaScript実行環境を作り直します。元のカード／オブジェクト参照やグローバル変数を新Deckへ引き継ぎません。新DeckのDeckスクリプト、続いて現在カードの `openCard(event)` を実行します。Player／Magicのモードは維持します。

Playerのカードサイズ・タイトルと、Deck別の位置設定も更新します。Homeの位置設定もHome自身のパスで識別します。TopMostの状態はDeck切り替えで維持します。

## 更新ファイル

`ScriptEngine.h`、`ScriptEngine.cpp`、`GuiView.h`、`GuiView.cpp`、`GuiWindow.cpp`、`HomeDeck.h`、`ResourceIds.h`、`ScriptDeck.rc`、`assets/home.deck`、`ScriptDeck.vcxproj`、`CMakeLists.txt`。

Resourceとプロジェクトの追加があるため、ZIPのプロジェクト一式を更新してリビルドしてください。
