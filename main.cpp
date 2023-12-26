// Library declaration
#include <iostream>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <FirebaseESP32.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>
// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// Link ".cpp" files using ".h" files
#include "Data/TimeData.h"                    // V
#include "Data/WiFiData.h"                    // V
#include "Data/UserData.h"                    // V
#include "Sensors/MPU6050Sensor.h"            // V
#include "Sensors/NTC10KSensor.h"             // V
#include "Sensors/AD8232Sensor.h"             // V
#include "Modules/ButtonModule.h"             // -
#include "Modules/MicroModule.h"              //
#include "Modules/SoundModule.h"              //
#include "Modules/SimModule.h"                // -
#include "Modules/GPSModule.h"                // -
#include "Modules/SDModule.h"                 // V

// Insert Firebase API Key
#define API_KEY "AIzaSyCPff5XoeO_JxHauUKCxsBJHBWTv-H8Ge4"
// Insert Firebase RTDB URL
#define DATABASE_URL "https://healthywarning-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "huMCxFkHwkXiaHyjlbQmtP0xXKyh1bzaEk1mPAqL"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseJson json;
FirebaseAuth auth;
FirebaseConfig config;

// Define WiFi object
WiFiMulti wifiMulti;

// Define HTTP
WiFiClient client;
HTTPClient http;  //--> Declare object of class HTTPClient.
int httpCode;     //--> Variables for HTTP return code.

// Variables for HTTP POST request data.
String postData = ""; // Variables sent for HTTP POST request data.
String payload = "";  // Variable for receiving response from HTTP POST.

// WiFi connect timeout
unsigned long sendDataPrevMillis = 0;
const uint32_t connectTimeoutMs = 10000;
bool signupOK = false;

void wifi_setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Add list of wifi networks
  int length = WiFi_Length(0);
  for (int i=0; i<length; ++i) {
    wifiMulti.addAP(&WiFi_SSID(i)[0], &WiFi_PASSWORD(i)[0]);
  }

  // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
  Serial.println("Connecting Wifi...");
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    Serial.print("WiFi connected to: ");
    Serial.print(WiFi.SSID());
    Serial.print("(");
    Serial.print(WiFi.RSSI());
    Serial.println(")");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("WiFi connected");
  } else {
    Serial.println("Restart the device");
    ESP.restart();
  }
}

void wifi_loop() {
  // If the connection to the stongest hotstop is lost, it will connect to the next network on the list
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    // Serial.print("WiFi connected to: ");
    // Serial.print(WiFi.SSID());
    // Serial.print(" (");
    // Serial.print(WiFi.RSSI());
    // Serial.println(")");
    // Serial.print("IP Address: ");
    // Serial.println(WiFi.localIP());
    // Serial.println("WiFi connected");
  } else {
    Serial.println("WiFi disconnected!");
    Serial.println("Reconnecting to WiFi...");
  }
}

void firebase_setup() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  // config.signer.test_mode = true;           // Test mode without Internet

  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
    Serial.println("Sign-up success");
  } else {
    signupOK = false;
    Serial.println(config.signer.signupError.message.c_str());
  }

  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; // See addons/TokenHelper.h

  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(16384 /* Rx buffer size in bytes from 512 - 16384 */, 16384 /* Tx buffer size in bytes from 512 - 16384 */);

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);
}

void firebase_loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    Firebase.RTDB.setString(&fbdo, "Count/number_count", Time_result(0));
    Firebase.RTDB.setString(&fbdo, "Time/realtime", Time_result(1));

    Firebase.RTDB.setFloat(&fbdo, "MPU6050/temp_pin", MPU6050_result(0));
    Firebase.RTDB.setFloat(&fbdo, "MPU6050/accX_pin", MPU6050_result(1));
    Firebase.RTDB.setFloat(&fbdo, "MPU6050/accY_pin", MPU6050_result(2));
    Firebase.RTDB.setFloat(&fbdo, "MPU6050/accZ_pin", MPU6050_result(3));
    Firebase.RTDB.setFloat(&fbdo, "MPU6050/gyroX_pin", MPU6050_result(4));
    Firebase.RTDB.setFloat(&fbdo, "MPU6050/gyroY_pin", MPU6050_result(5));
    Firebase.RTDB.setFloat(&fbdo, "MPU6050/gyroZ_pin", MPU6050_result(6));

    Firebase.RTDB.setFloat(&fbdo, "NTC10K/tempC_pin", NTC10K_result());

    Firebase.RTDB.setFloat(&fbdo, "AD8232/Output_pin", AD8232_result());

    // if (Firebase.RTDB.setFloat(&fbdo, "Count", count++)) {
    //   Serial.println("PASSED");
    //   Serial.println("PATH: " + fbdo.dataPath());
    //   Serial.println("TYPE: " + fbdo.dataType());
    // } else {
    //   Serial.println("FAILED");
    //   Serial.println("REASON: " + fbdo.errorReason());
    // }

  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

void http_setup() {
  for(uint8_t t = 4; t > 0; t--) {
    Serial.print("[SETUP] WAIT ... ");
    Serial.println(t);
    Serial.flush();
  }
}

void http_database_loop() {
  Serial.println("[HTTP] BEGIN...");
  
  // Process to check server.
  Serial.println();
  Serial.println("---------------database.php");

  http.begin(client, "http://170.170.41.7/ESP32_MySQL_Database/database.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  httpCode = http.GET();

  if(httpCode > 0) {
    Serial.print("[HTTP] GET... code: ");
    Serial.println(httpCode);

    if(httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.print("[HTTP] GET... failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }

  http.end();
  Serial.println("---------------");
}

void http_getdata_loop() {
  // Process to get data from database to control devices.
  postData = "id=esp32_data";
  payload = "";

  Serial.println();
  Serial.println("---------------getdata.php");

  // In this project I use local server or localhost with XAMPP application.
  // So make sure all PHP files are "placed" or "saved" or "run" in the "htdocs" folder.
  // I suggest that you create a new folder for this project in the "htdocs" folder.
  // The "htdocs" folder is in the "xampp" installation folder.
  // The order of the folders I recommend:
  // xampp\htdocs\your_project_folder_name\phpfile.php
  //
  // ESP32 accesses the data bases at this line of code: 
  // http.begin("http://REPLACE_WITH_YOUR_COMPUTER_IP_ADDRESS/REPLACE_WITH_PROJECT_FOLDER_NAME_IN_htdocs_FOLDER/getdata.php");
  // REPLACE_WITH_YOUR_COMPUTER_IP_ADDRESS = there are many ways to see the IP address, you can google it. 
  //                                         But make sure that the IP address used is "IPv4 address".
  // Example : http.begin("http://192.168.0.0/ESP32_MySQL_Database/Test/getdata.php");
  http.begin(client, "http://170.170.41.7/ESP32_MySQL_Database/getdata.php");  //--> Specify request destination
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");        //--> Specify content-type header
  
  httpCode = http.POST(postData); //--> Send the request
  payload = http.getString();     //--> Get the response payload

  Serial.print("httpCode : ");
  Serial.println(httpCode); //--> Print HTTP return code
  Serial.print("payload  : ");
  Serial.println(payload);  //--> Print request response payload
  
  http.end();  //--> Close connection
  Serial.println("---------------");
}

void http_update_loop() {
  Serial.println();
  Serial.println("---------------");
  JSONVar myObject = JSON.parse(payload);

  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    Serial.println("---------------");
    // return;
  }

  // The process to send the sensors data to the database.
  postData = "id=esp32_data";
  postData += "&count=" + Time_result(0);
  postData += "&time=" + Time_result(1);
  postData += "&temp=" + String(MPU6050_result(0));
  postData += "&accX=" + String(MPU6050_result(1));
  postData += "&accY=" + String(MPU6050_result(2));
  postData += "&accZ=" + String(MPU6050_result(3));
  postData += "&gyroX=" + String(MPU6050_result(4));
  postData += "&gyroY=" + String(MPU6050_result(5));
  postData += "&gyroZ=" + String(MPU6050_result(6));
  postData += "&tempC=" + String(NTC10K_result());
  postData += "&output=" + String(AD8232_result());
  postData += "&button=" + String(Button_result());
  payload = "";

  Serial.println();
  Serial.println("---------------update.php");
  http.begin(client, "http://170.170.41.7/ESP32_MySQL_Database/update.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  httpCode = http.POST(postData);
  payload = http.getString();

  Serial.print("httpCode : ");
  Serial.println(httpCode);
  Serial.print("payload  : ");
  Serial.println(payload);
  
  http.end();
  Serial.println("---------------");
}

void http_loop() {
  // Check WiFi connection status.
  if (wifiMulti.run() == WL_CONNECTED) {
    http_database_loop();
    http_getdata_loop();
    http_update_loop();
  }
}

void setup() {
  wifi_setup();
    delay(1);
  MPU6050_setup();
    delay(1);
  NTC10K_setup();
    delay(1);
  AD8232_setup();
    delay(1);
  Button_setup();
    delay(1);
  Micro_setup();
    delay(1);
  Sound_setup();
    delay(1);
  Sim_setup();
    delay(1);
  GPS_setup();
    delay(1);
  SD_setup();
    delay(1);
  Time_setup();
    delay(1);
  // firebase_setup();
    delay(1);
  http_setup();
    delay(1);
}

void loop() {
  wifi_loop();
    delay(1);
  MPU6050_loop();
    delay(1);
  NTC10K_loop();
    delay(1);
  AD8232_loop();
    delay(1);
  Button_loop();
    delay(1);
  Micro_loop();
    delay(1);
  Sound_loop();
    delay(1);
  Sim_loop();
    delay(1);
  GPS_loop();
    delay(1);
  SD_loop();
    delay(1);
  Time_loop();
    delay(1);
  // firebase_loop();
    delay(1);
  http_loop();
    delay(1);
}
