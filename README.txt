ーーーーーーーーーーーーーー
70310046 鈴木貴登
ーーーーーーーーーーーーーー
2025/05/25 16:12段階での統合プログラムです

[対応している機能]
・GETリクエスト(http＆benchmark)
・POSTリクエスト(http＆benchmark)
・CGI'(php)対応
・Basic認証
・各種ステータスコード (301,302,303,401,403,418)
・HTML5動画の再生

※このコードの実行においてポート番号が10046になっているので
　適宜変更してください

ーーーーーーーーーーーーーー
benchmarkフォルダについて
　こちらはUbuntu環境に入れて動かしてください
　動かし方はGETの時と変わりません。

　src-get：get通信用ベンチマークプログラム
　src-post：post通信用ベンチマークプログラム

ーーーーーーーーーーーーーー
RaspberryPiフォルダについて
　こちらはRaspberryPi環境に入れて動かしてください
　PHPの動作にはphp-cgiが必要です。以下のコマンドで入れておいてください
　
　sudo apt install php php-cgi

ーーーーーーーーーーーーーー
動作確認方法について
[HTML5動画読み込み]
192.168.1.101:XXXXX/video.html　にアクセス
	→　動画が再生出来たら成功

[各種ステータスコード＆Basic認証]
・200　Ubuntu環境で　curl -i -u admin3:12345 http://192.168.1.101:XXXXX/secret.html　と入力
				→　200 OKと表示されたらOK
・301　Ubuntu環境で　curl -i  http://192.168.1.101:XXXXX/old.html　と入力
				→　301 Moved Permanentlyと表示されたらOK
・302　Ubuntu環境で　curl -i  http://192.168.1.101:XXXXX/temp.html　と入力
				→　302 Foundと表示されたらOK
・303　Ubuntu環境で　curl -i  http://192.168.1.101:XXXXX/postdone　と入力
				→　303 See Otherと表示されたらOK
・401　Ubuntu環境で　curl -i -u admin:12345 http://192.168.1.101:XXXXX/secret.html　と入力
				→　401 Unauthorizedと表示されたらOK
・403　Ubuntu環境で　curl -i -u user:12345 http://192.168.1.101:XXXXX/secret.html　と入力
				→　403 Forbiddenと表示されたらOK
・418　Ubuntu環境で　curl -i  http://192.168.1.101:XXXXX/teapot　と入力
				→　418 I'm a teapotと表示されたらOK

[POSTリクエスト]
Ubuntuのsrc-postからベンチマークプログラムを動かす。
	→　Response Code:200で正しくファイルサイズが返ってこればOK

[PHP処理]
192.168.1.101:XXXXX/test_form.html　にアクセス
	→　「GET送信」ボタンを押して、入力したvalueが正しく反映されていればGET通信におけるPHP処理OK
	→　「POST送信」ボタンを押して、入力したvalueが正しく反映されていればPOST通信におけるPHP処理OK

その他…
192.168.1.101:XXXXX/sample.html　にアクセス
	→　入力した画像ファイル名が正しく表示されればOK
