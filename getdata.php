<?php
  include 'database.php';

  // Condition to check that POST value is not empty.
  if (!empty($_POST)) {
    // Keep track post values
    $id = $_POST['id'];
    
    $myObj = (object)array();
    
    $pdo = Database::connect();
    $sql = 'SELECT * FROM esp32_sensors WHERE id="' . $id . '"';
    foreach ($pdo->query($sql) as $row) {
      $myObj->id = $row['id'];
      $myObj->time = $row['time'];
      $myObj->health = $row['health'];
       
      $myJSON = json_encode($myObj);
      
      echo $myJSON;
    }
    Database::disconnect();
  }
?>
