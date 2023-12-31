<?php
  require 'database.php';

  // Condition to check that POST value is not empty.
  if (!empty($_POST)) {
    // Keep track POST values
    $id = $_POST['id'];
    $time = $_POST['time'];
    $count = $_POST['count'];
    $output = $_POST['output'];
    $temp = $_POST['temp'];
    $accX = $_POST['accX'];
    $accY = $_POST['accY'];
    $accZ = $_POST['accZ'];
    $gyroX = $_POST['gyroX'];
    $gyroY = $_POST['gyroY'];
    $gyroZ = $_POST['gyroZ'];
    $tempC = $_POST['tempC'];
    $latitude = $_POST['latitude'];
    $longitude = $_POST['longitude'];

    // Updating the data in the table.
    $pdo = Database::connect();
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $sql = "UPDATE esp32_sensors SET time = ?, count = ?, output = ?, temp = ?, accX = ?, accY = ?, accZ = ?, gyroX = ?, gyroY = ?, gyroZ = ?, tempC = ?, latitude = ?, longitude = ? WHERE id = ?";
    $q = $pdo->prepare($sql);
    $q->execute(array($time,$count,$output,$temp,$accX,$accY,$accZ,$gyroX,$gyroY,$gyroZ,$tempC,$latitude,$longitude,$id));
    Database::disconnect();
  }
?>
