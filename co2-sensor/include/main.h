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

#define VERSION_NUMBER "1.1.2"
#define VERSION_NUMBER_LEN 8
#define FIRMWARE_SERVER "co2-sensor-firmware.smoca.ch"
#define REMOTE_VERSION_FILE "/version.json"
#define REMOTE_FIRMWARE_FILE "/firmware.bin"

#define _ESPASYNC_WIFIMGR_LOGLEVEL_ 4

#define GRAPH_UNITS 240

#define NUM_WIFI_CREDENTIALS 1
#define MAX_SSID_LEN 32
#define MAX_PW_LEN 64
#define MIN_AP_PASSWORD_SIZE 8
#define USE_DHCP_IP true
#define USE_CONFIGURABLE_DNS true

#define CP_PASSWORD_GENERATION_LEN 8
#define MAX_CP_PASSWORD_LEN 16

#define TIME_SYNC_HOUR 2
#define TIME_SYNC_MIN rand() % 60

#define STATE_FILENAME "/state"
#define MQTT_FILENAME "/mqtt.json"
#define CONFIG_FILENAME "/wifi_config"

#define TOPIC_CO2 "/co2"
#define TOPIC_HUMIDITY "/humidity"
#define TOPIC_TEMPERATURE "/temperature"

#define MQTT_SERVER_Label "MQTT_SERVER_Label"
#define MQTT_SERVERPORT_Label "MQTT_SERVERPORT_Label"
#define MQTT_DEVICENAME_Label "MQTT_DEVICENAME_Label"
#define MQTT_TOPIC_Label "MQTT_TOPIC_Label"
#define MQTT_USERNAME_Label "MQTT_USERNAME_Label"
#define MQTT_KEY_Label "MQTT_KEY_Label"

#define MQTT_SERVER_LEN 64
#define MQTT_PORT_LEN 8
#define MQTT_DEVICENAME_LEN 24
#define MQTT_TOPIC_LEN 64
#define MQTT_USERNAME_LEN 24
#define MQTT_KEY_LEN 32

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
};

enum menuMode {
    menuModeGraphs,
    menuModeCalibrationSettings,
    menuModeCalibrationAlert,
    menuModeWiFiSettings,
    menuModeMQTTSettings,
    menuModeTimeSettings,
    menuModeUpdateSettings
};

enum info {
    infoCalSuccess,
    infoConfigPortalCredentials,
    infoWiFiConnected,
    infoWiFiFailed,
    infoWiFiLost,
    infoTimeSyncSuccess,
    infoTimeSyncFailed,
    infoUpdateFailed,
    infoEmpty
};

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
    enum menuMode menu_mode = menuModeGraphs;
    bool auto_calibration_on = false;
    int calibration_value = 400;
    enum info cal_info = infoEmpty;
    bool is_wifi_activated = false;
    bool is_config_running = false;
    bool is_requesting_reset = false;
    wl_status_t wifi_status = WL_DISCONNECTED;
    enum info wifi_info = infoEmpty;
    char password[MAX_CP_PASSWORD_LEN + 1];
    struct tm next_time_sync;
    bool is_sync_needed = false;
    bool force_sync = false;
    enum info time_info = infoEmpty;
    bool is_requesting_update = false;
    enum info update_info = infoEmpty;
    char newest_version[VERSION_NUMBER_LEN + 1];
    bool is_mqtt_connected = false;
    char mqttServer[MQTT_SERVER_LEN];
    char mqttPort[MQTT_PORT_LEN];
    char mqttDevice[MQTT_DEVICENAME_LEN];
    char mqttTopic[MQTT_TOPIC_LEN];
    char mqttUser[MQTT_USERNAME_LEN];
    char mqttPassword[MQTT_KEY_LEN];
};

struct graph {
    float co2[GRAPH_UNITS];
    float temperature[GRAPH_UNITS];
    float humidity[GRAPH_UNITS];
    float batteryMah[GRAPH_UNITS];
};

String randomPassword();

void loadStateFile();

void saveStateFile(struct state *oldstate, struct state *state);

void initSTAIPConfigStruct(WiFi_STA_IPConfig &in_WM_STA_IPconfig);

void initWiFi();

void initSD();

void initAirSensor();

void initAsyncWifiManager(struct state *state);

void checkIntervals(struct state *state);

void connectWiFi(struct state *state);

uint8_t connectMultiWiFi();

void configWiFi(WiFi_STA_IPConfig in_WM_STA_IPconfig);

void displayIPConfigStruct(WiFi_STA_IPConfig in_WM_STA_IPconfig);

void loadMQTTConfig();

void saveMQTTConfig(struct state *state);

bool loadConfigData();

void saveConfigData();

void handleNavigation(struct state *state);

void handleWiFi(struct state *oldstate, struct state *state);

void accessPointCallback(ESPAsync_WiFiManager *asyncWifiManager);

void configPortalCallback();

void saveConfigPortalCredentials();

bool areRouterCredentialsValid();

void publishMQTT(struct state *state);

void setMQTTServer(struct state *state);

bool MQTTConnect(struct state *state);

void handleFirmware(struct state *oldstate, struct state *state);

bool fetchRemoteVersion(struct state *state);

void updateTouch(struct state *state);

void updateTime(struct state *state);

void updateBattery(struct state *state);

void updateCo2(struct state *state);

void updateGraph(struct state *oldstate, struct state *state);

void updateLed(struct state *oldstate, struct state *state);

void setPassword(struct state *state);

void saveStateFile(struct state *oldstate, struct state *state);

void updateWiFiState(struct state *oldstate, struct state *state);

void updateWiFiInfo(struct state *oldstate, struct state *state);

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

bool setRtc(struct state *state);

void setTimeFromRtc();

void appendFile(fs::FS &fs, const char *path, const char *message);

void setDisplayPower(bool state);

uint32_t Read32bit(uint8_t Addr);

uint32_t ReadByte(uint8_t Addr);

void WriteByte(uint8_t Addr, uint8_t Data);
