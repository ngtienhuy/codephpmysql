#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <FirebaseESP32.h>

// Declare multi-threaded libraries
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>
// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// Link ".cpp" files using ".h" files
#include "Data/CountData.h"                   // V
#include "Data/TimeData.h"                    // V
#include "Data/UserData.h"                    // V
#include "Data/WiFiData.h"                    // V
#include "Sensors/AD8232Sensor.h"             // V
#include "Sensors/MPU6050Sensor.h"            // V
#include "Sensors/NTC10KSensor.h"             // V
#include "Modules/ButtonModule.h"             // V
#include "Modules/LedModule.h"                // V
#include "Modules/GPSModule.h"                // V
#include "Modules/SimModule.h"                // V
#include "Modules/MicroModule.h"              // V
#include "Modules/SoundModule.h"              // V
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

// Define HTTP object
WiFiClient client;
HTTPClient http;  //--> Declare object of class HTTPClient.
int httpCode;     //--> Variables for HTTP return code.
int healthStatus = 0;

// Variables for HTTP POST request data.
String postData = ""; // Variables sent for HTTP POST request data.
String payload = "";  // Variable for receiving response from HTTP POST.

// WiFi connect timeout
const uint32_t connectTimeoutMs = 10000;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Task handles
TaskHandle_t Count_handle;
TaskHandle_t Time_handle;
TaskHandle_t AD8232_handle;
TaskHandle_t MPU6050_handle;
TaskHandle_t NTC10K_handle;
TaskHandle_t Button_handle;
TaskHandle_t Led_handle;
TaskHandle_t GPS_handle;
TaskHandle_t Sim_handle;
TaskHandle_t Micro_handle;
TaskHandle_t Sound_handle;
TaskHandle_t SD_handle;
TaskHandle_t firebase_handle;
TaskHandle_t http_handle;

void wifi_setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Add list of wifi networks
  int length = WiFi_LENGTH(0);
  for (int i=0; i<length; ++i) {
    wifiMulti.addAP(&WIFI_SSID(i)[0], &WIFI_PASSWORD(i)[0]);
  }

  // Connect to Wi-Fi using wifiMulti (connects to the SSID with strongest connection)
  Serial.println("Connecting Wifi...");
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    Serial.println("WiFi connected to: " + WiFi.SSID() + "(" + WiFi.RSSI() + ")");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Restart the device");
    ESP.restart();
  }
}

void firebase_setup() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  // config.signer.test_mode = true;

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
  // Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(16384, 16384); // Rx & Tx buffer size in bytes from 512 - 16384

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);
}

void firebase_loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    if (Firebase.RTDB.setString(&fbdo, "Time/real_time", Time_value())) {
      // Serial.println("PASSED");
      // Serial.println("PATH: " + fbdo.dataPath());
      // Serial.println("TYPE: " + fbdo.dataType());

      Firebase.RTDB.setFloat(&fbdo, "Count/count_num", Count_value());
      Firebase.RTDB.setFloat(&fbdo, "AD8232/output_pin", AD8232_value());
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/temp_pin", MPU6050_value(0));
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/accX_pin", MPU6050_value(1));
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/accY_pin", MPU6050_value(2));
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/accZ_pin", MPU6050_value(3));
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/gyroX_pin", MPU6050_value(4));
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/gyroY_pin", MPU6050_value(5));
      Firebase.RTDB.setFloat(&fbdo, "MPU6050/gyroZ_pin", MPU6050_value(6));
      Firebase.RTDB.setFloat(&fbdo, "NTC10K/tempC_pin", NTC10K_value());
      Firebase.RTDB.setFloat(&fbdo, "GPS/latitude_pin", GPS_value(0));
      Firebase.RTDB.setFloat(&fbdo, "GPS/longitude_pin", GPS_value(1));
      Firebase.RTDB.setFloat(&fbdo, "Micro/micro_pin", Micro_value());
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

void getdata_loop(String host) {
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
  http.begin(client, host);   //--> Specify request destination
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");        //--> Specify content-type header
  
  httpCode = http.POST(postData); //--> Send the request
  payload = http.getString();     //--> Get the response payload

  Serial.print("httpCode : ");
  Serial.println(httpCode); //--> Print HTTP return code
  Serial.print("payload : ");
  Serial.println(payload);  //--> Print request response payload
  
  http.end();  //--> Close connection
  Serial.println("---------------");
}

void check_health() {
  // Serial.println();
  // Serial.println("---------------check_health");
  JSONVar myObject = JSON.parse(payload);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    Serial.println("---------------");
    return;
  }

  // if (myObject.hasOwnProperty("health")) {
  //   Serial.print("myObject[\"health\"] = ");
  //   Serial.println(myObject["health"]);
  // }

  if (JSON.stringify(myObject["health"]) == "1") {
    healthStatus = 1;
  }

  // Serial.println("---------------");
}

void postdata_loop(String host) {
  // The process to send the sensors data to the database.
  postData = "id=esp32_data";
  postData += "&time=" + Time_value();
  postData += "&count=" + String(Count_value());
  postData += "&heart=" + String(AD8232_value());
  postData += "&accX=" + String(MPU6050_value(1));
  postData += "&accY=" + String(MPU6050_value(2));
  postData += "&accZ=" + String(MPU6050_value(3));
  postData += "&gyroX=" + String(MPU6050_value(4));
  postData += "&gyroY=" + String(MPU6050_value(5));
  postData += "&gyroZ=" + String(MPU6050_value(6));
  postData += "&tempC=" + String(NTC10K_value());
  postData += "&latitude=" + String(GPS_value(0));
  postData += "&longitude=" + String(GPS_value(1));
  postData += "&micro=" + String(Micro_value());
  payload = "";

  if (healthStatus == 1) {
    postData += "&health=" + String("1");
  }

  // Serial.println();
  // Serial.println("---------------postdata.php");

  http.begin(client, host);   //--> Specify request destination
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");       //--> Specify content-type header
  
  httpCode = http.POST(postData); //--> Send the request
  payload = http.getString();     //--> Get the response payload

  // Serial.print("httpCode : ");
  // Serial.println(httpCode); //--> Print HTTP return code
  // Serial.print("payload : ");
  // Serial.println(payload);  //--> Print request response payload
  
  http.end();  //--> Close connection
  // Serial.println("---------------");
}

void http_loop() {
  // If the connection to the stongest hotstop is lost, it will connect to the next network on the list
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    getdata_loop("http://170.170.41.41/ESP32_MySQL_Database/getdata.php");
    check_health();
    postdata_loop("http://170.170.41.41/ESP32_MySQL_Database/postdata.php");
  } else {
    Serial.println("WiFi disconnected!");
    Serial.println("Reconnecting to WiFi...");
  }
}

void Count_task(void *parameter) {
  while(true) {
    Count_loop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void Time_task(void *parameter) {
  while(true) {
    Time_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void AD8232_task(void *parameter) {
  while(true) {
    AD8232_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void MPU6050_task(void *parameter) {
  while(true) {
    MPU6050_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void NTC10K_task(void *parameter) {
  while(true) {
    NTC10K_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void Button_task(void *parameter) {
  while(true) {
    if (healthStatus == 1) {
      Button_loop();
      if (Button_value() == 1) {
        healthStatus = 0;
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void Led_task(void *parameter) {
  while(true) {
    if (healthStatus == 1) {
      Led_loop();
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void GPS_task(void *parameter) {
  while(true) {
    GPS_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void Sim_task(void *parameter) {
  while(true) {
    Sim_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void Micro_task(void *parameter) {
  while(true) {
    Micro_loop();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void Sound_task(void *parameter) {
  while(true) {
    if (healthStatus == 1) {
      // Sound_loop();
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void SD_task(void *parameter) {
  while(true) {
    SD_loop();
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

void firebase_task(void *parameter) {
  while(true) {
    firebase_loop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void http_task(void *parameter) {
  while(true) {
    http_loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  wifi_setup();
  // Count_setup();
  Time_setup();
  AD8232_setup();
  MPU6050_setup();
  // NTC10K_setup();
  Button_setup();
  Led_setup();
  GPS_setup();
  // Sim_setup();
  Micro_setup();
  Sound_setup();
  SD_setup();
  // firebase_setup();
  xTaskCreate(Count_task, "Count_task", 2000, NULL, 1, &Count_handle);
  xTaskCreate(Time_task, "Time_task", 2000, NULL, 1, &Time_handle);
  xTaskCreate(AD8232_task, "AD8232_task", 1000, NULL, 1, &AD8232_handle);
  xTaskCreate(MPU6050_task, "MPU6050_task", 2000, NULL, 1, &MPU6050_handle);
  xTaskCreate(NTC10K_task, "NTC10K_task", 1000, NULL, 1, &NTC10K_handle);
  xTaskCreate(Button_task, "Button_task", 1000, NULL, 1, &Button_handle);
  xTaskCreate(Led_task, "Led_task", 1000, NULL, 1, &Led_handle);
  xTaskCreate(GPS_task, "GPS_task", 1000, NULL, 1, &GPS_handle);
  // xTaskCreate(Sim_task, "Sim_task", 1000, NULL, 1, &Sim_handle);
  xTaskCreate(Micro_task, "Micro_task", 1000, NULL, 1, &Micro_handle);
  xTaskCreate(Sound_task, "Sound_task", 1000, NULL, 1, &Sound_handle);
  xTaskCreate(SD_task, "SD_task", 3000, NULL, 0, &SD_handle);
  // xTaskCreate(firebase_task, "firebase_task", 3000, NULL, 0, &firebase_handle);
  xTaskCreate(http_task, "http_task", 3000, NULL, 0, &http_handle);
}

void loop() {
  // Count_loop();
  // Time_loop();
  // AD8232_loop();
  // MPU6050_loop();
  // NTC10K_loop();
  // Button_loop();
  // Led_loop();
  // GPS_setup();
  // Sim_setup();
  // Micro_setup();
  // Sound_setup();
  // SD_setup();
  // firebase_setup();
  // http_loop();
}
