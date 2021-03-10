<?php
$uploadfile = '';
// echo 'Uploading ';
// echo $_FILES['imageFile']['name'];
if ( strlen( basename( $_FILES['imageFile']['name'] ) ) > 0 ) {
    $uploadfile = basename( $_FILES['imageFile']['name'] );
    if ( move_uploaded_file( $_FILES['imageFile']['tmp_name'], $uploadfile ) ) {
        @chmod( $uploadfile, 0777 );
    } else echo ' Error copying!';
}
?>
