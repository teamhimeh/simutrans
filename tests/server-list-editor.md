# 接続先リスト編集の手動確認

基準: teamhimeh/simutrans OTRP-KUTAv6、e2bdac8（OTRP v62.2.0）。

旧方式の last-server.txt の保存・復元は含みません。既存ファイルからの移行も行いません。

- 接続画面で localhost を入力し、追加ボタンを押す。使用中のpakの config/servers.tab に `localhost = localhost` が追加され、一覧に表示される。
- 同じアドレスを再度追加しても登録件数が増えない。空欄や `=` を含む入力も保存されない。
- 保存済みの項目を選択し、削除ボタンを押す。ファイルと一覧の双方から該当項目だけが削除される。
- 公開サーバー一覧だけに存在する項目を選択した場合、削除ボタンが無効になる。
- 接続先の照会失敗時でも、保存済み項目は削除できる。
- servers.tab のコメント、日本語の登録名、空行、他の登録内容が編集後も維持される。
- servers.tab がない環境で追加すると作成される。config が書き込み不可の場合はエラーが表示され、既存ファイルが維持される。
- 再起動して保存した一覧が復元され、localhostへの照会・接続ができる。
- pakを切り替えると、そのpakのservers.tabだけが一覧へ反映される。
- サーバー一覧の取得中に追加・削除しても、取得完了後に保存済み項目が重複しない。

編集ロジックの単体テスト（MSYS2 MINGW64）:

```sh
g++ -std=c++11 tests/serverlist-edit-test.cc -o build/serverlist-edit-test.exe
./build/serverlist-edit-test.exe
```
