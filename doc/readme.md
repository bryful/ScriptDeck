# ScriptDeck ドキュメント

更新日：2026-10-08

ScriptDeckの使い方とJavaScript APIを、機能別にまとめています。

## 使い方

| ファイル | 内容 |
| --- | --- |
| [USER_GUIDE.md](USER_GUIDE.md) | Player／Magicの操作、スクリプト編集画面と雛形、Ctrl+Spaceの1行実行、終了時の自動保存・未保存確認、Homeからの新規Deck作成、アイコンとDeckの関連付け。 |

## JavaScript API

| ファイル | 内容 |
| --- | --- |
| [SCRIPT_API.md](SCRIPT_API.md) | Deck・カードスクリプトの実行タイミングとスコープ、`openCard`・`mouseUp`・`change`・`dropFiles`、Listbox／DropdownListの選択変更、文字列コードの実行。 |
| [OBJECT_API.md](OBJECT_API.md) | カード・部品の検索、各プロパティの読み書き、リストの項目・選択状態、Checkbox／RadioButtonの状態・グループ、部品の動的作成・複製・削除・重なり順。 |
| [NAVIGATION_API.md](NAVIGATION_API.md) | カード移動、Deck切り替えと保存指定、Home、新規Deck作成、`openDeck`・`saveDeck`・`saveAsDeck`、起動時に指定Deckが存在しない場合の動作。 |
| [APP_API.md](APP_API.md) | 起動引数、JSON・ダイアログ表示、標準入出力、音声、Player／Magic・コンソール・ウィンドウ制御、Home・Deck・EXE・各フォルダのパス、クリップボード、環境変数、外部プロセスの非同期起動・終了待ちと標準出力取得。 |
| [FILE_API.md](FILE_API.md) | UTF-8のBOM付き／なし、テキスト・バイナリーファイルの読み書き、ファイル・フォルダの列挙と操作、ファイル選択・保存ダイアログ、パス文字列とWindows／Unix形式の変換、ファイルサイズ・作成／更新／アクセス日時。 |

## 開発記録

| ファイル | 内容 |
| --- | --- |
| [HISTORY.md](HISTORY.md) | 過去の実装・変更・検証の記録。変更当時のファイル構成や仕様を含みます。現在の使い方・仕様は上記のカテゴリ別文書を参照してください。 |

## ビルド・ライセンス

- [ルートREADME.md](../README.md)：概要、動作環境、ビルド・起動方法、サンプル。
- [LICENSE](../LICENSE)：ScriptDeck本体のMITライセンス。
- [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)：同梱する第三者コード・データの著作権表示とライセンス。
