# ScriptDeck ドキュメント

更新日：2026-10-12

[プロジェクト概要・ビルド](../README.md)

## 使い方とファイル形式

| 文書 | 内容 |
| --- | --- |
| [操作ガイド](USER_GUIDE.md) | 起動、画面・コード編集、オブジェクト複製、終了時保存、Home、アイコンと関連付け。 |
| [Deck形式・Folder・deckxとExport](FILE_FORMATS.md) | 形式の比較、外部JS、名前変更、Export、画像・データ同梱、相対パス、deckxの保存と制限。 |

通常Deckを始める場合は操作ガイド、VSCodeを使う制作やdeckx配布はファイル形式の文書から読んでください。

VSCode拡張のインストール、8つのコマンド、ScriptDeck.exeの登録と未設定時の選択については[VSCode拡張の説明](../vscode-scriptdeck/README.md)を参照してください。

## JavaScript API

| 文書 | 内容 |
| --- | --- |
| [JavaScript・イベント](SCRIPT_API.md) | 実行タイミングとスコープ、openCard・mouseUp・change・dropFiles。 |
| [カード・オブジェクトAPI](OBJECT_API.md) | 検索、プロパティ、リスト・チェック状態、動的作成・複製・削除・重なり順。 |
| [カード・Deck移動API](NAVIGATION_API.md) | カード移動、Deck切り替え、openDeck・saveDeck・saveAsDeck、Homeと新規Deck。 |
| [アプリケーションAPI](APP_API.md) | 引数、標準入出力、音声、モード・ウィンドウ制御、パス、関連付け、クリップボード、外部プロセス。 |
| [ファイル・パスAPI](FILE_API.md) | UTF-8・バイナリーの読み書き、ファイル操作、ダイアログ、パス変換、サイズと日時。 |

形式ごとのAPI制約とパス基準は[Deck形式・Export](FILE_FORMATS.md)にもまとめています。

## 開発・ライセンス

- [変更履歴](HISTORY.md)：現在の更新・検証と、過去の実装記録。
- [ルートREADME](../README.md)：概要、ビルド、起動例、サンプル。
- [LICENSE](../LICENSE)：ScriptDeck本体のMITライセンス。
- [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md)：第三者コード・データの著作権表示とライセンス。
