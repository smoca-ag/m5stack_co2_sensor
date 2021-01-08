
/* 
 * This file is part of the co2sensor distribution (https://github.com/xxxx or http://xxx.github.io).
 * Copyright (c) 2020 David Gunzinger / smoca AG.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Libraries for SD card
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <sys/time.h>
#include <SPIFFS.h>

// Libraries for WiFi AccessPoint and ConfigPortal
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <ESPAsync_WiFiManager.h> // https://github.com/khoih-prog/ESPAsync_WiFiManager
                                  // you may need to install: 
                                  // https://github.com/me-no-dev/ESPAsyncWebServer
                                  // https://github.com/me-no-dev/AsyncTCP

#include <M5Core2.h>
#include <stdio.h>
#include <SparkFun_SCD30_Arduino_Library.h> // https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library

#include "Logo.h"

#define NUM_WIFI_CREDENTIALS 1
#define MAX_SSID_LEN 32
#define MAX_PW_LEN 64
#define MIN_AP_PASSWORD_SIZE 8
#define USE_DHCP_IP true
#define USE_CONFIGURABLE_DNS true

#define GRAPH_UNITS 240

#define STATE_FILENAME "/state"
#define CONFIG_FILENAME "/wifi_config"

typedef struct {
  char wifi_ssid[MAX_SSID_LEN];
  char wifi_pw[MAX_PW_LEN];
} WiFiCredentials;

typedef struct {
  String wifi_ssid;
  String wifi_pw;
} WiFiCredentialsString;

typedef struct {
  WiFiCredentials WiFi_Creds[NUM_WIFI_CREDENTIALS];
} WM_Config;

enum graphMode {
  graphModeCo2,
  graphModeTemperature,
  graphModeHumidity,
  graphModeBatteryMah,
  graphModeLogo
} ;

enum menuMode {
  menuModeDisplay,
  menuModeCalibration
} ;

struct state {
  int co2_ppm;
  int temperature_celsius;
  int humidity_percent;
  int battery_percent;
  float battery_mah;
  float battery_voltage;
  float battery_current;
  bool in_ac;
  struct tm current_time;
  int graph_index;
  enum graphMode graph_mode;
  bool display_sleep = false;
  float battery_capacity;
  enum menuMode menu_mode;
  bool auto_calibration_on = false;
  int calibration_value = 400;
  bool is_wifi_on = false; // todo: just connect if true
  bool is_requesting_cp = false;
} state;

struct graph {
  float co2[GRAPH_UNITS];
  float temperature[GRAPH_UNITS];
  float humidity[GRAPH_UNITS];
  float batteryMah[GRAPH_UNITS];
} graph;

// x, y, w, h
Button batteryButton(240, 0, 80, 40);
Button co2Button(0, 26, 320, 88);
Button midLeftButton(0, 114, 160,  40);
Button midRightButton(160, 114, 160, 40);
Button toggleAutoCalibrationButton(160, 166, 60, 50);
Button changeCalibrationButton(240, 166, 60, 50);

TFT_eSprite DisbuffHeader = TFT_eSprite(&M5.Lcd);
TFT_eSprite DisbuffValue  = TFT_eSprite(&M5.Lcd);
TFT_eSprite DisbuffGraph  = TFT_eSprite(&M5.Lcd);
TFT_eSprite DisbuffBody   = TFT_eSprite(&M5.Lcd);

String ssid               = "smoca CO2-" + String(ESP_getChipId(), HEX);
const char* password      = "12345678";

IPAddress stationIP       = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP       = IPAddress(192, 168, 2, 1);
IPAddress netMask         = IPAddress(255, 255, 255, 0);

IPAddress dns1IP          = gatewayIP;
IPAddress dns2IP          = IPAddress(8, 8, 8, 8);

IPAddress APStaticIP      = IPAddress(192, 168, 100, 1);
IPAddress APStaticGW      = IPAddress(192, 168, 100, 1);
IPAddress APStaticSN      = IPAddress(255, 255, 255, 0);

bool initialConfig        = false;

AsyncWebServer webServer(80);

SCD30 airSensor;

WM_Config WM_config;
WiFi_AP_IPConfig WM_AP_IPconfig;
WiFi_STA_IPConfig WM_STA_IPconfig;
WiFiMulti wifiMulti;

DNSServer dnsServer;
String Router_SSID;
String Router_Pass;

// statistics
unsigned long cycle;
int target_fps = 20;
int frame_duration_ms = 1000 / target_fps;
float my_nan;

void setup() {
  Serial.println("Start Setup.");
  my_nan = sqrt(-1);

  M5.begin();
  M5.Lcd.setSwapBytes(true);
  M5.Lcd.pushImage(96, 96, 128, 32, smoca);

  M5.Axp.SetCHGCurrent(AXP192::kCHG_280mA);
  M5.Axp.EnableCoulombcounter();
  M5.Axp.SetLed( M5.Axp.isACIN() ? 1 : 0);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  startConfigurationPanel();
  loadPersistentState();
  setDisplayPower(true);
  setTimeFromRtc();
  printTime();
  createSprites();

  for (int i = 0; i < GRAPH_UNITS; i ++) {
    graph.temperature[i] = my_nan;
    graph.co2[i] = my_nan;
    graph.humidity[i] = my_nan;
    graph.batteryMah[i] = my_nan;
  }

  initSD();
  initAirSensor();
  cycle = 0;
}

void loop() {
  unsigned long start = millis();
  struct state oldstate;
  memcpy(&oldstate, &state, sizeof(struct state));
  M5.update();

  requestConfigurationPanel(&oldstate, &state);
  checkStatus();

  updateTouch(&state);
  updateTime(&state);
  updateBattery(&state);
  updateCo2(&state);
  updateGraph(&oldstate, &state);
  updateLed(&oldstate, &state);

  if (!state.display_sleep) {
    drawHeader(&oldstate, &state);

    if (state.menu_mode == menuModeDisplay) {
      clearBody(&oldstate, &state);
      drawValues(&oldstate, &state);
      drawGraph(&oldstate, &state);
    } else if (state.menu_mode == menuModeCalibration) {
      drawCalibration(&oldstate, &state);
    }
  }

  writeSsd(&state);
  cycle++;

  unsigned long duration = millis() - start ;
  if (duration < frame_duration_ms) {
    delay(frame_duration_ms - duration);
  } else {
    Serial.println("we are to slow:" + String(duration));
  }
}

void createSprites() {
  DisbuffHeader.createSprite(320, 26);
  DisbuffHeader.setFreeFont(&FreeMono9pt7b);
  DisbuffHeader.setTextSize(1);
  DisbuffHeader.setTextColor(TFT_WHITE);
  DisbuffHeader.setTextDatum(TL_DATUM);
  DisbuffHeader.fillRect(0, 0, 320, 25, TFT_BLACK);
  DisbuffHeader.drawLine(0, 25, 320, 25, TFT_WHITE);

  DisbuffValue.createSprite(320, 117);
  DisbuffValue.setTextSize(1);
  DisbuffValue.setTextColor(TFT_WHITE);
  DisbuffValue.setTextDatum(TC_DATUM);
  DisbuffValue.fillRect(0, 0, 320, 116, TFT_BLACK);

  DisbuffGraph.createSprite(320, 97);
  DisbuffGraph.setFreeFont(&FreeMono9pt7b);
  DisbuffGraph.setTextSize(1);
  DisbuffGraph.setTextColor(TFT_WHITE);
  DisbuffGraph.setTextDatum(TC_DATUM);
  DisbuffGraph.fillRect(0, 0, 320, 97, TFT_BLACK);

  DisbuffBody.createSprite(320, 214);
}

void loadPersistentState() {
  File file = SPIFFS.open(STATE_FILENAME, "r");

  if (file) {
    String battery_string =  file.readStringUntil('\n');
    String auto_cal_string = file.readStringUntil('\n');
    String calibration_string = file.readStringUntil('\n');
    file.close();

    if (battery_string.length() > 0) {
      state.battery_capacity = battery_string.toFloat();
    }
    if (state.battery_capacity == 0) {
      state.battery_capacity = 700;
    }

    state.auto_calibration_on = auto_cal_string == "1" ? true : false;
    state.calibration_value = calibration_string.toInt() < 400 ? 400 : calibration_string.toInt();

    Serial.println("battery capacity: " + String(state.battery_capacity));
    Serial.println("auto cal: " + String(state.auto_calibration_on));
    Serial.println("calibration: " + String(state.calibration_value));
  } else {
    Serial.println("state file could not be read.");
  }
}

void updateStateFile(struct state *state) {
  File f = SPIFFS.open(STATE_FILENAME, "w");
  f.print(
    (String)state->battery_capacity + "\n" +
    (String)state->auto_calibration_on + "\n" +
    (String)state->calibration_value + "\n"
  );
  f.close();
}

void initAPIPConfigStruct(WiFi_AP_IPConfig &in_WM_AP_IPconfig) {
  in_WM_AP_IPconfig._ap_static_ip = APStaticIP;
  in_WM_AP_IPconfig._ap_static_gw = APStaticGW;
  in_WM_AP_IPconfig._ap_static_sn = APStaticSN;
}

void initSTAIPConfigStruct(WiFi_STA_IPConfig &in_WM_STA_IPconfig) {
  in_WM_STA_IPconfig._sta_static_ip = stationIP;
  in_WM_STA_IPconfig._sta_static_gw = gatewayIP;
  in_WM_STA_IPconfig._sta_static_sn = netMask;
#if USE_CONFIGURABLE_DNS  
  in_WM_STA_IPconfig._sta_static_dns1 = dns1IP;
  in_WM_STA_IPconfig._sta_static_dns2 = dns2IP;
#endif
}

void initSD() {
    SD.begin();
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
  }

  Serial.println("Initializing SD card...");
  if (!SD.begin()) {
    Serial.println("ERROR - SD card initialization failed!");
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if (!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Date, Co2 (ppm), Temperature, Humidity, Battery Charge \r\n");
  } else {
    Serial.println("File already exists");
  }

  file.close();
}

void initAirSensor() {
  Wire.begin(G32, G33);

  if (airSensor.begin(Wire, state.auto_calibration_on) == false)
  {
    DisbuffValue.setTextColor(TFT_RED);
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    DisbuffValue.drawString("Air sensor not detected.", 0, 0);
    DisbuffValue.drawString("Please check wiring.", 0, 25);
    DisbuffValue.drawString("Freezing.", 0, 50);
    DisbuffValue.pushSprite(0, 26);
    while (1) {
      delay(1000);
    };
  }

  airSensor.setTemperatureOffset(5.5);
  airSensor.setAltitudeCompensation(440);
}

void checkWiFi() {
  if ( (WiFi.status() != WL_CONNECTED) ) {
    Serial.println("WiFi lost. Call connectMultiWiFi in loop");
    connectMultiWiFi();
  }
}

void checkStatus() {
  static ulong checkwifi_timeout = 0;
  static ulong current_millis;

  #define WIFICHECK_INTERVAL 1000L
  current_millis = millis();
  
  if ((current_millis > checkwifi_timeout) || (checkwifi_timeout == 0)) {
    checkWiFi();
    checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
  }
}

void configWiFi(WiFi_STA_IPConfig in_WM_STA_IPconfig) {
  #if USE_CONFIGURABLE_DNS  
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    WiFi.config(in_WM_STA_IPconfig._sta_static_ip, in_WM_STA_IPconfig._sta_static_gw, in_WM_STA_IPconfig._sta_static_sn, in_WM_STA_IPconfig._sta_static_dns1, in_WM_STA_IPconfig._sta_static_dns2);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    WiFi.config(in_WM_STA_IPconfig._sta_static_ip, in_WM_STA_IPconfig._sta_static_gw, in_WM_STA_IPconfig._sta_static_sn);
  #endif 
}

void displayIPConfigStruct(WiFi_STA_IPConfig in_WM_STA_IPconfig) {
  LOGERROR3(F("stationIP ="), in_WM_STA_IPconfig._sta_static_ip, ", gatewayIP =", in_WM_STA_IPconfig._sta_static_gw);
  LOGERROR1(F("netMask ="), in_WM_STA_IPconfig._sta_static_sn);
#if USE_CONFIGURABLE_DNS
  LOGERROR3(F("dns1IP ="), in_WM_STA_IPconfig._sta_static_dns1, ", dns2IP =", in_WM_STA_IPconfig._sta_static_dns2);
#endif
}

void loadConfigData() {
  File file = SPIFFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));
  memset(&WM_config, 0, sizeof(WM_config));
  memset(&WM_STA_IPconfig, 0, sizeof(WM_STA_IPconfig));
    
  if (file) {
    file.readBytes((char *) &WM_config,   sizeof(WM_config));
    file.readBytes((char *) &WM_STA_IPconfig, sizeof(WM_STA_IPconfig));
    file.close();
    LOGERROR(F("OK"));
    displayIPConfigStruct(WM_STA_IPconfig);
  } else {
    LOGERROR(F("failed"));
  }
}

void saveConfigData() {
  File file = SPIFFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file) {
    file.write((uint8_t*) &WM_config, sizeof(WM_config));
    file.write((uint8_t*) &WM_STA_IPconfig, sizeof(WM_STA_IPconfig));
    file.close();
    LOGERROR(F("OK"));
  } else {
    LOGERROR(F("failed"));
  }
}

uint8_t connectMultiWiFi() {
  #define WIFI_MULTI_1ST_CONNECT_WAITING_MS 0
  #define WIFI_MULTI_CONNECT_WAITING_MS 100L
  uint8_t status;
  LOGERROR(F("ConnectMultiWiFi with :"));
  
  if ( (Router_SSID != "") && (Router_Pass != "") ) {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) ) {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }
  
  LOGERROR(F("Connecting MultiWifi..."));
  WiFi.mode(WIFI_STA);
  
#if !USE_DHCP_IP
  configWiFi(WM_STA_IPconfig);
#endif

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 20 ) && ( status != WL_CONNECTED ) ) {
    status = wifiMulti.run();

    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED ) {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

void startConfigurationPanel() {
  initAPIPConfigStruct(WM_AP_IPconfig);
  initSTAIPConfigStruct(WM_STA_IPconfig);

  unsigned long startedAt = millis();
  ESPAsync_WiFiManager ESPAsync_WiFiManager(&webServer, &dnsServer);
  ESPAsync_WiFiManager.setDebugOutput(true);
  ESPAsync_WiFiManager.setAPStaticIPConfig(WM_AP_IPconfig); // set custom IP
  ESPAsync_WiFiManager.setMinimumSignalQuality(-1);
  ESPAsync_WiFiManager.setConfigPortalChannel(0);

#if !USE_DHCP_IP    
    #if USE_CONFIGURABLE_DNS  
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    ESPAsync_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    ESPAsync_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask);
  #endif 
#endif     

  // can't use WiFi.SSID() in ESP32as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // todo: create a new function to store in EEPROM/SPIFFS for this purpose
  //Router_SSID = ESPAsync_wifiManager.WiFi_SSID();
  //Router_Pass = ESPAsync_wifiManager.WiFi_Pass();

  ssid.toUpperCase();
  if ( (Router_SSID != "") && (Router_Pass != "") ) {
    LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
    ESPAsync_WiFiManager.setConfigPortalTimeout(120);
    Serial.println("Got stored Credentials. Timeout 120s for Config Portal");
  } else {
    Serial.println("Open Config Portal without Timeout: No stored Credentials.");
    initialConfig = true;
  }

  if (initialConfig) {
    Serial.println("Starting configuration portal.");
    if (!ESPAsync_WiFiManager.startConfigPortal((const char *) ssid.c_str(), password))
      Serial.println("Not connected to WiFi but continuing anyway.");
    else
      Serial.println("WiFi connected :D");

    memset(&WM_config, 0, sizeof(WM_config));
    
    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
      String tempSSID = ESPAsync_WiFiManager.getSSID(i);
      String tempPW   = ESPAsync_WiFiManager.getPW(i);
  
      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);  

      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) ) {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    ESPAsync_WiFiManager.getSTAStaticIPConfig(WM_STA_IPconfig);
    displayIPConfigStruct(WM_STA_IPconfig);
    saveConfigData();
  }

  startedAt = millis();

  if (!initialConfig) {
    loadConfigData();

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) ) {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if ( WiFi.status() != WL_CONNECTED ) {
      Serial.println("ConnectMultiWiFi in setup");
      connectMultiWiFi();
    }
  }

  Serial.print("After waiting ");
  Serial.print((float) (millis() - startedAt) / 1000L);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  } else
    Serial.println(ESPAsync_WiFiManager.getStatus(WiFi.status()));
}

void requestConfigurationPanel(struct state *oldstate, struct state *state) {
  if (state->is_requesting_cp != oldstate->is_requesting_cp &&
      state->is_requesting_cp
  ) {
    ESPAsync_WiFiManager ESPAsync_WiFiManager(&webServer, &dnsServer);
    ESPAsync_WiFiManager.setMinimumSignalQuality(-1);
    ESPAsync_WiFiManager.setConfigPortalChannel(0);
    ESPAsync_WiFiManager.setAPStaticIPConfig(WM_AP_IPconfig);

#if !USE_DHCP_IP    
  #if USE_CONFIGURABLE_DNS  
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    ESPAsync_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    ESPAsync_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask);
  #endif 
#endif  

    Router_SSID = ESPAsync_WiFiManager.WiFi_SSID();
    Router_Pass = ESPAsync_WiFiManager.WiFi_Pass();

    if(!ESPAsync_WiFiManager.startConfigPortal((const char *)ssid.c_str(), password)) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      Serial.println("connected :D");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
    }

    if ( String(ESPAsync_WiFiManager.getSSID(0)) != "" && String(ESPAsync_WiFiManager.getSSID(1)) != "" ) {
      memset(&WM_config, 0, sizeof(WM_config));
      
      for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
        String tempSSID = ESPAsync_WiFiManager.getSSID(i);
        String tempPW   = ESPAsync_WiFiManager.getPW(i);
    
        if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
          strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
        else
          strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);
    
        if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
          strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
        else
          strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);  
    
        if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) ) {
          LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
          wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
        }
      }

      saveConfigData();
      state->is_requesting_cp = false;
    }
  }
}

void updateTouch(struct state *state) {
  if (state->menu_mode == menuModeDisplay) {
    if (batteryButton.wasPressed()) state->graph_mode = graphModeBatteryMah;
    if (co2Button.wasPressed()) state->graph_mode = graphModeCo2;
    if (midLeftButton.wasPressed()) state->graph_mode = graphModeTemperature;
    if (midRightButton.wasPressed()) state->graph_mode = graphModeHumidity;
  } else if (state->menu_mode == menuModeCalibration) {
    if (midLeftButton.wasPressed()) {
      state->calibration_value -= state->calibration_value >= 410 ? 10 : 0;
    }
    if (midRightButton.wasPressed()) {
      state->calibration_value += state->calibration_value <= 1990 ? 10 : 0;
    }
    if (toggleAutoCalibrationButton.wasPressed()) {
      state->auto_calibration_on = !state->auto_calibration_on;
      airSensor.setAutoSelfCalibration(state->auto_calibration_on);
      updateStateFile(state);
    } 
    if (changeCalibrationButton.wasPressed()) {
      airSensor.setForcedRecalibrationFactor(state->calibration_value);
      state->menu_mode = menuModeDisplay;
      updateStateFile(state);
    }
  }

  if (M5.BtnA.wasPressed()) {
    setDisplayPower(state->display_sleep);
    state->display_sleep = !state->display_sleep;
  }
  if (M5.BtnB.wasPressed()) state->menu_mode = menuModeDisplay;
  if (M5.BtnC.wasPressed()) state->menu_mode = menuModeCalibration;
}

void updateTime(struct state *state) {
  if (((cycle + 1) % target_fps) != 0) {
    return;
  }
  if (!getLocalTime(&(state->current_time))) {
    Serial.println("Failed to obtain time");
  }
}

void updateBattery(struct state *state) {
  if (((cycle + 1) % (target_fps)) != 0) {
    return;
  }

  int columbCharged = Read32bit(0xB0);
  int columbDischarged = Read32bit(0xB4);
  float sig = columbCharged > columbDischarged ? 1.0 : -1.0;
  float batVoltage = M5.Axp.GetBatVoltage();
  state->battery_current = M5.Axp.GetBatCurrent();

  if (batVoltage < 3.2 && columbDischarged > columbCharged) {
    M5.Axp.ClearCoulombcounter();
    M5.Axp.EnableCoulombcounter();
  }

  state->battery_voltage = batVoltage;
  state->battery_mah = 65536  * 0.5 *  (columbCharged - columbDischarged) / 3600.0 / 25.0;

  if (state->in_ac && abs(state->battery_current) < 0.1 && state->battery_voltage >= 4.15 && abs(state->battery_mah - state->battery_capacity) > 1) {
    Serial.println("maximum found " + String(state->battery_mah));
    state->battery_capacity = state->battery_mah;
    updateStateFile(state);
  }

  int batteryPercent = state->battery_mah * 100 / state->battery_capacity;
  batteryPercent = max(min(100, batteryPercent), 0);

  Serial.println(
    "mAh: " + String(state->battery_mah) + 
    " charged: " + String(columbCharged) + 
    " discharched: " + String(columbDischarged) + 
    " batteryPercent " + String(batteryPercent) + 
    " current " + state->battery_current  + 
    " voltage " + String(state->battery_voltage)
  );
  state->battery_percent = batteryPercent;
  state->in_ac = M5.Axp.isACIN();
}

void updateLed(struct state *oldstate, struct state *state) {
  if (oldstate->in_ac != state->in_ac) {
    M5.Axp.SetLed(state->in_ac ? 1 : 0);
  }
}

void updateGraph(struct state *oldstate, struct state *state) {
  if (oldstate->current_time.tm_min == state->current_time.tm_min ||
      state->co2_ppm == 0) {
    return;
  }

  graph.co2[state->graph_index] = state->co2_ppm;
  graph.temperature[state->graph_index] = state->temperature_celsius / 10.0;
  graph.humidity[state->graph_index] = state->humidity_percent / 10.0;
  graph.batteryMah[state->graph_index] = state->battery_mah;
  state->graph_index = (state->graph_index + 1) % GRAPH_UNITS;
}

void updateCo2(struct state *state) {
  if (((cycle + 2) %  (2 * target_fps)) != 0) {
    return;
  }

  if (airSensor.dataAvailable()) {
    state->co2_ppm = airSensor.getCO2();
    state->temperature_celsius = airSensor.getTemperature() * 10;
    state->humidity_percent = airSensor.getHumidity() * 10;
  }
}

void drawHeader(struct state *oldstate, struct state *state) {
  if (
    state->current_time.tm_sec == oldstate->current_time.tm_sec &&
    state->battery_percent == oldstate->battery_percent &&
    state->in_ac == oldstate->in_ac &&
    state->display_sleep == oldstate->display_sleep) {
    return;
  }

  DisbuffHeader.fillRect(0, 0, 320, 24, TFT_BLACK);
  char strftime_buf[64];
  strftime(strftime_buf, sizeof(strftime_buf) - 1, "%c", &(state->current_time));
  DisbuffHeader.drawString(String(strftime_buf), 0, 1);
  DisbuffHeader.setTextDatum(TR_DATUM);
  DisbuffHeader.drawString(  String(state->battery_percent) + "%" + (state->in_ac ? "+" : "-"), 320, 0);
  DisbuffHeader.setTextDatum(TL_DATUM);
  DisbuffHeader.drawLine(0, 25, 320, 25, TFT_WHITE);
  DisbuffHeader.pushSprite(0, 0);
}

uint16_t co2color(int value) {
  if (value < 600) {
    return TFT_CYAN;
  } else if (value < 800) {
    return TFT_GREEN;
  } else if (value < 1000) {
    return TFT_YELLOW;
  } else if (value < 1400) {
    return TFT_ORANGE;
  } else {
    return TFT_RED;
  }
}

void drawValues(struct state *oldstate, struct state *state) {
  if (
    state->temperature_celsius == oldstate->temperature_celsius &&
    state->humidity_percent == oldstate->humidity_percent &&
    state->co2_ppm == oldstate->co2_ppm &&
    state->display_sleep == oldstate->display_sleep &&
    state->menu_mode == oldstate->menu_mode) {
    return;
  }

  DisbuffValue.fillRect(0, 0, 320, 116, TFT_BLACK);
  DisbuffValue.setFreeFont(&FreeMonoBold18pt7b);
  DisbuffValue.setTextColor(co2color(state->co2_ppm));

  DisbuffValue.setTextSize(2);
  DisbuffValue.drawString(String(state->co2_ppm) + "ppm", 160, 5);
  DisbuffValue.setTextSize(1);

  DisbuffValue.setTextColor(TFT_WHITE);
  DisbuffValue.setFreeFont(&FreeMono18pt7b);

  DisbuffValue.drawLine(0, 82, 320, 82, TFT_WHITE);
  DisbuffValue.drawString(String(state->temperature_celsius / 10.0, 1) + "C", 80, 88);
  DisbuffValue.drawString(String(state->humidity_percent / 10.0, 1) + "%", 240, 88);
  DisbuffValue.drawLine(160, 82, 160, 116, TFT_WHITE);
  DisbuffValue.drawLine(0, 116, 320, 116, TFT_WHITE);

  DisbuffValue.pushSprite(0, 26);
}

void drawGraph(struct state *oldstate, struct state *state) {
  if (state->graph_mode == oldstate->graph_mode &&
      state->graph_index == oldstate->graph_index &&
      state->display_sleep == oldstate->display_sleep &&
      state->menu_mode == oldstate->menu_mode) {
    return;
  }

  DisbuffGraph.fillRect(0, 0, 320, 97, TFT_BLACK);

  float *values = graph.co2;
  if (state->graph_mode == graphModeBatteryMah) {
    values = graph.batteryMah;
  } else if (state->graph_mode == graphModeTemperature) {
    values = graph.temperature;
  } else if (state->graph_mode == graphModeHumidity) {
    values = graph.humidity;
  } else if (state->graph_mode == graphModeCo2) {
    values = graph.co2;
  }

  int i;
  float sorted[GRAPH_UNITS];
  int value_count = 0;

  for (i = 0; i < GRAPH_UNITS; i++) {
    float value = values[i];
    if (!isnan(value)) {
      sorted[value_count++] = value;
    }
  }

  if (value_count == 0) {
    return;
  }

  std::sort(sorted, sorted + value_count);
  /*  Serial.print("[");

    for (i = 0; i < value_count; i++) {
      float value = sorted[i];
      Serial.print(String(value) + ", ");
    }
    Serial.println("]");*/

  int skip = GRAPH_UNITS * 2.5 / 100;
  float min_value = sorted[value_count > 10 * skip ? skip : 0];
  float max_value = sorted[value_count > 10 * skip ? value_count - 1 - skip : value_count - 1];
  int last_index = ((state->graph_index - 1) % GRAPH_UNITS + GRAPH_UNITS) % GRAPH_UNITS;

  min_value = min(min(min_value, values[last_index]), max_value);
  max_value = max(max(max_value, values[last_index]), min_value);

  float factor = 96 / (max_value - min_value);
  DisbuffGraph.drawString(String(max_value, 1), 0, 2);
  DisbuffGraph.drawString(String(min_value, 1), 0, 70);

  for (i = 0; i < GRAPH_UNITS ; i++) {
    float value = values[i];
    if (!isnan(value)) {
      int y = min(max(96 - int(factor * (value - min_value)), 0), 96);
      int x = 320 - (((state->graph_index - i) % GRAPH_UNITS + GRAPH_UNITS) % GRAPH_UNITS);
      uint16_t color = state->graph_mode == graphModeCo2 ? co2color(value) : TFT_WHITE;
      for (int j = y ; j < 96 ; j++) {
        DisbuffGraph.drawPixel(x, j, color);
      }
    }
  }

  Serial.println("graph done");
  DisbuffGraph.pushSprite(0, 144);
}

void drawCalibration(struct state *oldstate, struct state *state) {
  if (state->display_sleep == oldstate->display_sleep &&
      state->calibration_value == oldstate->calibration_value &&
      state->auto_calibration_on == oldstate->auto_calibration_on &&
      state->menu_mode == oldstate->menu_mode) {
    return;
  }

  DisbuffBody.fillRect(0, 0, 320, 214, TFT_BLACK);

  DisbuffBody.setFreeFont(&FreeMono18pt7b);
  DisbuffBody.setTextColor(TFT_WHITE);
  DisbuffBody.drawString("CO2 Offset: ", 60, 5);
  
  DisbuffBody.setFreeFont(&FreeMonoBold12pt7b);
  DisbuffBody.setTextColor(co2color(state->calibration_value));
  DisbuffBody.setTextSize(2);
  DisbuffBody.drawString(String(state->calibration_value) + "ppm", 80, 35);
  DisbuffBody.setTextSize(1);

  DisbuffBody.setFreeFont(&FreeMonoBold18pt7b);
  DisbuffBody.setTextColor(TFT_WHITE);
  DisbuffBody.drawLine(0, 80, 320, 80, TFT_WHITE);
  DisbuffBody.drawString("-", 80, 86);
  DisbuffBody.drawString("+", 240, 86);
  DisbuffBody.drawLine(160, 80, 160, 120, TFT_WHITE);
  DisbuffBody.drawLine(0, 120, 320, 120, TFT_WHITE);

  uint16_t autoCalibrationColor = state->auto_calibration_on ? TFT_GREEN : TFT_RED; 
  DisbuffBody.setFreeFont(&FreeMono9pt7b);
  DisbuffBody.drawString("Enable Auto", 15, 150);
  DisbuffBody.drawString("Calibration", 15, 175);
  DisbuffBody.setFreeFont(&FreeMono12pt7b);

  DisbuffBody.setTextColor(autoCalibrationColor);
  DisbuffBody.drawLine(160, 140, 220, 140, autoCalibrationColor);
  DisbuffBody.drawLine(160, 140, 160, 190, autoCalibrationColor);
  DisbuffBody.drawString(state->auto_calibration_on ? "On" : "Off", 170, 155);
  DisbuffBody.drawLine(160, 190, 220, 190, autoCalibrationColor);
  DisbuffBody.drawLine(220, 140, 220, 190, autoCalibrationColor);

  DisbuffBody.setTextColor(TFT_CYAN);
  DisbuffBody.drawLine(240, 140, 300, 140, TFT_CYAN);
  DisbuffBody.drawLine(240, 140, 240, 190, TFT_CYAN);
  DisbuffBody.drawString("Ok", 255, 155);
  DisbuffBody.drawLine(240, 190, 300, 190, TFT_CYAN);
  DisbuffBody.drawLine(300, 140, 300, 190, TFT_CYAN);

  DisbuffBody.pushSprite(0, 26);
}

void clearBody(struct state *oldstate, struct state *state) {
  if (oldstate->menu_mode != state->menu_mode) {
    DisbuffBody.fillRect(0, 0, 320, 214, TFT_BLACK);
    DisbuffBody.pushSprite(0, 26);
  }
}

void writeSsd(struct state * state) {
  if (((cycle + 3) % (2 * target_fps)) != 0) {
    return;
  }

  String dateTime = String(
    state->current_time.tm_year + 1900) + "-" +
    padTwo(String(state->current_time.tm_mon + 1)) + "-" +
    padTwo(String(state->current_time.tm_mday)) + "-" +
    padTwo(String(state->current_time.tm_hour)) + "-" +
    padTwo(String(state->current_time.tm_min)) + "-" +
    padTwo(String(state->current_time.tm_sec)
  );
  String dataMessage = 
    dateTime + "," + 
    String(state->co2_ppm) + "," + 
    String(state->temperature_celsius / 10.0, 2) + "," + 
    String(state->humidity_percent / 10.0, 2) + "," +
    String(state->battery_mah) + "\r\n";
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

String padTwo(String input) {
  if (input.length() == 2) {
    return input;
  }
  return "0" + input;
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS & fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }

  file.close();
}

void printTime() {
  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  Serial.printf("The current date/time in Zuerich is: %s", strftime_buf);

}

void setTimeFromRtc() {
  setenv("TZ", "UTC", 1);
  tzset();
  struct tm tm;
  RTC_TimeTypeDef rtctime;
  RTC_DateTypeDef rtcdate;
  M5.Rtc.GetDate(&rtcdate);
  M5.Rtc.GetTime(&rtctime);
  tm.tm_year = rtcdate.Year - 1900;
  tm.tm_mon = rtcdate.Month - 1;
  tm.tm_mday = rtcdate.Date;
  tm.tm_hour = rtctime.Hours;
  tm.tm_min = rtctime.Minutes;
  tm.tm_sec = rtctime.Seconds;
  // TC Correction, not supportet by system ??
  time_t t = mktime(&tm);

  Serial.printf("Setting time: %s", asctime(&tm));
  struct timeval epoch = { .tv_sec = t};
  settimeofday(&epoch, NULL);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS & fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void setDisplayPower(bool state) {
  if (state) {
    M5.Lcd.setBrightness(255);
    M5.Lcd.wakeup();
    // Enable DC-DC3, enable backlight
    WriteByte(0x12, (ReadByte(0x12) | 2));
  } else {
    M5.Lcd.setBrightness(0);
    M5.Lcd.sleep();
    // Disable DC-DC3, display backlight
    WriteByte(0x12, (ReadByte(0x12) & (~2)));
  }
}

uint32_t Read32bit( uint8_t Addr ) {
  uint32_t ReData = 0;
  Wire1.beginTransmission(0x34);
  Wire1.write(Addr);
  Wire1.endTransmission();
  Wire1.requestFrom(0x34, 4);

  for ( int i = 0 ; i < 4 ; i++ )
  {
    ReData <<= 8;
    ReData |= Wire1.read();
  }

  return ReData;
}

uint32_t ReadByte ( uint8_t Addr ) {
  uint32_t ReData = 0;
  Wire1.beginTransmission(0x34);
  Wire1.write(Addr);
  Wire1.endTransmission();
  Wire1.requestFrom(0x34, 1);
  return Wire1.read();
}

void WriteByte(uint8_t Addr, uint8_t Data) {
  Wire1.beginTransmission(0x34);
  Wire1.write(Addr);
  Wire1.write(Data);
  Wire1.endTransmission();
}
