<?php
echo "フォームデータ: name=text1, value=" . htmlspecialchars($_GET['text1'] ?? '') . "<br>";
echo "フォームデータ: name=text2, value=" . htmlspecialchars($_GET['text2'] ?? '') . "<br>";
?>
