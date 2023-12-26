// PHP file to get state stored in the table/database.
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
      $myObj->count = $row['count'];
      $myObj->time = $row['time'];
      $myObj->temp = $row['temp'];
      $myObj->accX = $row['accX'];
      $myObj->accY = $row['accY'];
      $myObj->accZ = $row['accZ'];
      $myObj->gyroX = $row['gyroX'];
      $myObj->gyroY = $row['gyroY'];
      $myObj->gyroZ = $row['gyroZ'];
      $myObj->tempC = $row['tempC'];
      $myObj->output = $row['output'];
      $myObj->button = $row['button'];
      
      $myJSON = json_encode($myObj);
      
      echo $myJSON;
    }
    Database::disconnect();
  }
?>
