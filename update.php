// PHP code to update sensors' data in the table/database.
<?php
  require 'database.php';

  // Condition to check that POST value is not empty.
  if (!empty($_POST)) {
    // Keep track POST values
    $id = $_POST['id'];
    $count = $_POST['count'];
    $time = $_POST['time'];
    $temp = $_POST['temp'];
    $accX = $_POST['accX'];
    $accY = $_POST['accY'];
    $accZ = $_POST['accZ'];
    $gyroX = $_POST['gyroX'];
    $gyroY = $_POST['gyroY'];
    $gyroZ = $_POST['gyroZ'];
    $tempC = $_POST['tempC'];
    $output = $_POST['output'];
    $button = $_POST['button'];

    // Updating the data in the table.
    $pdo = Database::connect();
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $sql = "UPDATE esp32_sensors SET count = ?, time = ?, temp = ?, accX = ?, accY = ?, accZ = ?, gyroX = ?, gyroY = ?, gyroZ = ?, tempC = ?, output = ?, button = ? WHERE id = ?";
    $q = $pdo->prepare($sql);
    $q->execute(array($count,$time,$temp,$accX,$accY,$accZ,$gyroX,$gyroY,$gyroZ,$tempC,$output,$button,$id));
    Database::disconnect();
  }
?>
