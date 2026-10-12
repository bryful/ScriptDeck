# ScriptDeck 開発ガイド

更新日：2026-10-12

利用方法とVSCode拡張のインストールは[ルートREADME](../README.md)を参照してください。

## ビルドとテスト

GUIはWindows x64向けです。Dear ImGui、Direct3D 11、QuickJS-NGを使用します。
依存ライブラリのソースは同梱しています。日本語表示にはWindowsのMeiryoを使用します。

1. Visual Studio 2026に「C++によるデスクトップ開発」とWindows SDKをインストールします。
2. `ScriptDeck.slnx`を開きます（ツールセットv145）。
3. Release / x64を選んでリビルドします。
4. `ScriptDeck/bin/x64/Release/ScriptDeck.exe`を実行します。

CMakeでテストする場合：

```sh
cmake -S ScriptDeck -B build -DBUILD_TESTING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure --timeout 30
```

GUIはWindows専用です。非Windows向けにもCLI・モデル・JavaScript・ヘッドレスGUIテストを含みます。
今回のWindowsでの検証内容は[変更履歴](HISTORY.md)を参照してください。

## ソースの構成

- `ScriptDeck/`：本体のソース、Visual Studioプロジェクト、テスト。
- `vscode-scriptdeck/`：VSCode拡張のソース、テスト、パッケージ作成ツール。
- `sampleDeck/`：サンプルDeck。
- `doc/`：操作・仕様・API・開発文書。

## VSCode拡張の開発

Node.jsとPythonを用意し、`vscode-scriptdeck/`で次を実行します。依存パッケージのインストールは不要です。

```sh
npm test
python update-api.py
python package.py
```

`update-api.py`は本体の文書からAPI候補と同梱ドキュメントを更新します。`package.py`は`package.json`のバージョンに対応するVSIXを生成します。

VSCodeの画面操作はExtension Development Hostで確認してください。実装と検証範囲は[変更履歴](HISTORY.md)、拡張の操作は[拡張README](../vscode-scriptdeck/README.md)を参照してください。
