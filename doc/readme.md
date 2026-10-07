# ScriptDeck ドキュメント

ScriptDeckのAPI仕様、使用例、実装履歴をまとめています。

## API・機能の説明

| ファイル | 内容 |
| --- | --- |
| [BUILTINS.md](BUILTINS.md) | 組み込み関数の一覧。起動引数、JSON、標準入出力、ファイル操作、音声、パス文字列、ウィンドウ制御、コード実行など。 |
| [OBJECT_API.md](OBJECT_API.md) | カード・部品の検索、プロパティの読み書き、リスト項目と選択状態の操作、部品の動的作成・複製・削除など。 |
| [NAVIGATION_API.md](NAVIGATION_API.md) | カード移動、Deckの切り替え、Homeへの移動、自動保存の指定、起動時のHome作成について。 |
| [DAY2_API.md](DAY2_API.md) | Checkbox／RadioButtonの状態とイベント、ファイル選択・保存ダイアログ、複数ファイルのドロップについて。 |
| [VERSION1.md](VERSION1.md) | Playerの1行スクリプト入力、実行ファイル・Deckのアイコン、`.deck`ファイルの関連付けについて。 |
| [SCRIPT_API.md](SCRIPT_API.md) | JavaScriptとホストアプリの接続仕様。モード切り替え、コンソール制御などの実装契約について。 |

## 実装履歴・変更記録

| ファイル | 内容 |
| --- | --- |
| [OBJECT_CHANGES.md](OBJECT_CHANGES.md) | カード・部品APIを追加した際の変更ファイルと検証内容。 |
| [DEVELOPMENT_HISTORY.md](DEVELOPMENT_HISTORY.md) | 組み込み関数の実装時の説明、サンプルの使用方法、検証内容を保存した開発記録。 |

実装履歴には、変更当時のファイル配置やソリューション名が記載されています。現在のビルド手順とフォルダ構成は、[ルートのREADME.md](../README.md)を参照してください。

## ライセンス

- [LICENSE](../LICENSE)：ScriptDeck本体のMITライセンス。
- [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)：同梱する第三者コード・データの著作権表示とライセンス。
