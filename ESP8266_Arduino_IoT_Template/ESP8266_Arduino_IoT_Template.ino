// ------------------------------------------------------
// ESP8266 Arduino IoT template V1.0.0
//  Impliments
//  - WiFi
//  - Async Webserver with SPIFFS
//  - Websockets
//  - JSON
//  - mDNS
// ------------------------------------------------------

// WiFi libraries
#include <ESP8266WiFi.h>

// mDNS
#include <ESP8266mDNS.h>

// Async Webserver libraries
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// SPIFFS
#include <FS.h>
#include <SPIFFSEditor.h>

// JSON libraries
#include <AsyncJson.h>
#include <ArduinoJson.h>

// ------------------------------------------------------
// Function definitions
// ------------------------------------------------------

// general functions
String formatBytes(size_t);

// Webserver and Websocket functions
void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *, AwsEventType, void *, uint8_t *, size_t);

// JSON
void sendJSON(void);
uint8_t parseJSON(String);

// standard arduino functions
void setup(void);
void loop(void);
// ------------------------------------------------------

// websocket command bytes
enum wsCMDs {
  SLEEP =       0xFE,   // sleep
  RESET =       0xFF    // reset
};

// ------------------------------------------------------
// Global Variables
// ------------------------------------------------------

//WiFi Station variables
String STAssid = "STAssid";  // station WiFi SSID
String STApass = "STApass";  // station WiFi password

//WiFi AP variables
String APssid = "APssid";  // AP WiFi SSID
String APpass = "APpass";  // AP WiFi password
IPAddress APlocal_IP(192, 168, 8, 1);
IPAddress APgateway(192, 168, 8, 1);
IPAddress APsubnet(255, 255, 255, 0);

//Webserver variables
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
//AsyncEventSource events("/events");

// FS edit webpage
const char* http_username = "admin";
const char* http_password = "admin";

// mDNS
String hname = "hostname";

//JSON
char JSONbuf[500];

// serial print buffrt
char printBuf[250];
// ------------------------------------------------------

// ------------------------------------------------------
// SPIFFS utility
// ------------------------------------------------------
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

// ------------------------------------------------------
// Async websocket event handler
// ------------------------------------------------------
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    sprintf(printBuf, "ws[%s][%u] connect\n", server->url(), client->id());
    Serial.println(printBuf);
    client->printf(JSONbuf);
  } else if(type == WS_EVT_DISCONNECT){
    sprintf(printBuf, "ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    Serial.println(printBuf);
  } else if(type == WS_EVT_ERROR){
    sprintf(printBuf, "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    Serial.println(printBuf);
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      sprintf(printBuf, "Received message: %s\n\r",msg.c_str());
      Serial.println(printBuf);

      if (msg.startsWith("")) {
        sprintf(printBuf, "Client requested...\n\r");
        Serial.println(printBuf);
        client->printf(JSONbuf);
      } else {
        uint8_t updated = parseJSON(msg);
        if (!updated) {
          sprintf(printBuf, "Invalid message\n\r");
          Serial.println(printBuf);
        }
      }
    }
  }
}

// ------------------------------------------------------
// sends test JSON
// ------------------------------------------------------
void sendJSON(void) {
  StaticJsonBuffer<500> infoBuffer;
  JsonObject &infoRoot = infoBuffer.createObject();
  
  infoRoot["type"] = "test";
  JsonArray& clockTimeArray = infoRoot.createNestedArray("arrayVals");
  clockTimeArray.add("val1");
  clockTimeArray.add("val2");
  clockTimeArray.add("val3");
  clockTimeArray.add("val4");
  infoRoot["anotherTest"] = 100;

  infoRoot.printTo(JSONbuf);
}

// ------------------------------------------------------
// updates settings based on values from websocket message
// ------------------------------------------------------
uint8_t parseJSON(String msg) {
  uint8_t updated = 1;
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(msg);
  if (!root.success()) {
    sprintf(printBuf, "Failed to parse JSON\n\r");
    Serial.println(printBuf);
    updated = 0;
  } else {
    uint8_t cmd = root["cmd"];
    switch(cmd) {
      case SLEEP:
        {
          sprintf(printBuf, "Sleeping\n\r");
          Serial.println(printBuf);
        }
        break;
      case RESET:
        {
          sprintf(printBuf, "Restarting\n\r");
          Serial.println(printBuf);
          ESP.restart();
        }
        break;
    }
  }
  return updated;
}
// ------------------------------------------------------

// ------------------------------------------------------
// standard Arduino setup function
// ------------------------------------------------------
void setup() {
  delay(1000); // allow time to open serial console

  // initialize serial
  Serial.begin(38400);
  sprintf(printBuf, "Serial initialized\n\r");
  Serial.println(printBuf);

  // get flash info
  // ------------------------------------------------------
  sprintf(printBuf, "Flash chip size: %u\n\r",ESP.getFlashChipRealSize());
  Serial.println(printBuf);
  sprintf(printBuf, "Scketch size: %u\n\r",ESP.getSketchSize());
  Serial.println(printBuf);
  sprintf(printBuf, "Free flash space: %u\n\r",ESP.getFreeSketchSpace());
  Serial.println(printBuf);
  // ------------------------------------------------------

  // SPIFFS setup
  // ------------------------------------------------------
  SPIFFS.begin();
  sprintf(printBuf, "SPIFFS initialized\n\r");
  Serial.println(printBuf);
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {    
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    sprintf(printBuf, "FS File: %s, size: %s\n\r",fileName.c_str(), formatBytes(fileSize).c_str());
    Serial.println(printBuf);
  }
  // ------------------------------------------------------

  // initialize WiFi
  // ------------------------------------------------------
  WiFi.hostname(hname.c_str());
  
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(APlocal_IP, APgateway, APsubnet);

  WiFi.softAP(APssid.c_str(), APpass.c_str(), 1, 1);
  
  WiFi.begin(STAssid.c_str(), STApass.c_str());
  
  sprintf(printBuf, "WiFi initialized\n\r");
  Serial.println(printBuf);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");

  IPAddress ip = WiFi.localIP();
  sprintf(printBuf, "WiFi connected. IP address: %d.%d.%d.%d\n\r",ip[0],ip[1],ip[2],ip[3]);
  Serial.println(printBuf);
  // ------------------------------------------------------

  // start webserver & websocket
  // ------------------------------------------------------
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // mDNS responder
  if (!MDNS.begin(hname.c_str())) {
    sprintf(printBuf, "WiFi initialized\n\r");
    Serial.println(printBuf);
  }
  sprintf(printBuf, "mDNS initialized. Hostname: %s\n\r",hname.c_str());
  Serial.println(printBuf);
  
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);

  sprintf(printBuf, "mDNS services: HTTP, TCP, port 80; WS, TCP, port 81\n\r");
  Serial.println(printBuf);

//  events.onConnect([](AsyncEventSourceClient *client){
//    client->send("hello!",NULL,millis(),1000);
//  });
//  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(http_username,http_password));

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // temporary
  server.serveStatic("/wstest", SPIFFS, "/").setDefaultFile("wstest.htm");
  
  server.begin();
  ip = WiFi.softAPIP();
  sprintf(printBuf, "Webserver started. AP IP address: %d.%d.%d.%d\n\r",ip[0],ip[1],ip[2],ip[3]);
  Serial.println(printBuf);
  
  // send update to all clients
  sendJSON();
  ws.textAll(JSONbuf);
  // ------------------------------------------------------
}

// ------------------------------------------------------
// standard Arduino main loop
// ------------------------------------------------------
void loop() {
	delay(100);
}
