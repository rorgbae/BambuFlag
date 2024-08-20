#include <Arduino.h>
// BambuFlag
// Based on the code of t0nyz0/Bambu-Poop-Conveyor-ESP32
//
char version[10] = "0.0.1";

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ESP32Servo.h>

//---- SETTINGS YOU SHOULD ENTER --------------------------------------------------------------------------------------------------------------------------

// WiFi credentials
char ssid[40] = "your-wifi-ssid";
char password[40] = "your-wifi-password";

// MQTT credentials
char mqtt_server[40] = "your-bambu-printer-ip";
char mqtt_password[30] = "your-bambu-printer-accesscode";
char serial_number[20] = "your-bambu-printer-serial-number";

// --------------------------------------------------------------------------------------------------------------------------------------------------------

char mqtt_port[6] = "8883";
char mqtt_user[30] = "bblp";
char mqtt_topic[200];

//Servo properties
int pinServo = 27;
int minServo = 500;
int maxServo = 2000;
int hzServo = 40;
int posServo = 90;
bool enableCallback = true;

// Debug flag
bool debug = true;
bool autoPushAllEnabled = true;
bool pushAllCommandSent = false;
bool wifiConnected = false;
bool mqttConnected = false;

unsigned long lastAttemptTime = 0;
const unsigned long RECONNECT_INTERVAL = 15000;  // 15 seconds

unsigned int sequence_id = 20000;
unsigned long previousMillis = 0;

// MQTT state variables
int printer_stage = -1;
int printer_sub_stage = -1;
String printer_real_stage = "";
String gcodeState = "";

// Create instances
WiFiClientSecure espClient;
PubSubClient client(espClient);
Preferences preferences;
WebServer server(80);
Servo myServo;

int status0[] = {5, 6, 16, 17, 20, 21, 26, 27, 28, 30,  32, 33, 35 }; //ERROR
int status1[] = {0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19, 22, 23, 24, 25, 31, }; //PRINTING

// Function to get the printer stage description
const char* getStageInfo(int stage) {
  switch (stage) {
    case -1: return "Idle";
    case 0: return "Printing";
    case 1: return "Auto Bed Leveling";
    case 2: return "Heatbed Preheating";
    case 3: return "Sweeping XY Mech Mode";
    case 4: return "Changing Filament";
    case 5: return "M400 Pause";
    case 6: return "Paused due to filament runout";
    case 7: return "Heating Hotend";
    case 8: return "Calibrating Extrusion";
    case 9: return "Scanning Bed Surface";
    case 10: return "Inspecting First Layer";
    case 11: return "Identifying Build Plate Type";
    case 12: return "Calibrating Micro Lidar";
    case 13: return "Homing Toolhead";
    case 14: return "Cleaning Nozzle Tip";
    case 15: return "Checking Extruder Temperature";
    case 16: return "Printing was paused by the user";
    case 17: return "Pause of front cover falling";
    case 18: return "Calibrating Micro Lidar";
    case 19: return "Calibrating Extrusion Flow";
    case 20: return "Paused due to nozzle temperature malfunction";
    case 21: return "Paused due to heat bed temperature malfunction";
    case 22: return "Filament unloading";
    case 23: return "Skip step pause";
    case 24: return "Filament loading";
    case 25: return "Motor noise calibration";
    case 26: return "Paused due to AMS lost";
    case 27: return "Paused due to low speed of the heat break fan";
    case 28: return "Paused due to chamber temperature control error";
    case 29: return "Cooling chamber";
    case 30: return "Paused by the Gcode inserted by user";
    case 31: return "Motor noise showoff";
    case 32: return "Nozzle filament covered detected pause";
    case 33: return "Cutter error pause";
    case 34: return "First layer error pause";
    case 35: return "Nozzle clog pause";
    default: return "Unknown stage";
  }
}

void handleServoMin() {
  if (server.method() == HTTP_POST) {
    minServo = server.arg("minServo").toInt();
    preferences.putInt("minServo", minServo);

    myServo.detach();
    myServo.attach(pinServo, minServo, maxServo);
    myServo.write(posServo);
    server.send(200, "text/plain", "Motor activated manually");
  }
}

void handleServoMax() {
  if (server.method() == HTTP_POST) {
    maxServo = server.arg("maxServo").toInt();
    preferences.putInt("maxServo", maxServo);
    myServo.detach();
    myServo.attach(pinServo, minServo, maxServo);
    myServo.write(posServo);
    server.send(200, "text/plain", "Motor activated manually");
  }
}

void handleServoHz() {
  if (server.method() == HTTP_POST) {
    hzServo = server.arg("hzServo").toInt();
    preferences.putInt("hzServo", hzServo);
    myServo.write(hzServo);
    myServo.write(posServo);
    server.send(200, "text/plain", "Motor activated manually");
  }
}

void handleServoPos() {
  if (server.method() == HTTP_POST) {
    posServo = server.arg("posServo").toInt();
    myServo.write(posServo);
    server.send(200, "text/plain", "Motor activated manually");
  }
}

void handleEnableCallback() {
  if (server.method() == HTTP_POST) {
    enableCallback = server.arg("enableCallback") == "true";
    server.send(200, "text/plain", "Changed enabled callback");
  }
}

// Function to handle the config page
void handleConfig() {
  if (server.method() == HTTP_GET) {
    String html = "<!DOCTYPE html><html><head><title>BambuFlag</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #d1cdba; color: #333; text-align: center; margin: 0; padding: 0; }";
    html += ".container { max-width: 600px; margin: 5px auto; background: #fff; padding: 20px; border-radius: 5px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }";
    html += ".logo img { width: 100px; height: auto; margin-bottom: 0px; }";
    html += "h2 { font-size: 24px; margin-bottom: 20px; }";
    html += ".info { text-align: left; }";
    html += ".info label { display: block; margin: 10px 0 5px; font-weight: bold; }";
    html += ".info input { width: calc(100% - 20px); padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 5px; }";
    html += "form { margin-top: 20px; }";
    html += "input[type=submit] { padding: 10px 20px; border: none; border-radius: 5px; background-color: #28a745; color: white; cursor: pointer; }";
    html += "input[type=submit]:hover { background-color: #218838; }";
    html += ".links { margin-top: 20px; }";
    html += ".links a { display: inline-block; margin: 0 10px; padding: 10px 20px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; }";
    html += ".links a:hover { background-color: #0056b3; }";
    html += "</style>";
    html += "</head><body>";
    html += "<div class=\"container\">";
    html += "<h2>BambuFlag v" + String(version) + "</h2>";
    html += "<form action=\"/config\" method=\"POST\" class=\"info\">";
    html += "<label for=\"ssid\">Wifi SSID:</label><input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"" + String(ssid) + "\"><br>";
    html += "<label for=\"password\">Wifi Password:</label><input type=\"text\" id=\"password\" name=\"password\" value=\"" + String(password) + "\"><br>";
    html += "<label for=\"mqtt_server\">Bambu Printer IP Address:</label><input type=\"text\" id=\"mqtt_server\" name=\"mqtt_server\" value=\"" + String(mqtt_server) + "\"><br>";
    html += "<label for=\"mqtt_password\">Bambu Printer Access Code:</label><input type=\"text\" id=\"mqtt_password\" name=\"mqtt_password\" value=\"" + String(mqtt_password) + "\"><br>";
    html += "<label for=\"serial_number\">Bambu Printer Serial Number:</label><input type=\"text\" id=\"serial_number\" name=\"serial_number\" value=\"" + String(serial_number) + "\"><br>";
    html += "<label for=\"debug\">Serial Monitor Debug:</label><input type=\"checkbox\" id=\"debug\" name=\"debug\" " + String(debug ? "checked" : "") + "><br>";
    html += "<label for=\"minServo\">MinServo:</label><input type=\"number\" id=\"minServo\" name=\"minServo\" value=\"" + String(minServo) + "\"><br>";
    html += "<label for=\"maxServo\">MaxServo:</label><input type=\"number\" id=\"maxServo\" name=\"maxServo\" value=\"" + String(maxServo) + "\"><br>";
    html += "<label for=\"hzServo\">HzServo:</label><input type=\"number\" id=\"hzServo\" name=\"hzServo\" value=\"" + String(hzServo) + "\"><br>";
    html += "<label for=\"posServo\">PosServo:</label><input type=\"number\" id=\"posServo\" name=\"posServo\" value=\"" + String(posServo) + "\"><br>";
    
    html += "<input type=\"button\" value=\"Status0 position\" onclick=\"fetch('/servopos?posServo=180', { method: 'POST'});\">";
    html += "<input type=\"button\" value=\"Status1 position\" onclick=\"fetch('/servopos?posServo=90', { method: 'POST'});\">";
    html += "<input type=\"button\" value=\"Status2 position\" onclick=\"fetch('/servopos?posServo=0', { method: 'POST'});\">";
    html += "<label for=\"debug\">Enable callback process:</label><input type=\"checkbox\" value=\"0\" onclick=\"fetch('/enablecallback?enableCallback=' + this.checked, { method: 'POST'});\" " + String(enableCallback ? "checked" : "") + ">";
    html += "<input type=\"submit\" value=\"Save\">";
    html += "</form>";

    html += "</div></body>";
    html += "<script type='text/javascript'>";
    html += "var maxElement = document.getElementById('maxServo'); maxElement.addEventListener('change', changeMax);  function changeMax() {fetch('/servomax?maxServo='+maxElement.value, { method: 'POST'});}";
    html += "var minElement = document.getElementById('minServo'); minElement.addEventListener('change', changeMin);  function changeMin() {fetch('/servomin?minServo='+minElement.value, { method: 'POST'});}";
    html += "var hzElement = document.getElementById('hzServo'); hzElement.addEventListener('change', changeHz);  function changeHz() {fetch('/servohz?hzServo='+hzElement.value, { method: 'POST'});}";
    html += "var posElement = document.getElementById('posServo'); posElement.addEventListener('change', changePos);  function changePos() {fetch('/servopos?posServo='+posElement.value, { method: 'POST'});}";
    html += "</script>";
    html += "</html>";

    server.send(200, "text/html", html);
  } else if (server.method() == HTTP_POST) {
    strcpy(ssid, server.arg("ssid").c_str());
    strcpy(password, server.arg("password").c_str());
    strcpy(mqtt_server, server.arg("mqtt_server").c_str());
    strcpy(mqtt_password, server.arg("mqtt_password").c_str());
    strcpy(serial_number, server.arg("serial_number").c_str());
    debug = server.hasArg("debug");

    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putString("mqtt_server", mqtt_server);
    preferences.putString("mqtt_password", mqtt_password);
    preferences.putString("serial_number", serial_number);
    preferences.putBool("debug", debug);

    server.send(200, "text/html", "<h1>Settings saved. Device rebooting.</h1><br><a href=\"/config\">Click here to return to config page</a>");

    delay(1000);
    ESP.restart();
  }
}


String formatDateTime(time_t timestamp) {
  struct tm* timeinfo = localtime(&timestamp);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

  return String(buffer);
}

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  DynamicJsonDocument doc(20000);
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    if (debug) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    }
    return;
  }

  if (doc.containsKey("print") && doc["print"].containsKey("stg_cur")) {
    printer_stage = doc["print"]["stg_cur"].as<int>();
  }

  if (doc.containsKey("print") && doc["print"].containsKey("mc_print_sub_stage")) {
    printer_sub_stage = doc["print"]["mc_print_sub_stage"].as<int>();
  }

  int status = 2; // 0 = Error, 1 = Printing, 2 = Idle

  if(enableCallback){
    for(int i = 0; i < sizeof(status0)/sizeof(int); i++){
      if(status0[i] == printer_stage){
        status = 0;
      }
    }

    for(int i = 0; i < sizeof(status1)/sizeof(int); i++){
      if(status1[i] == printer_stage){
        status = 1;
      }
    }

    switch(status){
      case 0:
        posServo = 180;
        myServo.write(posServo);
        break;
        
      case 1:
        posServo = 0;
        myServo.write(posServo);
        break;

      default:
        posServo = 90;
        myServo.write(posServo);
        break;
    }
  }

  if (debug) {
    Serial.println("--------------------------------------------------------");
    Serial.print("BambuFlag v");
    Serial.print(version);
    Serial.print(" | Wifi: ");
    Serial.println(WiFi.localIP());
    Serial.print("Current Print Stage: ");
    Serial.print(getStageInfo(printer_stage));
    Serial.print(" | Sub stage: ");
    Serial.println(getStageInfo(printer_sub_stage));
  }
}

// Function to connect to MQTT
void connectToMqtt() {
  if (!client.connected()) {
    if (debug) Serial.print("Connecting to MQTT...");
    if (client.connect("BambuConveyor", mqtt_user, mqtt_password)) {
      if (debug) Serial.println("Connected to Bambu X1");
      sprintf(mqtt_topic, "device/%s/report", serial_number);
      client.subscribe(mqtt_topic);
      publishPushAllMessage();
    } else {
      if (debug) {
        Serial.print("Failed: ");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        if(client.state() == -2){
          client.setServer(mqtt_server, 8883);  // Assuming default MQTT port 8883
          espClient.setInsecure();
          client.setCallback(mqttCallback);
        }
      }
      lastAttemptTime = millis();
    }
  } else {
    
  }
}

// Function to publish a push all message
void publishPushAllMessage() {
  if (client.connected()) {
    char publish_topic[128];
    sprintf(publish_topic, "device/%s/request", serial_number);

    DynamicJsonDocument doc(1024);
    doc["pushing"]["sequence_id"] = sequence_id;
    doc["pushing"]["command"] = "pushall";
    doc["user_id"] = "2222222";

    String jsonMessage;
    serializeJson(doc, jsonMessage);

    if (debug) {
      Serial.println("JSON message to send: " + jsonMessage);
    }

    bool success = client.publish(publish_topic, jsonMessage.c_str());

    if (success) {
      if (debug) {
        Serial.print("Message successfully sent to: ");
        Serial.println(publish_topic);
      }
    } else {
      if (debug) Serial.println("Failed to send message.");
    }

    sequence_id++;
  } else {
    if (debug) Serial.println("Not connected to MQTT broker!");
  }
}

// Function to publish a JSON message
void publishJsonMessage(const char* jsonString) {
  if (client.connected()) {
    char publish_topic[128];
    sprintf(publish_topic, "device/%s/request", serial_number);

    if (client.publish(publish_topic, jsonString)) {
      if (debug) {
        Serial.print("Message successfully sent to: ");
        Serial.println(publish_topic);
      }
    } else {
      if (debug) Serial.println("Error sending the message.");
    }

    sequence_id++;
  } else {
    if (debug) Serial.println("Not connected to MQTT broker!");
  }
}

// Function to send a push all command
void sendPushAllCommand() {
  if (client.connected() && !pushAllCommandSent) {
    String mqtt_topic_request = "device/";
    mqtt_topic_request += serial_number;
    mqtt_topic_request += "/request";

    StaticJsonDocument<128> doc;
    doc["pushing"]["sequence_id"] = String(sequence_id++);
    doc["pushing"]["command"] = "pushall";
    doc["user_id"] = "2222222";

    String payload;
    serializeJson(doc, payload);
    if (debug) Serial.println("MQTT Callback sent - Sendpushallcommand");
    client.publish(mqtt_topic_request.c_str(), payload.c_str());
    pushAllCommandSent = true;
  }
}

// Setup function
void setup() {
  // Start the Serial communication
  Serial.begin(115200);
  client.setBufferSize(20000);

  // Configure LED PWM functionalities


  // Start Preferences
  preferences.begin("my-config", false);

  // Load stored preferences, use default values if not set
  preferences.getString("ssid", ssid, sizeof(ssid));
  preferences.getString("password", password, sizeof(password));
  preferences.getString("mqtt_server", mqtt_server, sizeof(mqtt_server));
  preferences.getString("mqtt_password", mqtt_password, sizeof(mqtt_password));
  preferences.getString("serial_number", serial_number, sizeof(serial_number));

  debug = preferences.getBool("debug", debug);

  if (debug) Serial.println(minServo);
  pinServo = preferences.getInt("pinServo", pinServo);
  minServo = preferences.getInt("minServo", minServo);
  maxServo = preferences.getInt("maxServo", maxServo);
  hzServo = preferences.getInt("hzServo", hzServo);
  preferences.getInt("posServo", posServo);

  if (debug) Serial.println(minServo);

  //Configure servo
  myServo.attach(pinServo, minServo, maxServo);
  myServo.setPeriodHertz(hzServo);
  myServo.write(posServo);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  if (debug) Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (debug) Serial.print('.');
  }

  if (debug) {
    Serial.println();
    Serial.print("Connected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());
  }

  client.setServer(mqtt_server, 8883);  // Assuming default MQTT port 8883
  espClient.setInsecure();
  client.setCallback(mqttCallback);
  sprintf(mqtt_topic, "device/%s/report", serial_number);

  // Define the root URL
  server.on("/", handleConfig);

  // Define control page
  server.on("/servomin", handleServoMin);
  server.on("/servomax", handleServoMax);
  server.on("/servohz", handleServoHz);
  server.on("/servopos", handleServoPos);
  server.on("/enablecallback", handleEnableCallback);

  // Define the config URL
  server.on("/config", handleConfig);

  // Start the server
  server.begin();
  if (debug) Serial.println("Web server started.");

  sendPushAllCommand();
}

// Loop function
void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      pushAllCommandSent = false;
    }
  } else {
    wifiConnected = false;
  }

  if (client.connected()) {
    if (!mqttConnected) {
      mqttConnected = true;
      sendPushAllCommand();
      pushAllCommandSent = true;
    }
  } else {
    mqttConnected = false;
  }

  if (!client.connected() && (millis() - lastAttemptTime) > RECONNECT_INTERVAL) {
    connectToMqtt();
  }

  if (autoPushAllEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 30000) {
      previousMillis = currentMillis;
      if (client.connected()) {
        if (debug) Serial.println("Requesting pushAll...");
        publishPushAllMessage();
      }
    }
  }

  client.loop();
}
