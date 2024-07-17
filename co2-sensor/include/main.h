#define VERSION_NUMBER "1.1.9"
#define VERSION_NUMBER_LEN 8
#define FIRMWARE_SERVER "co2-sensor-firmware.smoca.ch"
#define REMOTE_VERSION_FILE "/version.json"
#define REMOTE_FIRMWARE_FILE "/firmware.bin"

#define _ESPASYNC_WIFIMGR_LOGLEVEL_ 0

#define GRAPH_UNITS 240

#define NUM_WIFI_CREDENTIALS 2
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

#define TOPIC_DISCOVERY "homeassistant/sensor/"
#define TOPIC_CO2 "/co2"
#define TOPIC_HUMIDITY "/humidity"
#define TOPIC_TEMPERATURE "/temperature"
#define TOPIC_BATTERY "/battery"
#define TOPIC_CONFIG "/config"
#define TOPIC_STATE "/state"

#define HOMEASSISTANT_UNIQUE_ID_Label "unique_id"
#define HOMEASSISTANT_NAME_Label "name"
#define HOMEASSISTANT_STATE_TOPIC_Label "state_topic"
#define HOMEASSISTANT_UNIT_OF_MEASURE_Label "unit_of_measure"
#define HOMEASSISTANT_VALUE_TEMPLATE_Label "value_template"
#define HOMEASSISTANT_DEVICE_Label "device"
#define HOMEASSISTANT_DEVICE_IDENTIFIERS_Label "identifiers"
#define HOMEASSISTANT_DEVICE_NAME_Label "name"
#define HOMEASSISTANT_DEVICE_MODEL_Label "model"
#define HOMEASSISTANT_DEVICE_MANUFACTURER_Label "manufacturer"
#define HOMEASSISTANT_DEVICE_CLASS_Label "device_class"

#define HOMEASSISTANT_STATE_CO2_Label "carbon_dioxide"
#define HOMEASSISTANT_STATE_HUMIDITY_Label "humidity"
#define HOMEASSISTANT_STATE_TEMPERATURE_Label "temperature"
#define HOMEASSISTANT_STATE_BATTERY_Label "battery"

#define HOMEASSISTANT_DEVICE_MODEL_Value "Smoca CO2 Sensor"
#define HOMEASSISTANT_DEVICE_MANUFACTURER_Value "Smoca AG"

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

#define DISCOVERY_IDENTIFIERS_LEN 72
#define DISCOVERY_DEVICE_MODEL_NAME_LEN 24
#define DISCOVERY_DEVICE_MANUFACTURER_NAME_LEN 16
#define DISCOVERY_UNIQUE_ID_LEN 16
#define DISCOVERY_DEVICE_NAME_CLASS_LEN 32
#define DISCOVERY_TOPIC_LEN 72
#define DISCOVERY_VALUE_TEMPLATE_LEN 32
#define DISCOVERY_UNIT_OF_MEASURE_LEN 8

#define WIFI_SCAN_INTERVAL 5000L
#define WIFI_CONNECT_TIMEOUT 5000L
#define MQTT_INTERVAL 2000L
#define MQTT_PUBLISH_INTERVAL 60000L
#define STRCPY(dst, src) if (strlcpy(dst, src, sizeof(dst)) >= sizeof(dst)) { Serial.println("not enugh space in dst for src"); } 

// hardware
#include <m5stack_core2/pins_arduino.h>
#include <Arduino.h>
#include <M5Core2.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <SparkFun_SCD4x_Arduino_Library.h>

// memory
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <sys/time.h>
#include <SPIFFS.h>

// WiFi AccessPoint and ConfigPortal
#include <WiFi.h>
#include <ESPAsync_WiFiManager.h>
#include <ArduinoJson.h>

// firmware update
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Update.h>

// mqtt
#include <PubSubClient.h>

#include <smoca_logo.h>

#include <set>
typedef struct
{
    char wifi_ssid[MAX_SSID_LEN];
    char wifi_pw[MAX_PW_LEN];
} WiFiCredentials;

typedef struct
{
    String wifi_ssid;
    String wifi_pw;
} WiFiCredentialsString;

typedef struct
{
    WiFiCredentials WiFi_Creds[NUM_WIFI_CREDENTIALS];
} WM_Config;

enum graphMode
{
    graphModeCo2,
    graphModeTemperature,
    graphModeHumidity,
    graphModeBatteryMah,
    graphModeLogo
};

enum menuMode
{
    menuModeGraphs,
    menuModeCalibrationPpmSettings,
    menuModeCalibrationTempSettings,
    menuModeCalibrationPpmAlert,
    menuModeCalibrationTempAlert,
    menuModeWiFiSettings,
    menuModeMQTTSettings,
    menuModeTimeSettings,
    menuModeUpdateSettings,
    menuModeRotationSettings
};

enum info
{
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

enum connectionState
{
    WiFi_down_MQTT_down = 0,
    WiFi_scan_MQTT_down = 1,
    WiFi_starting_MQTT_down = 2,
    WiFi_up_MQTT_down = 3,
    WiFi_up_MQTT_starting = 4,
    WiFi_up_MQTT_up = 5
};

struct state
{
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
    int calibration_ppm_value = 400;
    float calibration_temp_value = 22.0;
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
    enum connectionState connectionState = WiFi_down_MQTT_down;
    bool is_screen_rotated = false;
};

struct discoveryDeviceConfig
{
    char identifiers[DISCOVERY_IDENTIFIERS_LEN];
    char name[MQTT_DEVICENAME_LEN];
    char model[DISCOVERY_DEVICE_MODEL_NAME_LEN];
    char manufacturer[DISCOVERY_DEVICE_MANUFACTURER_NAME_LEN];
};

struct discoveryConfig
{
    char configTopic[MQTT_TOPIC_LEN];
    char uniqueId[DISCOVERY_UNIQUE_ID_LEN];
    char name[DISCOVERY_DEVICE_NAME_CLASS_LEN];
    char stateTopic[MQTT_TOPIC_LEN];
    char unitOfMeasure[DISCOVERY_UNIT_OF_MEASURE_LEN];
    char valueTemplate[DISCOVERY_VALUE_TEMPLATE_LEN];
    char deviceClass[DISCOVERY_DEVICE_NAME_CLASS_LEN];
    discoveryDeviceConfig device;
};

struct graph
{
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

void initDeviceDiscoveryConfig(struct discoveryDeviceConfig *config);

void initDiscoveryValueConfig(
    struct discoveryConfig *config,
    String configurationTopic,
    String uniqueId,
    String stateTopic,
    String name,
    String topic,
    String unitOfMeasure,
    String valueTemplate,
    String deviceClass
);

void initCo2DiscoveryConfig(struct discoveryConfig *config);

void initHumidityDiscoveryConfig(struct discoveryConfig *config);

void initTemperatureDiscoveryConfig(struct discoveryConfig *config);

void initBatteryDiscoveryConfig(struct discoveryConfig *config);

void handleWifiMqtt(struct state *oldstate, struct state *state);

void connectWiFi(struct state *state);

void configWiFi(WiFi_STA_IPConfig in_WM_STA_IPconfig);

void displayIPConfigStruct(WiFi_STA_IPConfig in_WM_STA_IPconfig);

void loadMQTTConfig();

void saveMQTTConfig(struct state *state);

bool loadConfigData();

void saveConfigData();

void handleNavigation(struct state *state);

void handleConfigPortal(struct state *oldstate, struct state *state);

void accessPointCallback(ESPAsync_WiFiManager *asyncWifiManager);

void configPortalCallback();

void saveConfigPortalCredentials();

bool areRouterCredentialsValid();

void setMQTTServer(struct state *state);

bool MQTTConnect(struct state *state);

void handleFirmware(struct state *oldstate, struct state *state);

bool fetchRemoteVersion(struct state *state);

void sendMQTTDiscoveryMessage(struct discoveryConfig *config);

void sendMQTTDiscoveryMessages(
    struct discoveryConfig *co2Config,
    struct discoveryConfig *humidityConfig,
    struct discoveryConfig *temperatureConfig,
    struct discoveryConfig *batteryConfig
);

void updateScreenRotation(struct state *oldstate, struct state *state);

void updateTouch(struct state *state);

void updateTime(struct state *state);

void updateBattery(struct state *state);

void updateCo2(struct state *state);

void updateGraph(struct state *oldstate, struct state *state);

void updateLed(struct state *oldstate, struct state *state);

void setPassword(struct state *state);

void saveStateFile(struct state *oldstate, struct state *state);

void updateWiFiInfo(struct state *oldstate, struct state *state);

void updateTimeState(struct state *oldstate, struct state *state);

void updateMQTT(struct state *state);

void createSprites();

uint16_t co2color(int value);

void drawScreen(struct state *oldstate, struct state *state);

void drawHeader(struct state *oldstate, struct state *state);

void drawValues(struct state *oldstate, struct state *state);

void drawGraph(struct state *oldstate, struct state *state);

void drawCalibrationPpmSettings(struct state *oldstate, struct state *state);

void drawCalibrationTempSettings(struct state *oldstate, struct state *state);

void drawCalibrationAlert(struct state *oldstate, struct state *state);

void drawCalibrationTempAlert(struct state *oldstate, struct state *state);

void drawWiFiSettings(struct state *oldstate, struct state *state);

void drawMQTTSettings(struct state *oldstate, struct state *state);

void drawSyncSettings(struct state *oldstate, struct state *state);

void drawUpdateSettings(struct state *oldstate, struct state *state);

void drawRotationSettings(struct state *oldstate, struct state *state);

void hideButtons();

void clearScreen(struct state *oldstate, struct state *state);

bool needFirmwareUpdate(const char *deviceVersion, const char *remoteVersion);

void writeSsd(struct state *state);

String padTwo(String input);

void writeFile(fs::FS &fs, const char *path, const char *message);

void printTime();

void syncData(struct state *state);

bool setRtc();

void setTimeFromRtc();

void appendFile(fs::FS &fs, const char *path, const char *message);

void setDisplayPower(bool state);

uint32_t Read32bit(uint8_t Addr);

uint32_t ReadByte(uint8_t Addr);

void WriteByte(uint8_t Addr, uint8_t Data);
