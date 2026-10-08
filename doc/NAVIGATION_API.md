# カードとDeckの移動

更新日：2026-10-08

PlayerとMagicの実行プレビューで利用できます。グローバル関数と `app` のメソッドは同じ動作です。

## 起動時のHome

Deckを指定せずに起動すると、Player／Magicともに `%LOCALAPPDATA%\ScriptDeck\home.deck` を開きます。

ファイルがなければ、実行ファイル内のRCDATAリソース `assets/home.deck` から初期Homeを保存して開きます。初期Homeには「Open Deck」と「新規Deck」のボタンがあります。新規Deckは未保存のDeckを作成してMagicへ切り替えます。Homeを編集・保存した後は、その内容を使います。存在するHomeが壊れている場合はエラーを表示し、自動上書きしません。

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

## Deckファイルの読み込み・保存

グローバル関数と `app` メソッドの両方で利用できます。

| API | 動作・戻り値 |
| --- | --- |
| `openDeck(path)` / `app.openDeck(path)` | 指定Deckを開く。省略時は現在のDeckをディスクから読み直す。未保存の変更は確認せず破棄。戻り値はundefined。 |
| `saveDeck(path)` / `app.saveDeck(path)` | 指定パスに現在のDeckを保存。省略時は現在のパスへ上書き。指定パスへの保存成功後はそのパスを現在のDeckパスにする。戻り値はundefined。 |
| `saveAsDeck()` / `app.saveAsDeck()` | Deck用の保存ダイアログを表示して保存。成功でtrue、キャンセルでfalse。成功後は選択したパスを現在のDeckパスにする。 |

```javascript
saveDeck();                 // 現在のファイルを上書き
saveDeck("backup.deck");    // 別パスに保存して現在のパスを変更
openDeck();                 // 未保存の変更を捨てて読み直す
openDeck("other.deck");     // 保存せずに別Deckを開く
if (saveAsDeck()) alert(app.getDeckPath());
```

パスは文字列で、相対パスは起動時のカレントフォルダ基準です。空文字列・nullなどは不正な引数として例外になります。引数省略時に現在のパスがない場合、openDeck／saveDeckは例外になります。新規の未保存DeckはsaveAsDeck、またはパス付きsaveDeckで保存してください。

openDeckは指定ファイルを先に検証し、失敗時は現在のDeckを維持します。読み込み成功後の切り替えはイベント処理終了後に行い、スクリプト環境を作り直します。そのため呼び出したイベントの残りのコードは移動前のDeckを参照します。移動後のopenCardでは新しいDeckになります。

保存は同期処理で、保存成功時に未保存マークを解除します。保存失敗時は現在のパスと未保存状態を維持し、例外になります。saveAsDeckのキャンセルでもパス・状態は変えません。Deck切り替えが保留中の場合は保存を拒否します。読み込み・保存先の親フォルダは自動作成しません。ダイアログは既存ファイルへの上書き確認を表示します。

### 起動引数のDeckが存在しない場合

- Player：エラーを表示し、終了コード1で終了します。
- `-magic`：指定パスに初期Deckを作成・保存して編集画面を開きます。作成できなければエラー終了します。
- ファイルが存在するがJSONが壊れている場合：両モードともエラー終了します。上書きしません。
- Deck引数を省略した場合：従来どおりAppDataのHomeを開き、なければ内蔵Homeを保存して開きます。


## 新規Deck

`app.newDeck()` / `newDeck()` は現在のDeckを保存し、新しい未保存Deckを作ってMagicへ切り替えます。切り替えはイベント処理後です。現在のDeckが未保存かつ保存先がない場合は例外になるため、先にsaveDeck(path)／saveAsDeck()で保存してください。保存に失敗した場合も現在のDeckを維持します。新規Deckは「保存」で保存先を指定します。


Magicの「Homeを開く」はMagicのまま移動します。未保存なら破棄確認を表示するUI操作で、既定で保存するスクリプトのgoHome()とは保存の扱いが異なります。内蔵Homeの更新で、既存のAppData内のhome.deckを自動上書きすることはありません。
