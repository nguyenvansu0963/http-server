<?php
echo "Content-Type: text/html\n\n"; // CGI用

echo "<h1>フォーム受信結果</h1>";

echo "<h2>GETデータ:</h2>";
foreach ($_GET as $key => $value) {
    echo "name=$key, value=$value<br>";
}

echo "<h2>POSTデータ:</h2>";
foreach ($_POST as $key => $value) {
    echo "name=$key, value=$value<br>";
}
?>
