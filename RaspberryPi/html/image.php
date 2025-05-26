<?php
// POSTで送信された画像名を取得
$image_name = $_POST['image_name'] ?? '';

// サーバ内の画像ディレクトリ（例：images）を設定
$image_dir = '/home/pi/sample/day6/html/';

// ファイルパスを作成
$image_path = $image_dir . $image_name;

// 画像が存在するか確認
if (file_exists($image_path)) {
    // 画像が存在すれば、その画像を表示
    header('Content-Type: image/jpeg');  // 画像のMIMEタイプを指定（ここではJPEGを例としています）
    readfile($image_path);  // 画像を出力
    exit;
} else {
    // 画像が存在しない場合
    echo "画像が見つかりません。指定されたファイル名を確認してください。";
}
?>
