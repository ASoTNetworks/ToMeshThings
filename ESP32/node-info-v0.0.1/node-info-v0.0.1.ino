#include "esp_system.h"
#include "WiFi.h"
#include <U8x8lib.h>
#include <iostream>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include <Time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <analogWrite.h> //https://github.com/ERROPiX/ESP32_AnalogWrite

// OLED config
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

// display scroll delay in ms
#define displayScroll 2000

// display contrast 0 to 255
#define displayContrast 0

// display fonts
#define bigfont u8x8_font_inb46_4x8_r
#define stdfont u8x8_font_chroma48medium8_r

// NTP config
// timezone in seconds
#define NTP_OFFSET -18000
// sync NTP every miliseconds
#define NTP_INTERVAL 3600 * 1000
// NTP server
#define NTP_ADDRESS  "10.0.0.1"

// nodeinfo vars
String httpNiHost = "10.0.0.1";
String httpNiPath = "/nodeinfo.json";

// put values into niurl
String niurl=("http://" + httpNiHost + httpNiPath);

// YGG HTTP vars
String httpYHost = "10.0.0.1";
String httpYPath = "/cgi-bin/peers-yggdrasil";

// put values into yurl
String yurl=("http://" + httpYHost + httpYPath);

// cjdns HTTP vars
String httpCHost = "10.0.0.1";
String httpCPath = "/cgi-bin/peers-cjdns-arduino";

// put values into curl
String curl=("http://" + httpCHost + httpCPath);

static volatile bool wifi_connected = false;

// defines
String mac = "";
int prg = 0;
int forceprg = 0;
String WaitForInput(String Question) {
  Serial.println(Question);
  while(!Serial.available()) {
    // wait for input
  }
  return Serial.readStringUntil(10);
}

// NTP function
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

void setup(){
    // init the serial console
    Serial.begin(115200);

    // init the EEPROM
    EEPROM.begin(512);

    // init the OLED
    u8x8.begin();
    u8x8.setFont(stdfont);
    u8x8.setContrast(displayContrast);

    // init GPIOs
    pinMode(25, OUTPUT);
    pinMode(0, INPUT);
    
    // prompt for program mode
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.printf("Hold PRG button");
    u8x8.setCursor(0, 1);
    u8x8.printf("to clear EEPROM");
    u8x8.setCursor(0, 2);
    u8x8.printf("and re-program.");
    delay(2000);

    // get wifi config from eeprom
    // get SSID from eeprom
    String essid = "";
    for (int i = 0; i < 32; ++i){
      essid += char(EEPROM.read(i));
    }
    // get password from eeprom
    String epasswd = "";
    for (int i = 32; i < 96; ++i){
      epasswd += char(EEPROM.read(i));
    }

    // check if values are blank if it is go into programming mode
    if ((essid == "") || (epasswd == "")) {
      forceprg = 1;
    }
    // go into programming mode when PRG button is pressed.
    prg = digitalRead(0);
    if ((prg == LOW) || (forceprg == 1)){
      Serial.println("Entered Programming Mode");
      // strobe LED
      analogWriteFrequency(10);
      analogWrite(LED_BUILTIN, 15);

      // print programming mode on OLED
      u8x8.clear();
      u8x8.setCursor(0, 0);
      u8x8.printf("Programming Mode");
      u8x8.setCursor(0, 1);
      u8x8.printf("Open serial");
      u8x8.setCursor(0, 2);
      u8x8.printf("console and");
      u8x8.setCursor(0, 3);
      u8x8.printf("press ENTER or");
      u8x8.setCursor(0, 4);
      u8x8.printf("Send.");
      u8x8.setCursor(0, 6);
      u8x8.printf("Baud rate");
      u8x8.setCursor(0, 7);
      u8x8.printf("115200");

      // clear EEPROM
      Serial.println("Clearing EEPROM...");
      for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
      EEPROM.commit();
      Serial.println("EEPROM Cleared!");

      // user serial prompt
      // press enter to start
      WaitForInput("Press enter or Send button to start.");

      // ask SSID
      String qssid;
      qssid = WaitForInput("Please enter your SSID");
      Serial.print("SSID set to: ");
      Serial.println(qssid);

      // ask password
      String qpasswd;
      qpasswd = WaitForInput("Please enter your password");
      Serial.println("password stored");

      // write data to EEPROM
      // SSID
      Serial.println("writing SSID to EEPROM:");
      for (int i = 0; i < qssid.length(); ++i){
        EEPROM.write(i, qssid[i]);
      }
      // passwd
      Serial.println("writing passwd to EEPROM:");
      for (int i = 0; i < qpasswd.length(); ++i){
        EEPROM.write(32+i, qpasswd[i]);
      }
      EEPROM.commit();
      Serial.println("EEPROM Programmed. Rebooting.");
      
      // reboot
      delay(500);
      Serial.println("Rebooting");
      analogWrite(LED_BUILTIN, 0);
      esp_restart();
    }

    // init wifi
    Serial.print("WiFi connecting to: ");
    Serial.println(essid.c_str());
    WiFi.disconnect(true);
    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(essid.c_str(), epasswd.c_str());
    mac=WiFi.macAddress();

    // start NTP client
    timeClient.begin();

    // print starting on OLED
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.print("Starting...");
    u8x8.setCursor(0, 2);
    u8x8.print("MAC:");
    u8x8.setCursor(0, 3);
    u8x8.print(mac);
}

void loop(){
    if(wifi_connected){
        // sync NTP
        timeClient.update();
        // start wifi connected loop
        wifiConnectedLoop();
    }
    while(Serial.available()) Serial.write(Serial.read());
}

void wifiOnConnect(){
    Serial.print("WiFi MAC:");
    Serial.println(mac);
    Serial.println("STA Connected");
    Serial.print("STA IPv4: ");
    Serial.println(WiFi.localIP());
    Serial.print("STA IPv6: ");
    Serial.println(WiFi.localIPv6());
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.print("WiFi Connected.");
    u8x8.setCursor(0, 1);
    u8x8.print("IP is:");
    u8x8.setCursor(0, 2);
    u8x8.print(WiFi.localIP());
    delay(1000);
    u8x8.clear();
}

void wifiOnDisconnect(){
    Serial.println("STA Disconnected");
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.print("WiFi");
    u8x8.setCursor(0, 1);
    u8x8.print("Connection Fail");
    delay(5000);
    
    // get wifi config from eeprom
    // get SSID from eeprom
    String essid = "";
    for (int i = 0; i < 32; ++i){
      essid += char(EEPROM.read(i));
    }
    // get password from eeprom
    String epasswd = "";
    for (int i = 32; i < 96; ++i){
      epasswd += char(EEPROM.read(i));
    }
    // defines for wifi client
    Serial.print("WiFi connecting to: ");
    Serial.println(essid.c_str());
    WiFi.begin(essid.c_str(), epasswd.c_str());
}

void WiFiEvent(WiFiEvent_t event){
    switch(event) {
        case SYSTEM_EVENT_STA_CONNECTED:
            //enable sta ipv6 here
            WiFi.enableIpV6();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            wifiOnConnect();
            wifi_connected = true;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifi_connected = false;
            wifiOnDisconnect();
            break;
        default:
            break;
    }
}

void wifiConnectedLoop(){
    // turn LED on to indicate processing date
    analogWrite(LED_BUILTIN, 64);
    unsigned long epoch = timeClient.getEpochTime();
    Serial.print("");
    
    // format time to readable format from epoch
    time_t utcCalc = epoch ;
    unsigned long cyear = year(utcCalc );
    unsigned long cmonth = month(utcCalc );
    unsigned long cday = day(utcCalc );
    unsigned long chour = hour(utcCalc );
    unsigned long cminute = minute(utcCalc );
    unsigned long csecond = second(utcCalc );
    Serial.printf("Current Time: %04lu-%02lu-%02lu %02lu:%02lu:%02lu\n", cyear,cmonth,cday,chour,cminute,csecond);
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.printf("Current Time is");
    u8x8.setCursor(0, 1);
    u8x8.printf("%04lu-%02lu-%02lu\n%02lu:%02lu", cyear,cmonth,cday,chour,cminute);
    // turn LED off to indicate processing date done
    analogWrite(LED_BUILTIN, 0);
    delay(displayScroll);

    // turn LED on to indicate processing nodeinfo data
    analogWrite(LED_BUILTIN, 64);
    // get nodeinfo data from node over HTTP
    // clear screen and print nodeinfo peers;
    
    // object of class HTTPClient
    Serial.println("Starting HTTPClient nihttp");
    HTTPClient nihttp;
    Serial.println("nihttp.begin: " + niurl);
    nihttp.begin(niurl);
    nihttp.setTimeout(1000);
    Serial.println("nihttp.GET");
    int nihttpCode = nihttp.GET();
    int nicount=0;
    int niline=0;
    
    //Check the returning code
    if (nihttpCode > 0) {
      // Get the request response payload
      String nidata = nihttp.getString();
      
      // output RAW data
      Serial.println("RAW DATA");
      Serial.println(nidata);

      // Parsing
      StaticJsonDocument<5000> doc;
      DeserializationError error = deserializeJson(doc, nidata);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        u8x8.clear();
        u8x8.setCursor(0, 1);
        u8x8.printf("deserializeJson\nfailed:\n%s", error.c_str());
        analogWrite(LED_BUILTIN, 0);
        delay(displayScroll);
        return;
      }

      JsonObject obj=doc.as<JsonObject>();
      JsonObject::iterator it=obj.begin(); 
      Serial.print("key: ");
      const char* key = doc["key"];
      Serial.println(key);
      
      // output stuff to the screen
      char *keyshort = strrchr(key, ':');
      Serial.print("node: ");
      Serial.println(keyshort+1);
      
      // set big font
      u8x8.setFont(bigfont);
      u8x8.setCursor(0, 0);
      u8x8.printf(keyshort+1);
      
      // set to standard font
      u8x8.setFont(stdfont);
    }
    //Close connection
    nihttp.end();
    // turn LED off to indicate done processing nodeinfo data
    analogWrite(LED_BUILTIN, 0);
    delay(displayScroll);

    // turn LED on to indicate processing Yggdrasil data
    analogWrite(LED_BUILTIN, 64);
    // get Yggdrasil data from node over HTTP
    // clear screen and print Yggdrasil peers
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.printf("Yggdrasil Peers");
    
    // object of class HTTPClient
    Serial.println("Starting HTTPClient yhttp");
    HTTPClient yhttp;
    Serial.println("yhttp.begin: " + yurl);
    yhttp.begin(yurl);
    yhttp.setTimeout(1000);
    Serial.println("yhttp.GET");
    int yhttpCode = yhttp.GET();
    int ycount=0;
    int yline=0;
    
    //Check the returning code
    if (yhttpCode > 0) {
      // Get the request response payload
      String ydata = yhttp.getString();
      
      // output RAW data
      Serial.println("RAW DATA");
      Serial.println(ydata);

      // Parsing
      StaticJsonDocument<5000> doc;
      DeserializationError error = deserializeJson(doc, ydata);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        u8x8.setCursor(0, 1);
        u8x8.printf("deserializeJson\nfailed:\n%s", error.c_str());
        analogWrite(LED_BUILTIN, 0);
        delay(displayScroll);
        return;
      }

      JsonObject obj=doc["peers"].as<JsonObject>();
      for (JsonObject::iterator it=obj.begin(); it!=obj.end(); ++it) {
        ycount++;
        yline++;
        Serial.print("Peer: ");
        Serial.print(it->key().c_str());
        Serial.print(" Endpoint ");
        const char* endpoint = doc["peers"][it->key()]["endpoint"];
        Serial.print(endpoint);
        Serial.print(" Uptime ");
        const int uptime = doc["peers"][it->key()]["uptime"]; // uptime
        Serial.println(uptime);

        // output stuff to the screen 
        char *yspeer = strrchr(it->key().c_str(), ':');
        if (yspeer != NULL) {
          if (yline <= 7){
            u8x8.clearLine(yline);
            u8x8.setCursor(0, yline);
            u8x8.printf("%s\n",yspeer+1);
          }
          else {
            delay(displayScroll);
            u8x8.clearLine(1);
            u8x8.clearLine(2);
            u8x8.clearLine(3);
            u8x8.clearLine(4);
            u8x8.clearLine(5);
            u8x8.clearLine(6);
            u8x8.clearLine(7);
            yline=1;
            u8x8.setCursor(0, yline);
            u8x8.printf("%s\n",yspeer+1);
          }
        }
      }
    }
    //Close connection
    yhttp.end();
    // turn LED off to indicate done processing Yggdrasil data
    analogWrite(LED_BUILTIN, 0);
    delay(displayScroll);

    // cjdns
    // turn LED on to indicate processing cjdns data
    analogWrite(LED_BUILTIN, 64);
    // get Yggdrasil data from node over HTTP
    // clear screen and print cjdns peers
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.printf("cjdns Peers");
    
    // object of class HTTPClient
    Serial.println("Starting HTTPClient http");
    HTTPClient chttp;
    Serial.println("chttp.begin: " + curl);
    chttp.begin(curl);
    chttp.setTimeout(5000);
    Serial.println("chttp.GET");
    int chttpCode = chttp.GET();
    int ccount=0;
    int cline=0;
    
    //Check the returning code
    if (chttpCode > 0) {
      // Get the request response payload
      String cdata = chttp.getString();
      
      // output RAW data
      Serial.println("RAW DATA");
      Serial.println(cdata);

      // Parsing
      StaticJsonDocument<5000> doc;
      DeserializationError error = deserializeJson(doc, cdata);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        u8x8.setCursor(0, 1);
        u8x8.printf("deserializeJson\nfailed:\n%s", error.c_str());
        analogWrite(LED_BUILTIN, 0);
        delay(displayScroll);
        return;
      }

      JsonObject obj=doc["peers"].as<JsonObject>();
      for (JsonObject::iterator it=obj.begin(); it!=obj.end(); ++it) {
        ccount++;
        cline++;
        Serial.print("Peer: ");
        Serial.println(it->key().c_str());

        // output stuff to the screen 
        char *cspeer = strrchr(it->key().c_str(), ':');
        if (cspeer != NULL) {
          if (yline <= 7){
            u8x8.clearLine(cline);
            u8x8.setCursor(0, cline);
            u8x8.printf("%s\n",cspeer+1);
          }
          else {
            delay(displayScroll);
            u8x8.clearLine(1);
            u8x8.clearLine(2);
            u8x8.clearLine(3);
            u8x8.clearLine(4);
            u8x8.clearLine(5);
            u8x8.clearLine(6);
            u8x8.clearLine(7);
            cline=1;
            u8x8.setCursor(0, cline);
            u8x8.printf("%s\n",cspeer+1);
          }
        }
      }
    }
    //Close connection
    chttp.end();
    // turn LED off to indicate done processing cjdns data
    analogWrite(LED_BUILTIN, 0);
    delay(displayScroll);
}
