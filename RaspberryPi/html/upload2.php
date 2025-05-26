<?php
echo "フォームデータ: name=text1, value=" . htmlspecialchars($_POST['text1'] ?? '') . "<br>";
echo "フォームデータ: name=text2, value=" . htmlspecialchars($_POST['text2'] ?? '') . "<br>";
echo "フォームデータ: name=text3, value=" . htmlspecialchars($_POST['text3'] ?? '') . "<br>";
echo "フォームデータ: name=text4, value=" . htmlspecialchars($_POST['text4'] ?? '') . "<br>";
?>
