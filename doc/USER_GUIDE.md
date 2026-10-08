# Player・Magicの使い方

更新日：2026-10-08

通常起動はPlayer、`-magic` を付けた起動は編集用のMagicです。Deck未指定時はAppDataのHomeを開きます。指定Deckが存在しない場合、Playerはエラー終了、Magicはそのパスに初期Deckを作成します。

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
app.setMagic(true);
```

成功時のメッセージは表示しません。スクリプトエラーは既存のエラーダイアログに表示します。入力コードはDeckに保存しません。

## アイコンと関連付け

実行ファイル用とDeck用の2種類のアイコンを `assets/scriptdeck.ico` / `assets/deck.ico` として同梱し、実行ファイルのICONリソースに組み込みます。各ICOは16／20／24／32／40／48／64／128／256pxを含みます。編集用SVGと確認用PNGも同梱しています。

```javascript
app.install();   // .deckの関連付けをユーザー単位で登録
app.uninstall(); // 自分の登録を解除
```

Ctrl + Spaceの入力欄から実行できます。

`app.install()` は実行中のEXEの場所を使って、以下を登録します。

- `.deck` のファイル種類 `ScriptDeck.Deck`。
- Deckのアイコン（EXE内の2番目のアイコン）。
- ファイルを開くコマンド `"EXEの絶対パス" "%1"`。空白・日本語を含むパスに対応。
- 「プログラムから開く」と既定のアプリ選択に使う候補情報。

登録先は `HKEY_CURRENT_USER` で、管理者権限は不要です。登録前のユーザー単位の拡張子設定を記録します。再登録時には最初の設定を保持し、EXEの場所を更新します。EXEを移動した場合は、新しい場所から `app.install()` を実行してください。

`app.uninstall()` は登録元のEXEと同じ場所から呼ばれた場合にのみ解除します。拡張子がまだScriptDeckを指していれば登録前の設定に戻します。他アプリに変更されていた場合はその設定を維持します。EXE、Deck、Home、ウィンドウ位置設定などのファイルは削除しません。

成功は `true` を返します。未登録、または別の場所のScriptDeckが登録元の場合、`uninstall` は `false` を返します。登録エラーは例外になります。成功時のダイアログは表示しません。

Windowsで既に別アプリを既定として選んでいる場合、登録だけで切り替わらないことがあります。その場合は `.deck` を右クリックし、「プログラムから開く」でScriptDeckを選んで既定に設定してください。WindowsのUserChoice設定を直接変更する処理は入れていません。

## ビルドと配布

ビルド手順は [ルートREADME.md](../README.md) を参照してください。バイナリー配布時はルートの `LICENSE` と `THIRD_PARTY_NOTICES.md` を同梱してください。

## MagicからPlayerへ

Magicのツールバーの「Playerモードへ」で通常のPlayerに切り替えます。「実行プレビュー」とは別の機能です。編集中のDeckと未保存の変更を保持します。PlayerからMagicへ戻す場合はCtrl + Spaceで `app.setMagic(true);` を実行できます。


## 終了時の未保存変更

Magicでは、従来どおり未保存の変更があると破棄確認ダイアログを表示します。

Playerでは未保存の変更を現在のDeckファイルへ自動保存して終了します。フィールド入力、選択状態、Checkbox／RadioButton、スクリプトからのプロパティ変更・部品作成・削除、カード移動も変更として記録します。変更がない場合は書き込みません。ウィンドウを閉じた場合とapp.exit()の両方に適用します。

保存に失敗した場合はエラーを表示し、終了せず未保存の内容を保持します。保存先がまだないDeckは保存ダイアログを表示します。キャンセルした場合は終了しません。Magicでの未保存内容をPlayerへ切り替えた場合も、終了時に自動保存します。

openDeck()やchangeDeck(path,false)で明示的に破棄して移動した内容は自動保存されません。保存のAPIについては [NAVIGATION_API.md](NAVIGATION_API.md) を参照してください。


## Magicのスクリプト編集

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
| Field / InputBox / Text / Image | 専用イベントがないことを示すコメント。 |

既存Deckの読み込みでコードを書き換えることはありません。既存の空スクリプトは、編集ボタンを押した時に同じ雛形を挿入します。部品複製は元のコードを保持し、createObjectでscriptを指定した場合はその指定を優先します。
