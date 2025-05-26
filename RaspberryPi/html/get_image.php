<?php
$fileid = $_POST['fileid'];
$filename = sprintf("%03d.jpg", $fileid);
if (file_exists($filename)) {
    header("Content-Type: image/jpeg");
    readfile($filename);
} else {
    http_response_code(404);
    echo "File not found.";
}
?>
