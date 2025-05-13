#define DEBUG             0

#define TIMEOUT           100
#define DEBOUNCE_TIMEOUT  1000 // milliseconds
#define WD_TIMEOUT        30       // seconds  
// Define custom I2C pins
#define I2C_SDA   14  // Example: GPIO21
#define I2C_SCL   32  // Example: GPIO22

#define LED_PIN     14
#define NUM_PIXELS  21
#define RST_SWITCH  4
#define TRIG_SWITCH 13

#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

#include <Wire.h>
#include <Arduino.h>
#include <OSCMessage.h>
#include <ETH.h>
#include <WiFiUdp.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <vector>
#include "eth_properties.h"
#include "esp_task_wdt.h"

BluetoothSerial SerialBT; // Bluetooth Serial
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);  // Choose your IRQ and RESET pins
Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800); // NeoPixel strip on GPIO 13
Preferences preferences;  // Preferences for storing data
WiFiUDP Udp;

IPAddress ip, subnet, gateway, outIp;
uint16_t inPort = 7001;
uint16_t outPort = 7000;
uint32_t lastMillis = 0;  
uint32_t resetMillis = 0; // Last time WDT was reset
uint32_t colors[][3] = {
            {255,0,0},   // Red
            {0,255,0},   // Green
            {255,255,0},   // Blue
            {255,255,0}, // Yellow
            {255,0,255}, // Magenta
            {0,255,255}  // Cyan
            };

bool success      = false;
bool cardPresesnt = false;


uint8_t numTags       = 0;                            // Number of tags
String removeCommand  = "";                           // Remove command
std::vector<String> tags;
std::vector<String> commands;
std::vector<uint8_t> OSCMessageMode;
String tagID          = "";       // Current tag ID
String prevTagID      = "";       // Previous tag ID
String RemoveOSCMessageString = ""; // OSC message for tag removal

const String HELP = "NFC PN532 - Firmware v1.0\n N<num> - Set number of tags. 'Eg: N10'\nT<index> - Set Last placed tag ID for index. Eg: T01\nC<index><command> - Set command for index. Eg: C01HELLO - Set HELLO command for index 1\nR<command> - Set Tag Remove command. Eg: RREMOVED - Set REMOVED command for tag remove. \n HELP - Show this help message\n\n";


void showColor(uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) { strip.setPixelColor(i, color);  }
  strip.show();
}

void showColorFromArray(int index) {
  if (index >= 0 && index < sizeof(colors) / sizeof(colors[0])) {
  uint32_t color = strip.Color(colors[index][0], colors[index][1], colors[index][2]);
  showColor(color);
  } else {
  if (DEBUG) { Serial.println("Invalid color index.");}
  }
}

void saveIPAddress(const char* keyPrefix, IPAddress address) {
  for (int i = 0; i < 4; i++) {
    String key = String(keyPrefix) + i;
    preferences.putUInt(key.c_str(), address[i]);
  }
}

IPAddress loadIPAddress(const char* keyPrefix, IPAddress defaultIP) {
  IPAddress result;
  for (int i = 0; i < 4; i++) {
    String key = String(keyPrefix) + i;
    result[i] = preferences.getUInt(key.c_str(), defaultIP[i]);
  }
  return result;
}

void saveNetworkConfig() {
  preferences.begin("RFID", false);
  saveIPAddress("ip", ip);
  saveIPAddress("sub", subnet);
  saveIPAddress("gw", gateway);
  saveIPAddress("out", outIp);
  preferences.putUInt("inPort", inPort); // Save input port
  preferences.putUInt("outPort", outPort); // Save output port
  for (int i = 0; i < numTags; i++) { if (i < OSCMessageMode.size()) { preferences.putUInt(("mode" + String(i)).c_str(), OSCMessageMode[i]);}} // Save mode for each tag
  preferences.end();
}

void loadNetworkConfig() {
  preferences.begin("RFID", true);
  ip      = loadIPAddress("ip",  IPAddress(10, 255, 250, 150));
  subnet  = loadIPAddress("sub", IPAddress(255, 255, 254, 0));
  gateway = loadIPAddress("gw",  IPAddress(10, 255, 250, 1));
  outIp   = loadIPAddress("out", IPAddress(10, 255, 250, 129));
  inPort  = preferences.getUInt("inPort", 7001); // Load input port
  outPort = preferences.getUInt("outPort", 7000); // Load output port
  for (int i = 0; i < numTags; i++) { if (i < OSCMessageMode.size()) { OSCMessageMode[i] = preferences.getUInt(("mode" + String(i)).c_str(), 0); } } // Load mode for each tag
  preferences.end();
}

void oscSend(uint8_t column, int value) {
  char address[40];  // increase buffer size
  snprintf(address, sizeof(address), "/composition/columns/%d/connect", column);
  if (DEBUG){ Serial.println(address); } // Debug: print the address to Serial
  OSCMessage msg(address);
  msg.add(value);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void timeCodeOSCSend(uint8_t mode){
  char address[20];
  snprintf(address, sizeof(address), "/mode%d", mode);
  OSCMessage msg(address);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void processTagID(String tagID) {
  for (int i = 0; i < numTags; i++) {
    if (i < tags.size()) {
      if (tagID == tags[i]) {
        timeCodeOSCSend(OSCMessageMode[i]);
        oscSend(i+1, 1);
        Serial.println("TAG ID: " + tagID + " - " + commands[i]);
        SerialBT.println("TAG ID: " + tagID + " - " + commands[i]);
        showColorFromArray(1); 
        return;
      }
    }
  }
}

void readNFC(){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  tagID = "";
  // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UUID (normally 7)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, TIMEOUT);
  // NO CHANGE IN CARD
  if (success && cardPresesnt){ return; }
  // WAITING FOR NEW CARD
  if (!success && !cardPresesnt) { return; }
  // IF NEW TAG COUND
  if (success & (!cardPresesnt)) {
    cardPresesnt = true;
    // Store UID to tagID
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) { tagID += "0"; }
      tagID += String(uid[i], HEX);
    }
    tagID.toUpperCase();
    prevTagID = tagID;
    if (DEBUG) {
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      Serial.println("TAG ID: "+ tagID); Serial.print("  UID Value: "); nfc.PrintHex(uid, uidLength);
      Serial.println("");
    }
    processTagID(tagID);
    return;
  }
  // IF CARD REMOVED
  if (!success && cardPresesnt) {
    cardPresesnt = false;
    if(DEBUG) {Serial.println("CARD REMOVED");}
    for (int i = 0; i < numTags; i++) {
      if (i < tags.size()) {
        if (prevTagID == tags[i]) {
          Serial.println(); Serial.println(removeCommand);
          showColorFromArray(2);
          return;
        }
      }
    }
  }
}

void saveConfig(){
  preferences.begin("RFID", false); // Open preferences with read/write access
  preferences.putUInt("numTags", numTags); // Save number of tags
  preferences.putString("removeCommand", removeCommand); // Save remove command
  for (int i = 0; i < numTags; i++) {
    if (i < tags.size()) {
      preferences.putString(("tag" + String(i)).c_str(), tags[i]); // Save tag ID
      preferences.putString(("command" + String(i)).c_str(), commands[i]); // Save command for tag ID
    }
  }
  preferences.end(); // Close preferences
}

void getConfig(){
  preferences.begin("RFID", true); // Open preferences with read-only access
  numTags = preferences.getUInt("numTags", 2); // Get number of tags
  removeCommand = preferences.getString("removeCommand", ""); // Get remove command
  for (int i = 0; i < numTags; i++) {
    if (i < tags.size()) {
      tags[i] = preferences.getString(("tag" + String(i)).c_str(), ""); // Get tag ID
      commands[i] = preferences.getString(("command" + String(i)).c_str(), ""); // Get command for tag ID
    }
  }
  preferences.end(); // Close preferences
  if (DEBUG) {
    Serial.println("Number of tags: " + String(numTags));
    Serial.println("Remove command: " + removeCommand);
    for (int i = 0; i < numTags; i++) {
      if (i < tags.size()) {
        Serial.println("Tag ID " + String(i) + ": " + tags[i]);
        Serial.println("Command for tag ID " + String(i) + ": " + commands[i]);
      }
    }
  }
  SerialBT.println("Number of tags: " + String(numTags));
  SerialBT.println("Remove command: " + removeCommand);
  for (int i = 0; i < numTags; i++) {
    if (i < tags.size()) {
      SerialBT.println("Tag ID " + String(i) + ": " + tags[i]);
      SerialBT.println("Command for tag ID " + String(i) + ": " + commands[i]);
    }
  }

  SerialBT.printf("Input port: %d\n", inPort);
  SerialBT.printf("Output port: %d\n", outPort);
  SerialBT.printf("IP: %s\n", ip.toString().c_str());
  SerialBT.printf("Subnet: %s\n", subnet.toString().c_str());
  SerialBT.printf("Gateway: %s\n", gateway.toString().c_str());
  SerialBT.printf("OutIP: %s\n", outIp.toString().c_str());
}

void processData(String data) {
  data.trim(); // Remove leading and trailing whitespace
  auto updateIP = [&](const String& prefix, IPAddress& target, int offset) {
    String value = data.substring(offset);
    if (target.fromString(value)) {
      saveNetworkConfig();
      SerialBT.printf("✅ %s updated and saved.\n", prefix.c_str());
    } else {
      SerialBT.printf("❌ Invalid %s format.\n", prefix.c_str());
    }
  };
  if (data.startsWith("SET_IP ")) { updateIP("IP", ip, 7); } 
  else if (data.startsWith("SET_SUBNET ")) { updateIP("Subnet", subnet, 11); } 
  else if (data.startsWith("SET_GATEWAY ")) { updateIP("Gateway", gateway, 12);  } 
  else if (data.startsWith("SET_OUTIP ")) { updateIP("OutIP", outIp, 10); } 
  else if (data.startsWith ("SET_INPORT ")) {
    int port = data.substring(10).toInt();
    if (port > 0 && port < 65536) { inPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ Input port set to %d and saved.\n", inPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }
  else if (data.startsWith("SET_OUTPORT ")) {
    int port = data.substring(12).toInt();
    if (port > 0 && port < 65536) { outPort = static_cast<uint16_t>(port); saveNetworkConfig(); SerialBT.printf("✅ Output port set to %d and saved.\n", outPort); } 
    else { SerialBT.println("❌ Invalid port. Must be between 1 and 65535."); }
  }
  else if (data.startsWith("SET_MODE ")) {
    int index = data.substring(9, 11).toInt() - 1;
    if (index >= 0 && index < numTags) {
      int mode = data.substring(11).toInt();
      if (mode >= 0 && mode <= numTags) {
        OSCMessageMode[index] = static_cast<uint8_t>(mode);
        saveNetworkConfig();
        SerialBT.printf("✅ Mode for tag %d set to %d and saved.\n", index + 1, mode);
      } else {
        SerialBT.println("❌ Invalid mode. Must be 0 or 1.");
      }
    } else {
      SerialBT.println("❌ Invalid tag index. Must be between 1 and " + String(numTags) + ".");
    }
  }
  
  else if (data == "IP") { SerialBT.printf("ETH IP: %s\n", ETH.localIP().toString().c_str());}
  else if (data == "MAC") { SerialBT.printf("ETH MAC: %s\n", ETH.macAddress().c_str());}
  if (data.startsWith("N")) {
    numTags = data.substring(1, data.length()).toInt();
    if (numTags > 20) numTags = 10;                   // Ensure numTags does not exceed array bounds
    tags.resize(numTags, "");
    commands.resize(numTags, "");
    OSCMessageMode.resize(numTags, 0);
    saveConfig();
    SerialBT.println("Number of tags set to: " + String(numTags));
    Serial.println("Number of tags set to: " + String(numTags));
    return;
  } else if (data.startsWith("T")) {
    int index = data.substring(1, data.length()).toInt() - 1;
    if (index >= 0 && index < numTags) {
      tags[index] = prevTagID;
    }
    saveConfig();
    SerialBT.println("Tag ID set for index " + String(index) + ": " + tags[index]);
    Serial.println("Tag ID set for index " + String(index) + ": " + tags[index]);
    return;
  } else if (data.startsWith("C")) {
    int index = data.substring(1, data.length()).toInt() - 1;
    if (index >= 0 && index < numTags) {
      commands[index] = data.substring(3, data.length());
    }
    saveConfig();
    SerialBT.println("Command set for index " + String(index) + ": " + commands[index]);
    Serial.println("Command set for index " + String(index) + ": " + commands[index]);
    return;
  } else if (data.startsWith("R")) {
    removeCommand = data.substring(1, data.length());
    saveConfig();
    SerialBT.println("Remove command set to: " + removeCommand);
    Serial.println("Remove command set to: " + removeCommand);
    return;
  } else if (data.indexOf("HELP")>=0){
    SerialBT.println(HELP);
    Serial.println(HELP);
    return;
  } else if (data.indexOf("GET")>=0){
    getConfig();
    return;
  }
}

void readBTSerial(){
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    processData(incoming);
    if (DEBUG) {SerialBT.println(incoming);}
  }
}

void readSwitches() {
  if (millis() - lastMillis < DEBOUNCE_TIMEOUT ){ return; } // Debounce delay
  if (digitalRead(RST_SWITCH) == HIGH) {
    oscSend(numTags + 1, 1);
    lastMillis = millis(); // Update lastMillis to current time
    if (DEBUG) { Serial.println("RST_SWITCH pressed"); }
  } if (digitalRead(TRIG_SWITCH) == HIGH) {
    oscSend(numTags + 2, 1);
    lastMillis = millis(); // Update lastMillis to current time
    if (DEBUG) { Serial.println("TRIG_SWITCH pressed"); }
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH IP: ");
      Serial.println(ETH.localIP());
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      ESP.restart(); // Restart ESP32
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      break;
    default:
      break;
  }
}

void ethInit() {
  ETH.begin( ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE_0);
  ETH.config(ip, gateway, subnet);
  WiFi.onEvent(WiFiEvent);
  Udp.begin(inPort);
  delay(5000); // Wait for the Ethernet to initialize
  Serial.println("ETH Initialized");
  Serial.printf("ETH IP: %s\n", ETH.localIP().toString().c_str());
  Serial.printf("ETH MAC: %s\n", ETH.macAddress().c_str());
}

void nfcInit(){
  Wire.begin(I2C_SDA, I2C_SCL);  delay(100); // Initialize I2C with custom pins
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not detected. Retrying...");
    delay(5000);  // Retry after 5 seconds
    ESP.restart(); // Or go back to loop
  }
  
  if (DEBUG) {
    // Got ok data, print it out!
    Serial.print("Found chip PN532"); Serial.println((versiondata>>24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

    Serial.println("Waiting for an ISO14443A Card ...");
  } 
}

void loadConfig(){
  preferences.begin("RFID", true); // 
  numTags = preferences.getUInt("numTags", 2); // Get number of tags
  tags.resize(numTags, "");
  commands.resize(numTags, "");
  OSCMessageMode.resize(numTags, 0);
  removeCommand = preferences.getString("removeCommand", ""); // Get remove command
  for (int i = 0; i < numTags; i++) {
    if (i < tags.size()) {
      tags[i] = preferences.getString(("tag" + String(i)).c_str(), ""); // Get tag ID
      commands[i] = preferences.getString(("command" + String(i)).c_str(), ""); // Get command for tag ID
    }
  }
  preferences.end(); // Close preferences
}


void stripInit() {
  strip.begin(); // Initialize the NeoPixel strip
  strip.show(); // Initialize all pixels to 'off'
  strip.setBrightness(255); // Set brightness to 50 (0-255)
  showColorFromArray(2);
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Mini Holotube");
  pinMode(RST_SWITCH, INPUT_PULLUP);
  pinMode(TRIG_SWITCH, INPUT_PULLUP);
  stripInit();
  // Initialize WDT (8 seconds timeout)
  esp_task_wdt_init(WD_TIMEOUT, true); // timeout in seconds, panic = true
  esp_task_wdt_add(NULL);     // Add current thread to WDT
  loadConfig();
  loadNetworkConfig();
  nfcInit();
  ethInit();
}

void loop() {
  esp_task_wdt_reset(); // Feed the watchdog
  readNFC();
  readBTSerial();
  readSwitches();
}