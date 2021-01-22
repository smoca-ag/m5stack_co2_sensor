// hardware
#include <m5stack_core2/pins_arduino.h>
#include <Arduino.h>
#include <M5Core2.h>
#include <SparkFun_SCD30_Arduino_Library.h>

// memory
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <sys/time.h>
#include <SPIFFS.h>

// WiFi AccessPoint and ConfigPortal
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsync_WiFiManager.h>
#include <ArduinoJson.h>

// time sync
#include <NTPClient.h>

// firmware update
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Update.h>

// mqtt
#include <PubSubClient.h>

#include <smoca_logo.h>

#define VERSION_NUMBER "1.0.0"
#define FIRMWARE_SERVER "co2-sensor-firmware.smoca.ch"
#define REMOTE_VERSION_FILE "/version.json"
#define REMOTE_FIRMWARE_FILE "/firmware.bin"

#define _ESPASYNC_WIFIMGR_LOGLEVEL_ 3

#define GRAPH_UNITS 240

#define NUM_WIFI_CREDENTIALS 1
#define MAX_SSID_LEN 32
#define MAX_PW_LEN 64
#define MIN_AP_PASSWORD_SIZE 8
#define USE_DHCP_IP true
#define USE_CONFIGURABLE_DNS true

#define TIME_SYNC_HOUR 2
#define TIME_SYNC_MIN rand() % 60

#define STATE_FILENAME "/state"
#define MQTT_FILENAME "/mqtt.json"
#define CONFIG_FILENAME "/wifi_config"

#define TOPIC_CO2 "co2"
#define TOPIC_HUMIDITY "humidity"
#define TOPIC_TEMPERATURE "temperature"

#define MQTT_SERVER_Label "MQTT_SERVER_Label"
#define MQTT_SERVERPORT_Label "MQTT_SERVERPORT_Label"
#define MQTT_DEVICENAME_Label "MQTT_DEVICENAME_Label"
#define MQTT_USERNAME_Label "MQTT_USERNAME_Label"
#define MQTT_KEY_Label "MQTT_KEY_Label"

#define MQTT_SERVER_LEN 20
#define MQTT_PORT_LEN 5
#define MQTT_DEVICENAME_LEN 40
#define MQTT_USERNAME_LEN 40
#define MQTT_KEY_LEN 40

String randomPassword();

void loadStateFile();

void saveStateFile(struct state *oldstate, struct state *state);

void initAPIPConfigStruct(WiFi_AP_IPConfig &in_WM_AP_IPconfig);

void initSTAIPConfigStruct(WiFi_STA_IPConfig &in_WM_STA_IPconfig);

void initWiFi();

void initSD();

void initAirSensor();

void checkIntervals(struct state *state);

void checkWiFi();

uint8_t connectMultiWiFi();

void disconnectWiFi(bool wifiOff, bool eraseAP);

void configWiFi(WiFi_STA_IPConfig in_WM_STA_IPconfig);

void displayIPConfigStruct(WiFi_STA_IPConfig in_WM_STA_IPconfig);

void loadMQTTConfig();

void saveMQTTConfig(struct state *state);

void loadConfigData();

void saveConfigData();

void handleNavigation(struct state *state);

void handleWiFi(struct state *oldstate, struct state *state);

void startWiFiManager(ESPAsync_WiFiManager *ESPAsync_WiFiManager, struct state *oldstate, struct state *state);

void resetWiFiManager(ESPAsync_WiFiManager *ESPAsync_WiFiManager, struct state *state);

void resetCallback();

void saveConfigPortalCredentials(ESPAsync_WiFiManager *ESPAsync_WiFiManager);

bool areRouterCredentialsValid();

void setupWiFiManager(ESPAsync_WiFiManager *ESPAsync_WiFiManager);

void publishMQTT(struct state *state);

bool MQTTConnect(struct state *state);

void handleUpdate(struct state *oldstate, struct state *state);

void fetchRemoteVersion(struct state *state);

void updateTouch(struct state *state);

void updateTime(struct state *state);

void updateBattery(struct state *state);

void updateCo2(struct state *state);

void updateGraph(struct state *oldstate, struct state *state);

void updateLed(struct state *oldstate, struct state *state);

void updatePassword(struct state *state);

void saveStateFile(struct state *oldstate, struct state *state);

void updateWiFiState(struct state *oldstate, struct state *state);

void updateTimeState(struct state *oldstate, struct state *state);

void updateMQTT(struct state *state);

void createSprites();

uint16_t co2color(int value);

void drawScreen(struct state *oldstate, struct state *state);

void drawHeader(struct state *oldstate, struct state *state);

void drawValues(struct state *oldstate, struct state *state);

void drawGraph(struct state *oldstate, struct state *state);

void drawCalibrationSettings(struct state *oldstate, struct state *state);

void drawCalibrationAlert(struct state *oldstate, struct state *state);

void drawWiFiSettings(struct state *oldstate, struct state *state);

void drawMQTTSettings(struct state *oldstate, struct state *state);

void drawSyncSettings(struct state *oldstate, struct state *state);

void drawUpdateSettings(struct state *oldstate, struct state *state);

void hideButtons();

void clearScreen(struct state *oldstate, struct state *state);

bool needFirmwareUpdate(const char *deviceVersion, const char *remoteVersion);

void writeSsd(struct state *state);

String padTwo(String input);

void writeFile(fs::FS &fs, const char *path, const char *message);

void printTime();

void syncData(struct state *state);

void setRtc(struct state *state);

void setTimeFromRtc();

void appendFile(fs::FS &fs, const char *path, const char *message);

void setDisplayPower(bool state);

uint32_t Read32bit(uint8_t Addr);

uint32_t ReadByte(uint8_t Addr);

void WriteByte(uint8_t Addr, uint8_t Data);
