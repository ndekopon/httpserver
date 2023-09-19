# httpserver

Windows用の用途の限られたHTTPサーバー。
静的ファイルの配布用途。

## Feature

- HTTP/1.1に対応
- GET/HEADのみ
- リクエストの末端がスラッシュで終わる > index.htmlの取得
- HTTPS非対応

## Usage

`httpserver.exe` と同じフォルダに `htdocs` を作成し、配布したいコンテンツを配置する。

`httpserver.exe` を起動する。

### Customize

`httpserver.exe` と同じフォルダに `httpserver.ini` を作成し、設定を変更する。
待ち受けIPアドレス、ポート番号、許容する同時最大接続数がカスタマイズ可能。

```ini
[MAIN]
IP=127.0.0.1
PORT=20082
CONNECTIONS=64
```

## TODO

- サーバー側からのkeepalive切断対応(現時点はクライアントからの接続断を待つ)

