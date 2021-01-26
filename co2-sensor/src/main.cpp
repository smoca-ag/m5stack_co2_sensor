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

#include <main.h>

struct state state;
struct graph graph;

// background, text, outline
ButtonColors offWhite = {BLACK, WHITE, WHITE};
ButtonColors onWhite = {WHITE, BLACK, WHITE};
ButtonColors offCyan = {BLACK, CYAN, CYAN};
ButtonColors onCyan = {CYAN, BLACK, CYAN};
ButtonColors offRed = {BLACK, RED, RED};
ButtonColors onRed = {RED, BLACK, RED};
ButtonColors offGreen = {BLACK, GREEN, GREEN};
ButtonColors onGreen = {GREEN, BLACK, GREEN};

// x, y, w, h, rot, txt, off col, on color, txt pos, x-offset, y-offset, corner radius
Button batteryButton(240, 0, 80, 40);
Button co2Button(0, 26, 320, 88);

Button midLeftButton(-2, 104, 163, 40, false, "midLeft", offWhite, onWhite, BUTTON_DATUM, 0, 0, 0);
Button midRightButton(161, 104, 164, 40, false, "midRight", offWhite, onWhite, BUTTON_DATUM, 0, 0, 0);

Button toggleAutoCalButton(15, 175, 130, 50, false, "Auto Cal: OFF", offRed, onRed);
Button submitCalibrationButton(175, 175, 130, 50, false, "Calibrate", offCyan, onCyan);

Button toggleWiFiButton(20, 175, 120, 50, false, "OFF", offRed, onRed);
Button resetWiFiButton(180, 175, 120, 50, false, "Reset", offCyan, onCyan);

Button syncTimeButton(20, 175, 280, 50, false, "Sync Time", offCyan, onCyan);

TFT_eSprite DisbuffHeader = TFT_eSprite(&M5.Lcd);
TFT_eSprite DisbuffValue = TFT_eSprite(&M5.Lcd);
TFT_eSprite DisbuffGraph = TFT_eSprite(&M5.Lcd);
TFT_eSprite DisbuffBody = TFT_eSprite(&M5.Lcd);

String ssid = "smoca CO2-" + String(ESP_getChipId(), HEX);
IPAddress stationIP = IPAddress(0, 0, 0, 0);
IPAddress gatewayIP = IPAddress(192, 168, 2, 1);
IPAddress netMask = IPAddress(255, 255, 255, 0);

IPAddress dns1IP = gatewayIP;
IPAddress dns2IP = IPAddress(8, 8, 8, 8);

IPAddress APStaticIP = IPAddress(192, 168, 100, 1);
IPAddress APStaticGW = IPAddress(192, 168, 100, 1);
IPAddress APStaticSN = IPAddress(255, 255, 255, 0);

AsyncWebServer webServer(80);

SCD30 airSensor;

WM_Config WM_config;
WiFi_AP_IPConfig WM_AP_IPconfig;
WiFi_STA_IPConfig WM_STA_IPconfig;
WiFiMulti wifiMulti;

DNSServer dnsServer;
String Router_SSID;
String Router_Pass;

WiFiClient client;
PubSubClient mqtt(client);

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
    M5.Lcd.pushImage(96, 96, 128, 32, smoca_logo);

    M5.Axp.SetCHGCurrent(AXP192::kCHG_280mA);
    M5.Axp.EnableCoulombcounter();
    M5.Axp.SetLed(M5.Axp.isACIN() ? 1 : 0);

    mqtt.setBufferSize(512);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    loadStateFile();
    loadMQTTConfig();
    setDisplayPower(true);
    setTimeFromRtc();
    printTime();
    createSprites();
    hideButtons();

    if (!state.next_time_sync.tm_year) {
        state.next_time_sync.tm_year = 1900;
        state.next_time_sync.tm_hour = TIME_SYNC_HOUR;
        state.next_time_sync.tm_min = TIME_SYNC_MIN;
    }

    for (int i = 0; i < GRAPH_UNITS; i++) {
        graph.temperature[i] = my_nan;
        graph.co2[i] = my_nan;
        graph.humidity[i] = my_nan;
        graph.batteryMah[i] = my_nan;
    }

    initWiFi();
    initSD();
    initAirSensor();
    initAPIPConfigStruct(WM_AP_IPconfig);
    initSTAIPConfigStruct(WM_STA_IPconfig);
    cycle = 0;
    Serial.println(state.is_wifi_activated ? "WiFi on" : "WiFi off");
}

void loop() {
    unsigned long start = millis();
    struct state oldstate;
    memcpy(&oldstate, &state, sizeof(struct state));
    M5.update();

    updateTouch(&state);
    updateTime(&state);
    updateBattery(&state);
    updateCo2(&state);
    updateGraph(&oldstate, &state);
    updateLed(&oldstate, &state);
    updatePassword(&state);
    updateWiFiState(&oldstate, &state);
    updateTimeState(&oldstate, &state);
    updateMQTT(&state);

    syncData(&state);
    saveStateFile(&oldstate, &state);
    drawScreen(&oldstate, &state);

    handleWiFi(&oldstate, &state);
    checkIntervals(&state);
    handleFirmware(&oldstate, &state);

    writeSsd(&state);
    cycle++;

    unsigned long duration = millis() - start;
    if (duration < frame_duration_ms) {
        delay(frame_duration_ms - duration);
    } else {
        //Serial.println("we are to slow:" + String(duration));
    }
}

String randomPassword(int length) {
    char alphanum[63] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int strLen = sizeof(alphanum);
    String pwd;
    srand(millis());

    for (int i = 0; i < length; i++) {
        pwd += alphanum[rand() % strLen];
    }

    return pwd;
}

void loadStateFile() {
    File file = SPIFFS.open(STATE_FILENAME, "r");

    if (file) {
        String battery_string = file.readStringUntil('\n');
        String auto_cal_string = file.readStringUntil('\n');
        String calibration_string = file.readStringUntil('\n');
        String is_wifi_activated = file.readStringUntil('\n');
        String password = file.readStringUntil('\n');
        String newest_version = file.readStringUntil('\n');
        file.close();

        if (battery_string.length() > 0) {
            state.battery_capacity = battery_string.toFloat();
        }
        if (state.battery_capacity == 0) {
            state.battery_capacity = 700;
        }

        state.auto_calibration_on = auto_cal_string == "1" ? true : false;
        state.calibration_value = calibration_string.toInt() < 400 ? 400 : calibration_string.toInt();
        state.is_wifi_activated = is_wifi_activated == "1" ? true : false;
        strncpy(state.password, password.c_str(), MAX_CP_PASSWORD_LEN);
        strncpy(state.newest_version, newest_version.c_str(), VERSION_NUMBER_LEN);
        Serial.println("Loaded state file.");
    } else {
        Serial.println("state file could not be read.");
    }
}

void saveStateFile(struct state *oldstate, struct state *state) {
    if (state->battery_capacity == oldstate->battery_capacity &&
        state->auto_calibration_on == oldstate->auto_calibration_on &&
        state->calibration_value == oldstate->calibration_value &&
        state->is_wifi_activated == oldstate->is_wifi_activated &&
        strncmp(state->password, oldstate->password, MAX_CP_PASSWORD_LEN) == 0 &&
        strncmp(state->newest_version, oldstate->newest_version, VERSION_NUMBER_LEN) == 0) {
        return;
    }

    File f = SPIFFS.open(STATE_FILENAME, "w");
    f.print(
            (String) state->battery_capacity + "\n" +
            (String) state->auto_calibration_on + "\n" +
            (String) state->calibration_value + "\n" +
            (String) state->is_wifi_activated + "\n" +
            (String) state->password + "\n" +
            (String) state->newest_version + "\n"
    );
    f.close();
    Serial.println("State file saved");
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

void initWiFi() {
    ESPAsync_WiFiManager ESPAsync_WiFiManager(&webServer, &dnsServer);

    Router_SSID = ESPAsync_WiFiManager.WiFi_SSID();
    Router_Pass = ESPAsync_WiFiManager.WiFi_Pass();

    Serial.print("Router_SSID: " + Router_SSID);
    Serial.println("; Router_Pass: " + Router_Pass);
    if (!areRouterCredentialsValid()) {
        Serial.println("Disconnect WiFi in setup()");
        state.is_wifi_activated = false;
        WiFi.disconnect(true, false);
    }
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

    if (airSensor.begin(Wire, state.auto_calibration_on) == false) {
        DisbuffValue.setTextColor(RED);
        Serial.println("Air sensor not detected. Please check wiring. Freezing...");
        DisbuffValue.drawString("Air sensor not detected.", 0, 0);
        DisbuffValue.drawString("Please check wiring.", 0, 25);
        DisbuffValue.drawString("Freezing.", 0, 50);
        DisbuffValue.pushSprite(0, 26);
        while (1) {
            delay(1000);
        }
    }

    airSensor.setTemperatureOffset(5.5);
    airSensor.setAltitudeCompensation(440);
}

void checkIntervals(struct state *state) {
#define WIFICHECK_INTERVAL 1000L
#define MQTT_PUBLISH_INTERVAL 60000L
#define MQTT_CHECK_CONNECTION 2000L

    static ulong checkwifi_timeout = 0;
    static ulong checkmqtt_timeout = 0;
    static ulong checkmqtt_connection_timeout = 0;
    static ulong current_millis;
    current_millis = millis();

    if ((current_millis > checkwifi_timeout) || (checkwifi_timeout == 0)) {
        checkWiFi();
        checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
    }

    if (current_millis > checkmqtt_connection_timeout || checkmqtt_connection_timeout == 0) {
        if (!mqtt.connected() && state->wifi_status == WL_CONNECTED &&
            (String) state->mqttServer != "" && (String) state->mqttPort != "") {
            setMQTTServer(state);
            MQTTConnect(state);
        } else if (mqtt.connected() && state->wifi_status != WL_CONNECTED)
            mqtt.disconnect();
    
        checkmqtt_connection_timeout = current_millis + MQTT_CHECK_CONNECTION; 
    }

    if (current_millis > checkmqtt_timeout || checkmqtt_timeout == 0) {
        if (state->wifi_status == WL_CONNECTED &&
            (String) state->mqttTopic != "" &&
            mqtt.connected()) 
            publishMQTT(state);

        checkmqtt_timeout = current_millis + MQTT_PUBLISH_INTERVAL;
    }
}

void checkWiFi() {
    if ((state.wifi_status != WL_CONNECTED && state.is_wifi_activated)) {
        Serial.println("WiFi lost. Trying to reconnect. Status: " + (String) state.wifi_status);
        if (connectMultiWiFi() != WL_CONNECTED) {
            Serial.println("WiFi turned off.");
            state.is_wifi_activated = false;
        }
    }
}

uint8_t connectMultiWiFi() {
#define WIFI_MULTI_1ST_CONNECT_WAITING_MS 0
#define WIFI_MULTI_CONNECT_WAITING_MS 50L
    uint8_t status;
    LOGERROR(F("ConnectMultiWiFi with :"));

    if (areRouterCredentialsValid()) {
        LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass);
    }

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
        if (String(WM_config.WiFi_Creds[i].wifi_ssid) != "" &&
            strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) {
            LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "),
                      WM_config.WiFi_Creds[i].wifi_pw);
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

    while ((i++ < 10) && (status != WL_CONNECTED)) {
        status = wifiMulti.run();

        if (status == WL_CONNECTED)
            break;
        else
            delay(WIFI_MULTI_CONNECT_WAITING_MS);
    }

    if (status == WL_CONNECTED) {
        LOGERROR1(F("WiFi connected after time: "), i);
        LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
        LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP());
        Serial.println("WiFi Connected SSID: " + (String) WiFi.SSID());
    } else {
        LOGERROR3(F("WiFi not connected."), " ", "Status: ", (String) status);
    }

    return status;
}

void configWiFi(WiFi_STA_IPConfig in_WM_STA_IPconfig) {
#if USE_CONFIGURABLE_DNS
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    WiFi.config(in_WM_STA_IPconfig._sta_static_ip, in_WM_STA_IPconfig._sta_static_gw, in_WM_STA_IPconfig._sta_static_sn,
                in_WM_STA_IPconfig._sta_static_dns1, in_WM_STA_IPconfig._sta_static_dns2);
#else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    WiFi.config(in_WM_STA_IPconfig._sta_static_ip, in_WM_STA_IPconfig._sta_static_gw,
                in_WM_STA_IPconfig._sta_static_sn);
#endif
}

void displayIPConfigStruct(WiFi_STA_IPConfig in_WM_STA_IPconfig) {
    LOGERROR3(F("stationIP ="), in_WM_STA_IPconfig._sta_static_ip, ", gatewayIP =", in_WM_STA_IPconfig._sta_static_gw);
    LOGERROR1(F("netMask ="), in_WM_STA_IPconfig._sta_static_sn);
#if USE_CONFIGURABLE_DNS
    LOGERROR3(F("dns1IP ="), in_WM_STA_IPconfig._sta_static_dns1, ", dns2IP =", in_WM_STA_IPconfig._sta_static_dns2);
#endif
}

void loadMQTTConfig() {
    File file = SPIFFS.open(MQTT_FILENAME, "r");

    if (!file) {
        Serial.println("failed to open mqtt file for reading");
        return;
    }

    size_t size = file.size();
    std::unique_ptr<char[]> buf(new char[size + 1]);
    file.readBytes(buf.get(), size);
    file.close();

    DynamicJsonDocument json(1024);
    DeserializationError error = deserializeJson(json, buf.get());

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    if (json.containsKey(MQTT_SERVER_Label))
        strncpy(state.mqttServer, json[MQTT_SERVER_Label], MQTT_SERVER_LEN);

    if (json.containsKey(MQTT_SERVERPORT_Label))
        strncpy(state.mqttPort, json[MQTT_SERVERPORT_Label], MQTT_PORT_LEN);

    if (json.containsKey(MQTT_TOPIC_Label))
        strncpy(state.mqttTopic, json[MQTT_TOPIC_Label], MQTT_TOPIC_LEN);

    if (json.containsKey(MQTT_DEVICENAME_Label))
        strncpy(state.mqttDevice, json[MQTT_DEVICENAME_Label], MQTT_DEVICENAME_LEN);

    if (json.containsKey(MQTT_USERNAME_Label))
        strncpy(state.mqttUser, json[MQTT_USERNAME_Label], MQTT_USERNAME_LEN);

    if (json.containsKey(MQTT_KEY_Label))
        strncpy(state.mqttPassword, json[MQTT_KEY_Label], MQTT_KEY_LEN);
}

void saveMQTTConfig(struct state *state) {
    DynamicJsonDocument json(1024);

    json[MQTT_SERVER_Label] = state->mqttServer;
    json[MQTT_SERVERPORT_Label] = state->mqttPort;
    json[MQTT_TOPIC_Label] = state->mqttTopic;
    json[MQTT_DEVICENAME_Label] = state->mqttDevice;
    json[MQTT_USERNAME_Label] = state->mqttUser;
    json[MQTT_KEY_Label] = state->mqttPassword;

    File file = SPIFFS.open(MQTT_FILENAME, "w");

    if (!file) {
        Serial.println("failed to open mqtt file to for writing");
        return;
    }

    serializeJson(json, file);
    file.close();
}

void loadConfigData() {
    File file = SPIFFS.open(CONFIG_FILENAME, "r");
    LOGERROR(F("LoadWiFiCfgFile "));
    memset(&WM_config, 0, sizeof(WM_config));
    memset(&WM_STA_IPconfig, 0, sizeof(WM_STA_IPconfig));

    if (file) {
        file.readBytes((char *) &WM_config, sizeof(WM_config));
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
        file.write((uint8_t * ) & WM_config, sizeof(WM_config));
        file.write((uint8_t * ) & WM_STA_IPconfig, sizeof(WM_STA_IPconfig));
        file.close();
        LOGERROR(F("OK"));
    } else {
        LOGERROR(F("failed"));
    }
}

void handleNavigation(struct state *state) {
    switch (state->menu_mode) {
        case menuModeGraphs:
            state->menu_mode = menuModeCalibrationSettings;
            break;

        case menuModeCalibrationSettings:
            state->menu_mode = menuModeWiFiSettings;
            break;

        case menuModeWiFiSettings:
            state->menu_mode = menuModeMQTTSettings;
            break;

        case menuModeMQTTSettings:
            state->menu_mode = menuModeTimeSettings;
            break;

        case menuModeTimeSettings:
            state->menu_mode = menuModeUpdateSettings;
            break;

        case menuModeUpdateSettings:
            state->menu_mode = menuModeGraphs;
            break;

        default:
            state->menu_mode = menuModeGraphs;
            break;
    }
}

void handleWiFi(struct state *oldstate, struct state *state) {
    if (state->is_wifi_activated && state->wifi_status == WL_CONNECT_FAILED)
        state->is_wifi_activated = false;

    if (!state->is_wifi_activated && state->wifi_status == WL_CONNECTED) {
        WiFi.disconnect(true, false);
    }

    if (state->is_wifi_activated && state->wifi_status != WL_CONNECTED) {
        ESPAsync_WiFiManager ESPAsync_WiFiManager(&webServer, &dnsServer);
        startWiFiManager(&ESPAsync_WiFiManager, oldstate, state);
    }

    if (state->is_requesting_reset) {
        ESPAsync_WiFiManager ESPAsync_WiFiManager(&webServer, &dnsServer);
        resetWiFiManager(&ESPAsync_WiFiManager, state);
    }
}

void startWiFiManager(ESPAsync_WiFiManager *ESPAsync_WiFiManager, struct state *oldstate, struct state *state) {
    unsigned long startedAt = millis();
    ESPAsync_WMParameter mqttServer(MQTT_SERVER_Label, "MQTT Server *", state->mqttServer, MQTT_SERVER_LEN - 1);
    ESPAsync_WMParameter mqttPort(MQTT_SERVERPORT_Label, "MQTT Serverport *", state->mqttPort, MQTT_PORT_LEN - 1);
    ESPAsync_WMParameter mqttTopic(MQTT_TOPIC_Label, "MQTT Topic *", state->mqttTopic, MQTT_TOPIC_LEN - 1);
    ESPAsync_WMParameter mqttDevice(MQTT_DEVICENAME_Label, "MQTT unique device name", state->mqttDevice, MQTT_DEVICENAME_LEN - 1);
    ESPAsync_WMParameter mqttUser(MQTT_USERNAME_Label, "MQTT Username", state->mqttUser, MQTT_USERNAME_LEN - 1);
    ESPAsync_WMParameter mqttPassword(MQTT_KEY_Label, "MQTT Password", state->mqttPassword, MQTT_KEY_LEN - 1);

    ESPAsync_WiFiManager->addParameter(&mqttServer);
    ESPAsync_WiFiManager->addParameter(&mqttPort);
    ESPAsync_WiFiManager->addParameter(&mqttTopic);
    ESPAsync_WiFiManager->addParameter(&mqttDevice);
    ESPAsync_WiFiManager->addParameter(&mqttUser);
    ESPAsync_WiFiManager->addParameter(&mqttPassword);

    setupWiFiManager(ESPAsync_WiFiManager);

    Router_SSID = ESPAsync_WiFiManager->WiFi_SSID();
    Router_Pass = ESPAsync_WiFiManager->WiFi_Pass();

    if (areRouterCredentialsValid()) {
        LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
        wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
        ESPAsync_WiFiManager->setConfigPortalTimeout(0);
        Serial.println("Got Stored Credentials: " + Router_SSID);

        loadConfigData();

        for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
            if ((String(WM_config.WiFi_Creds[i].wifi_ssid) != "") &&
                (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE)) {
                LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "),
                          WM_config.WiFi_Creds[i].wifi_pw);
                wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
            }
        }

        if (state->wifi_status != WL_CONNECTED && state->is_wifi_activated)
            connectMultiWiFi();
    } else {
        Serial.println("No Credentials.");
        Serial.println("Start Configuration Portal.");

        state->wifi_info = infoConfigPortalCredentials;
        drawScreen(oldstate, state);

        ESPAsync_WiFiManager->setSaveConfigCallback(configPortalCallback);
        if (!ESPAsync_WiFiManager->startConfigPortal((const char *) ssid.c_str(), (const char *) state->password)) {
            Serial.println("Not connected to WiFi but continuing anyway.");
        } else {
            Serial.println("WiFi connected :D");
            Serial.println("WiFi Status: " + (String) state->wifi_status);
            Serial.println("WiFi activated: " + (String) state->is_wifi_activated);
        }

        saveConfigPortalCredentials(ESPAsync_WiFiManager);

        strncpy(state->mqttServer, mqttServer.getValue(), MQTT_SERVER_LEN);
        strncpy(state->mqttPort, mqttPort.getValue(), MQTT_PORT_LEN);
        strncpy(state->mqttTopic, mqttTopic.getValue(), MQTT_TOPIC_LEN);
        strncpy(state->mqttDevice, mqttDevice.getValue(), MQTT_DEVICENAME_LEN);
        strncpy(state->mqttUser, mqttUser.getValue(), MQTT_USERNAME_LEN);
        strncpy(state->mqttPassword, mqttPassword.getValue(), MQTT_KEY_LEN);

        saveMQTTConfig(state);
    }

    Serial.print("After waiting ");
    Serial.print((float) (millis() - startedAt) / 1000L);
    Serial.print(" secs more in loop(), connection result is ");

    if (state->is_wifi_activated && state->wifi_status == WL_CONNECTED) {
        Serial.print("connected. Local IP: ");
        Serial.println(WiFi.localIP());
    } else
        Serial.println(ESPAsync_WiFiManager->getStatus(state->wifi_status));
}

void resetWiFiManager(ESPAsync_WiFiManager *ESPAsync_WiFiManager, struct state *state) {
    Router_SSID = "";
    Router_Pass = "";
    ESPAsync_WiFiManager->resetSettings();

    mqtt.disconnect();
    WiFi.disconnect(true, true);
    state->wifi_status = WL_DISCONNECTED;

    ESPAsync_WMParameter mqttServer(MQTT_SERVER_Label, "MQTT Server *", state->mqttServer, MQTT_SERVER_LEN - 1);
    ESPAsync_WMParameter mqttPort(MQTT_SERVERPORT_Label, "MQTT Serverport *", state->mqttPort, MQTT_PORT_LEN - 1);
    ESPAsync_WMParameter mqttTopic(MQTT_TOPIC_Label, "MQTT Topic *", state->mqttTopic, MQTT_TOPIC_LEN - 1);
    ESPAsync_WMParameter mqttDevice(MQTT_DEVICENAME_Label, "MQTT unique device name", state->mqttDevice, MQTT_DEVICENAME_LEN - 1);
    ESPAsync_WMParameter mqttUser(MQTT_USERNAME_Label, "MQTT Username", state->mqttUser, MQTT_USERNAME_LEN - 1);
    ESPAsync_WMParameter mqttPassword(MQTT_KEY_Label, "MQTT Password", state->mqttPassword, MQTT_KEY_LEN - 1);

    ESPAsync_WiFiManager->addParameter(&mqttServer);
    ESPAsync_WiFiManager->addParameter(&mqttPort);
    ESPAsync_WiFiManager->addParameter(&mqttTopic);
    ESPAsync_WiFiManager->addParameter(&mqttDevice);
    ESPAsync_WiFiManager->addParameter(&mqttUser);
    ESPAsync_WiFiManager->addParameter(&mqttPassword);

    setupWiFiManager(ESPAsync_WiFiManager);
    Serial.println("Start Configuration Portal for reset.");
    ESPAsync_WiFiManager->setSaveConfigCallback(configPortalCallback);
    ESPAsync_WiFiManager->startConfigPortal((const char *) ssid.c_str(), (const char *) state->password);
    saveConfigPortalCredentials(ESPAsync_WiFiManager);

    strncpy(state->mqttServer, mqttServer.getValue(), MQTT_SERVER_LEN);
    strncpy(state->mqttPort, mqttPort.getValue(), MQTT_PORT_LEN);
    strncpy(state->mqttTopic, mqttTopic.getValue(), MQTT_TOPIC_LEN);
    strncpy(state->mqttDevice, mqttDevice.getValue(), MQTT_DEVICENAME_LEN);
    strncpy(state->mqttUser, mqttUser.getValue(), MQTT_USERNAME_LEN);
    strncpy(state->mqttPassword, mqttPassword.getValue(), MQTT_KEY_LEN);

    saveMQTTConfig(state);
}

// This gets called when custom parameters have been set 
// AND a connection has been established
void configPortalCallback() {
    state.is_requesting_reset = false;
}

void saveConfigPortalCredentials(ESPAsync_WiFiManager *ESPAsync_WiFiManager) {
    if (String(ESPAsync_WiFiManager->getSSID(0)) != "" && String(ESPAsync_WiFiManager->getSSID(1)) != "") {
        memset(&WM_config, 0, sizeof(WM_config));

        for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
            String tempSSID = ESPAsync_WiFiManager->getSSID(i);
            String tempPW = ESPAsync_WiFiManager->getPW(i);

            if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
                strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
            else
                strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(),
                        sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

            if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
                strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
            else
                strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);

            if (String(WM_config.WiFi_Creds[i].wifi_ssid) != "" &&
                String(WM_config.WiFi_Creds[i].wifi_ssid) != "0" &&
                strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) {
                LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "),
                          WM_config.WiFi_Creds[i].wifi_pw);
                wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
            }
        }

        saveConfigData();
    }
}

bool areRouterCredentialsValid() {
    if (Router_SSID == "" || Router_SSID == "0") {
        Serial.println("invalid creds: " + Router_SSID);
        return false;
    } else {
        Serial.println("valid creds: " + Router_SSID);
        return true;
    }
}

void setupWiFiManager(ESPAsync_WiFiManager *ESPAsync_WiFiManager) {
    ESPAsync_WiFiManager->setDebugOutput(true);
    ESPAsync_WiFiManager->setAPStaticIPConfig(WM_AP_IPconfig);
    ESPAsync_WiFiManager->setMinimumSignalQuality(-1);
    ESPAsync_WiFiManager->setConfigPortalChannel(0);

#if !USE_DHCP_IP
#if USE_CONFIGURABLE_DNS
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    ESPAsync_wifiManager->setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);
#else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    ESPAsync_wifiManager->setSTAStaticIPConfig(stationIP, gatewayIP, netMask);
#endif
#endif

    if (areRouterCredentialsValid()) {
        Serial.println("Got stored Credentials: " + Router_SSID);
        ESPAsync_WiFiManager->setConfigPortalTimeout(0);
    } else {
        Serial.println("No stored Credentials.");
    }

    ssid.toUpperCase();
}

void publishMQTT(struct state *state) {
    String co2Topic = (String) state->mqttTopic + (String) TOPIC_CO2;
    String humidityTopic = (String) state->mqttTopic + (String) TOPIC_HUMIDITY;
    String temperatureTopic = (String) state->mqttTopic + (String) TOPIC_TEMPERATURE;

    String co2 = (String) state->co2_ppm;
    String humidity = (String) ((double) state->humidity_percent / 10);
    String temperature = (String) ((double) state->temperature_celsius / 10);

    if (mqtt.publish((const char *) co2Topic.c_str(), (const char *) co2.c_str()))
        Serial.println("Published co2 to MQTT");

    if (mqtt.publish((const char *) humidityTopic.c_str(), (const char *) humidity.c_str()))
        Serial.println("Published humidity to MQTT");

    if (mqtt.publish((const char *) temperatureTopic.c_str(), (const char *) temperature.c_str()))
        Serial.println("Published temperature to MQTT");
}

void setMQTTServer(struct state *state) {
    IPAddress serverIP;
    if (serverIP.fromString((String) state->mqttServer)) {
        int ip[4];
        sscanf(state->mqttServer, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
        serverIP = IPAddress(ip[0], ip[1], ip[2], ip[3]);
        Serial.print("Setting ip Server: ");
        Serial.println((String) state->mqttServer + ", " + (String) state->mqttPort);
        mqtt.setServer(serverIP, atoi(state->mqttPort));
    } else {
        Serial.print("Setting char Server: ");
        Serial.println((String) state->mqttServer + ", " + (String) state->mqttPort);
        mqtt.setServer((const char *) state->mqttServer, atoi(state->mqttPort));
    }
}

bool MQTTConnect(struct state *state) {
    if (mqtt.connected())
        return true;

    char clientID[MQTT_DEVICENAME_LEN];
    strncpy(clientID, (String) state->mqttDevice == "" ? ssid.c_str() : state->mqttDevice, MQTT_DEVICENAME_LEN);

    if ((String) state->mqttUser != "" && (String) state->mqttPassword != "") {
        if (mqtt.connect(clientID, state->mqttUser, state->mqttPassword)) {
            Serial.println(F("MQTT connection successful!"));
            return true;
        }
    } else {
        if (mqtt.connect(clientID)) {
            Serial.println(F("MQTT connection successful!"));
            return true;
        }
    }

    Serial.print("MQTT connection failed: ");
    Serial.println(mqtt.state());

    Serial.println("Could not connect to MQTT");
    return false;
}

void handleFirmware(struct state *oldstate, struct state *state) {
    if (state->is_requesting_update == oldstate->is_requesting_update)
        return;

    if (state->is_requesting_update && state->wifi_status == WL_CONNECTED) {
        String firmwareFile = "http://" + (String) FIRMWARE_SERVER + (String) REMOTE_FIRMWARE_FILE;
        HTTPClient http;
        http.begin(firmwareFile);
        int httpCode = http.GET();

        if (httpCode <= 0) {
            Serial.print("Fetching firmware failed, error: ");
            Serial.println(http.errorToString(httpCode));
            return;
        }

        int contentLen = http.getSize();
        if (!Update.begin(contentLen)) {
            Serial.println("Not enough space to update firmware");
            return;
        }

        WiFiClient *client = http.getStreamPtr();
        size_t written = Update.writeStream(*client);

        if (written == contentLen) {
            Serial.println("Written " + (String) written + " successfully");
        } else {
            Serial.println("Written only " + (String) written + "/" + (String) contentLen + ". Canceling.");
            return;
        }

        if (Update.end()) {
            if (Update.isFinished()) {
                Serial.println("Update was successful. Rebooting.");
                ESP.restart();
            } else
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
        } else
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));

        http.end();
    }
}

bool fetchRemoteVersion(struct state *state) {
    if (state->wifi_status == WL_CONNECTED) {
        HTTPClient http;
        String versionFile = "http://" + (String) FIRMWARE_SERVER + (String) REMOTE_VERSION_FILE;
        http.begin(versionFile);
        int httpCode = http.GET();

        if (httpCode <= 0) {
            Serial.print("Fetching version failed, error: ");
            Serial.println(http.errorToString(httpCode));
            return false;
        }

        WiFiClient client = http.getStream();

        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, client);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.f_str());
            return false;
        }

        strncpy(state->newest_version, doc["version"].as<char *>(), VERSION_NUMBER_LEN);
        http.end();
        return true;
    }
    return false;
}

void updateTouch(struct state *state) {
    switch(state->menu_mode) {
        case menuModeGraphs: 
            if (batteryButton.wasPressed())
                state->graph_mode = graphModeBatteryMah;
            if (co2Button.wasPressed())
                state->graph_mode = graphModeCo2;
            if (midLeftButton.wasPressed())
                state->graph_mode = graphModeTemperature;
            if (midRightButton.wasPressed())
                state->graph_mode = graphModeHumidity;
            break;

        case menuModeCalibrationSettings:
            if (midLeftButton.wasPressed())
                state->calibration_value -= state->calibration_value >= 410 ? 10 : 0;
            if (midRightButton.wasPressed())
                state->calibration_value += state->calibration_value <= 1990 ? 10 : 0;
            if (toggleAutoCalButton.wasPressed()) {
                state->auto_calibration_on = !state->auto_calibration_on;
                airSensor.setAutoSelfCalibration(state->auto_calibration_on);
            }
            if (submitCalibrationButton.wasPressed())
                state->menu_mode = menuModeCalibrationAlert;
            break;

        case menuModeCalibrationAlert: 
            if (submitCalibrationButton.wasPressed()) {
                airSensor.setForcedRecalibrationFactor(state->calibration_value);
                state->menu_mode = menuModeCalibrationSettings;
                state->cal_info = infoCalSuccess;
            }
            if (toggleAutoCalButton.wasPressed())
                state->menu_mode = menuModeCalibrationSettings;
            break;

        case menuModeWiFiSettings:
            if (toggleWiFiButton.wasPressed())
                state->is_wifi_activated = !state->is_wifi_activated;
            if (resetWiFiButton.wasPressed()) {
                state->is_requesting_reset = true;
            }
            break;

        case menuModeTimeSettings:
            if (syncTimeButton.wasPressed())
                state->force_sync = true;
            break;

        case menuModeUpdateSettings:
            if (syncTimeButton.wasPressed())
                state->is_requesting_update = true;
            break;
        
        default: 
            break;
    }
 
    if (M5.BtnA.wasPressed()) {
        setDisplayPower(state->display_sleep);
        state->display_sleep = !state->display_sleep;
    }

    if (M5.BtnC.wasPressed()) {
        handleNavigation(state);
    }
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
    float batVoltage = M5.Axp.GetBatVoltage();
    state->battery_current = M5.Axp.GetBatCurrent();

    if (batVoltage < 3.2 && columbDischarged > columbCharged) {
        M5.Axp.ClearCoulombcounter();
        M5.Axp.EnableCoulombcounter();
    }

    state->battery_voltage = batVoltage;
    state->battery_mah = 65536 * 0.5 * (columbCharged - columbDischarged) / 3600.0 / 25.0;

    if (state->in_ac && abs(state->battery_current) < 0.1 && state->battery_voltage >= 4.15 &&
        abs(state->battery_mah - state->battery_capacity) > 1) {
        Serial.println("maximum found " + String(state->battery_mah));
        state->battery_capacity = state->battery_mah;
    }

    int batteryPercent = state->battery_mah * 100 / state->battery_capacity;
    batteryPercent = max(min(100, batteryPercent), 0);

    /*Serial.println(
      "mAh: " + String(state->battery_mah) +
      " charged: " + String(columbCharged) +
      " discharched: " + String(columbDischarged) +
      " batteryPercent " + String(batteryPercent) +
      " current " + state->battery_current  +
      " voltage " + String(state->battery_voltage)
    ); */
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
    if (((cycle + 2) % (2 * target_fps)) != 0) {
        return;
    }

    if (airSensor.dataAvailable()) {
        state->co2_ppm = airSensor.getCO2();
        state->temperature_celsius = airSensor.getTemperature() * 10;
        state->humidity_percent = airSensor.getHumidity() * 10;
    }
}

void updatePassword(struct state *state) {
    if ((String) state->password == "") {
        String password = randomPassword(8);
        strncpy(state->password, password.c_str(), MAX_CP_PASSWORD_LEN);
        Serial.println(state->password);
    }
}

void updateWiFiState(struct state *oldstate, struct state *state) {
    if (WiFi.status() == oldstate->wifi_status &&
        state->is_requesting_reset == oldstate->is_requesting_reset)
        return;

    Serial.println("updating wifi status ...");
    state->wifi_status = WiFi.status();

    if (state->wifi_status == WL_CONNECTED)
        state->wifi_info = infoWiFiConnected;
    else if (state->wifi_status == WL_CONNECT_FAILED)
        state->wifi_info = infoWiFiFailed;
    else if (state->wifi_status == WL_CONNECTION_LOST)
        state->wifi_info = infoWiFiLost;
    else
        state->wifi_info = infoEmpty;

    if (state->is_requesting_reset)
        state->wifi_info = infoConfigPortalCredentials;
    else if (state->menu_mode == menuModeWiFiSettings)
        drawWiFiSettings(oldstate, state);

    Serial.println("WiFi Status: " + (String) state->wifi_status);
}

void updateTimeState(struct state *oldstate, struct state *state) {
    if (state->current_time.tm_min == oldstate->current_time.tm_min)
        return;

    if (mktime(&state->current_time) > mktime(&state->next_time_sync))
        state->is_sync_needed = true;
}

void updateMQTT(struct state *state) {
    if (mqtt.loop())
        state->is_mqtt_connected = true;
    else {
        mqtt.disconnect();
        state->is_mqtt_connected = false;
    }
}

void createSprites() {
    DisbuffHeader.createSprite(320, 26);
    DisbuffHeader.setFreeFont(&FreeMono9pt7b);
    DisbuffHeader.setTextSize(1);
    DisbuffHeader.setTextColor(WHITE);
    DisbuffHeader.setTextDatum(TL_DATUM);
    DisbuffHeader.fillRect(0, 0, 320, 25, BLACK);
    DisbuffHeader.drawLine(0, 25, 320, 25, WHITE);

    DisbuffValue.createSprite(320, 117);
    DisbuffValue.setTextSize(1);
    DisbuffValue.setTextColor(WHITE);
    DisbuffValue.setTextDatum(TC_DATUM);
    DisbuffValue.fillRect(0, 0, 320, 116, BLACK);

    DisbuffGraph.createSprite(320, 97);
    DisbuffGraph.setFreeFont(&FreeMono9pt7b);
    DisbuffGraph.setTextSize(1);
    DisbuffGraph.setTextColor(WHITE);
    DisbuffGraph.setTextDatum(TC_DATUM);
    DisbuffGraph.fillRect(0, 0, 320, 97, BLACK);

    DisbuffBody.createSprite(320, 214);
}

uint16_t co2color(int value) {
    if (value < 600) {
        return CYAN;
    } else if (value < 800) {
        return GREEN;
    } else if (value < 1000) {
        return YELLOW;
    } else if (value < 1400) {
        return ORANGE;
    } else {
        return RED;
    }
}

void drawScreen(struct state *oldstate, struct state *state) {
    if (!state->display_sleep) {
        drawHeader(oldstate, state);

        if (state->menu_mode == menuModeGraphs) {
            clearScreen(oldstate, state);
            drawValues(oldstate, state);
            drawGraph(oldstate, state);
        } else if (state->menu_mode == menuModeCalibrationSettings) {
            clearScreen(oldstate, state);
            drawCalibrationSettings(oldstate, state);
        } else if (state->menu_mode == menuModeCalibrationAlert) {
            clearScreen(oldstate, state);
            drawCalibrationAlert(oldstate, state);
        } else if (state->menu_mode == menuModeWiFiSettings) {
            state->cal_info = infoEmpty;
            clearScreen(oldstate, state);
            drawWiFiSettings(oldstate, state);
        } else if (state->menu_mode == menuModeMQTTSettings) {
            clearScreen(oldstate, state);
            drawMQTTSettings(oldstate, state);
        } else if (state->menu_mode == menuModeTimeSettings) {
            clearScreen(oldstate, state);
            drawSyncSettings(oldstate, state);
        } else if (state->menu_mode == menuModeUpdateSettings) {
            state->time_info = infoEmpty;
            clearScreen(oldstate, state);
            drawUpdateSettings(oldstate, state);
        }
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

    DisbuffHeader.fillRect(0, 0, 320, 24, BLACK);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf) - 1, "%c", &(state->current_time));
    DisbuffHeader.drawString(String(strftime_buf), 0, 1);
    DisbuffHeader.setTextDatum(TR_DATUM);
    DisbuffHeader.drawString(String(state->battery_percent) + "%" + (state->in_ac ? "+" : "-"), 320, 0);
    DisbuffHeader.setTextDatum(TL_DATUM);
    DisbuffHeader.drawLine(0, 25, 320, 25, WHITE);
    DisbuffHeader.pushSprite(0, 0);
}

void drawValues(struct state *oldstate, struct state *state) {
    if (state->temperature_celsius == oldstate->temperature_celsius &&
        state->humidity_percent == oldstate->humidity_percent &&
        state->co2_ppm == oldstate->co2_ppm &&
        state->display_sleep == oldstate->display_sleep &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffValue.fillRect(0, 0, 320, 116, BLACK);
    DisbuffValue.setFreeFont(&FreeMonoBold18pt7b);
    DisbuffValue.setTextColor(co2color(state->co2_ppm));

    DisbuffValue.setTextSize(2);
    DisbuffValue.drawString(String(state->co2_ppm) + "ppm", 160, 10);

    DisbuffValue.pushSprite(0, 26);

    String temperature = String(state->temperature_celsius / 10.0, 1) + "C";
    String humidity = String(state->humidity_percent / 10.0, 1) + "%";

    midLeftButton.setLabel(temperature.c_str());
    midLeftButton.draw();
    midRightButton.setLabel(humidity.c_str());
    midRightButton.draw();
    batteryButton.draw();
    co2Button.draw();
}

void drawGraph(struct state *oldstate, struct state *state) {
    if (state->graph_mode == oldstate->graph_mode &&
        state->graph_index == oldstate->graph_index &&
        state->display_sleep == oldstate->display_sleep &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffGraph.fillRect(0, 0, 320, 97, BLACK);

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

    int skip = GRAPH_UNITS * 2.5 / 100;
    float min_value = sorted[value_count > 10 * skip ? skip : 0];
    float max_value = sorted[value_count > 10 * skip ? value_count - 1 - skip : value_count - 1];
    int last_index = ((state->graph_index - 1) % GRAPH_UNITS + GRAPH_UNITS) % GRAPH_UNITS;

    min_value = min(min(min_value, values[last_index]), max_value);
    max_value = max(max(max_value, values[last_index]), min_value);

    float factor = 96 / (max_value - min_value);
    DisbuffGraph.drawString(String(max_value, 1), 0, 2);
    DisbuffGraph.drawString(String(min_value, 1), 0, 70);

    for (i = 0; i < GRAPH_UNITS; i++) {
        float value = values[i];
        if (!isnan(value)) {
            int y = min(max(96 - int(factor * (value - min_value)), 0), 96);
            int x = 320 - (((state->graph_index - i) % GRAPH_UNITS + GRAPH_UNITS) % GRAPH_UNITS);
            uint16_t color = state->graph_mode == graphModeCo2 ? co2color(value) : WHITE;
            for (int j = y; j < 96; j++) {
                DisbuffGraph.drawPixel(x, j, color);
            }
        }
    }

    //Serial.println("graph done");
    DisbuffGraph.pushSprite(0, 144);
}

void drawCalibrationSettings(struct state *oldstate, struct state *state) {
    if (state->display_sleep == oldstate->display_sleep &&
        state->calibration_value == oldstate->calibration_value &&
        state->auto_calibration_on == oldstate->auto_calibration_on &&
        state->cal_info == oldstate->cal_info &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffBody.fillRect(0, 0, 320, 214, BLACK);

    DisbuffBody.setFreeFont(&FreeMono18pt7b);
    DisbuffBody.setTextColor(WHITE);
    DisbuffBody.drawString("Calibration: ", 45, 5);

    DisbuffBody.setFreeFont(&FreeMonoBold12pt7b);
    DisbuffBody.setTextColor(co2color(state->calibration_value));
    DisbuffBody.setTextSize(2);
    DisbuffBody.drawString(String(state->calibration_value) + "ppm", 80, 30);
    DisbuffBody.setTextSize(1);

    DisbuffBody.setFreeFont(&FreeMono9pt7b);
    DisbuffBody.setTextColor(WHITE);

    if (state->cal_info == infoCalSuccess) {
        DisbuffBody.setTextColor(GREEN);
        DisbuffBody.drawString("Calibration Successful", 35, 130);
    }

    DisbuffBody.pushSprite(0, 26);

    midLeftButton.setLabel("-");
    midLeftButton.setFont(&FreeMonoBold12pt7b);
    midLeftButton.draw();
    midRightButton.setLabel("+");
    midRightButton.setFont(&FreeMonoBold12pt7b);
    midRightButton.draw();

    toggleAutoCalButton.off = state->auto_calibration_on ? offGreen : offRed;
    toggleAutoCalButton.on = state->auto_calibration_on ? onGreen : offGreen;
    String toggleAutoCalLabel = state->auto_calibration_on ? "Auto Cal: ON" : "Auto Cal: OFF";
    toggleAutoCalButton.setLabel(toggleAutoCalLabel.c_str());
    toggleAutoCalButton.draw();

    if (!state->auto_calibration_on) {
        submitCalibrationButton.off = offCyan;
        submitCalibrationButton.on = onCyan;
        submitCalibrationButton.setLabel("Calibrate");
        submitCalibrationButton.draw();
    }
}

void drawCalibrationAlert(struct state *oldstate, struct state *state) {
    if (state->display_sleep == oldstate->display_sleep &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffBody.fillRect(0, 0, 320, 214, BLACK);

    DisbuffBody.setFreeFont(&FreeMono18pt7b);
    DisbuffBody.setTextColor(WHITE);
    DisbuffBody.drawString("Attention! ", 65, 20);

    DisbuffBody.setFreeFont(&FreeMono12pt7b);
    String ppm = String(state->calibration_value) + "ppm";
    String info0 = "Change Calibration";
    String info1 = "to " + ppm + " ?";
    String info2 = "This can't be undone.";
    DisbuffBody.drawString(info0, 30, 55);
    DisbuffBody.drawString(info1, 85, 80);
    DisbuffBody.drawString(info2, 20, 105);

    DisbuffBody.pushSprite(0, 26);

    toggleAutoCalButton.setLabel("NO");
    toggleAutoCalButton.draw();
    submitCalibrationButton.off = offGreen;
    submitCalibrationButton.on = onGreen;
    submitCalibrationButton.setLabel("YES");
    submitCalibrationButton.draw();
}

void drawWiFiSettings(struct state *oldstate, struct state *state) {
    if (state->display_sleep == oldstate->display_sleep &&
        state->is_wifi_activated == oldstate->is_wifi_activated &&
        state->is_requesting_reset == oldstate->is_requesting_reset &&
        state->wifi_status == oldstate->wifi_status &&
        state->wifi_info == oldstate->wifi_info &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffBody.fillRect(0, 0, 320, 214, BLACK);

    DisbuffBody.setFreeFont(&FreeMonoBold18pt7b);
    DisbuffBody.setTextColor(WHITE);
    DisbuffBody.setTextSize(2);
    DisbuffBody.drawString("WiFi", 75, 10);

    DisbuffBody.setFreeFont(&FreeMono9pt7b);
    DisbuffBody.setTextSize(1);

    if (state->wifi_info == infoConfigPortalCredentials) {
        String wifiInfo = "Join " + ssid;
        String ipInfo = "Open http://" +
                        (String) APStaticIP[0] + "." +
                        (String) APStaticIP[1] + "." +
                        (String) APStaticIP[2] + "." +
                        (String) APStaticIP[3];
        String pwInfo = "Password: " + (String) state->password;
        DisbuffBody.drawString(wifiInfo, 25, 75);
        DisbuffBody.drawString(pwInfo, 40, 95);
        DisbuffBody.drawString(ipInfo, 20, 115);
    } else if (state->wifi_info == infoWiFiConnected) {
        String connectedTo = (String) WiFi.SSID();
        String connectedInfo =
                "Connected: " + ((connectedTo.length() > 15) ? (connectedTo.substring(0, 14) + "...") : connectedTo);
        DisbuffBody.drawString(connectedInfo, 40, 90);
    } else if (state->wifi_info == infoWiFiLost) {
        DisbuffBody.setTextColor(YELLOW);
        DisbuffBody.drawString("Connection lost", 40, 90);
    } else if (state->wifi_info == infoWiFiFailed) {
        DisbuffBody.setTextColor(RED);
        DisbuffBody.drawString("Connection failed", 60, 90);
    }

    DisbuffBody.pushSprite(0, 26);

    toggleWiFiButton.off = state->is_wifi_activated ? offGreen : offRed;
    toggleWiFiButton.on = state->is_wifi_activated ? onGreen : onRed;
    toggleWiFiButton.setLabel(state->is_wifi_activated ? "ON" : "OFF");
    toggleWiFiButton.draw();

    if (state->is_wifi_activated)
        resetWiFiButton.draw();
}

void drawMQTTSettings(struct state *oldstate, struct state *state) {
    if (state->display_sleep == oldstate->display_sleep &&
        state->is_mqtt_connected == oldstate->is_mqtt_connected &&
        state->mqttServer == oldstate->mqttServer &&
        state->mqttPort == oldstate->mqttPort &&
        state->mqttDevice == oldstate->mqttDevice &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffBody.fillRect(0, 0, 320, 214, BLACK);

    DisbuffBody.setFreeFont(&FreeMonoBold18pt7b);
    DisbuffBody.setTextColor(WHITE);
    DisbuffBody.setTextSize(2);
    DisbuffBody.drawString("MQTT", 75, 10);

    DisbuffBody.setFreeFont(&FreeMono9pt7b);
    DisbuffBody.setTextSize(1);

    if (state->is_mqtt_connected) {
        DisbuffBody.setTextColor(GREEN);
        DisbuffBody.drawString("Connected", 100, 80);
        DisbuffBody.setTextColor(WHITE);
    } else {
        DisbuffBody.setTextColor(RED);
        DisbuffBody.drawString("Not Connected", 85, 80);
        DisbuffBody.setTextColor(WHITE);
    }

    String server = ((String) state->mqttServer == "" ? "Not Configured" : (String) state->mqttServer);
    String port = (String) state->mqttPort == "" ? "Not Configured" : (String) state->mqttPort;
    String user = (String) state->mqttDevice == "" ? ssid : (String) state->mqttDevice;

    String info0 = "Server: " + (server.length() > 18 ? (server.substring(0, 14) + "...") : server);
    String info1 = "Port  : " + port;
    String info2 = "Device: " + (user.length() > 18 ? (user.substring(0, 14) + "...") : user);

    DisbuffBody.drawString(info0, 15, 100);
    DisbuffBody.drawString(info1, 15, 115);
    DisbuffBody.drawString(info2, 15, 130);

    DisbuffBody.pushSprite(0, 26);
}

void drawSyncSettings(struct state *oldstate, struct state *state) {
    if (state->display_sleep == oldstate->display_sleep &&
        state->wifi_status == oldstate->wifi_status &&
        state->time_info == oldstate->time_info &&
        state->force_sync == oldstate->force_sync &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffBody.fillRect(0, 0, 320, 214, BLACK);

    DisbuffBody.setFreeFont(&FreeMonoBold18pt7b);
    DisbuffBody.setTextColor(WHITE);
    DisbuffBody.setTextSize(2);
    DisbuffBody.drawString("Sync", 75, 10);

    DisbuffBody.setFreeFont(&FreeMono9pt7b);
    DisbuffBody.setTextSize(1);

    DisbuffBody.drawString("Sync time & firmware", 40, 80);
    DisbuffBody.drawString("with online Servers.", 40, 95);

    if (state->time_info == infoTimeSyncSuccess) {
        DisbuffBody.setTextColor(GREEN);
        DisbuffBody.drawString("Sync successful", 75, 115);
    } else if (state->time_info == infoTimeSyncFailed) {
        DisbuffBody.setTextColor(RED);
        DisbuffBody.drawString("Sync Failed", 80, 115);
    }

    if (state->wifi_status != WL_CONNECTED) {
        DisbuffBody.setTextColor(RED);
        DisbuffBody.drawString("Can not synchronize", 45, 150);
        DisbuffBody.drawString("WiFi is not connected", 40, 170);
    }

    DisbuffBody.pushSprite(0, 26);

    // always push Disbuff before drawing buttons, otherwise button is not visible
    if (state->wifi_status == WL_CONNECTED) {
        syncTimeButton.setLabel("Synchronize");
        syncTimeButton.draw();
    }
}

void drawUpdateSettings(struct state *oldstate, struct state *state) {
    if (state->display_sleep == oldstate->display_sleep &&
        state->wifi_status == oldstate->wifi_status &&
        strncmp(state->newest_version, oldstate->newest_version, VERSION_NUMBER_LEN) == 0 &&
        state->menu_mode == oldstate->menu_mode)
        return;

    DisbuffBody.fillRect(0, 0, 320, 214, BLACK);

    DisbuffBody.setFreeFont(&FreeMonoBold18pt7b);
    DisbuffBody.setTextColor(WHITE);
    DisbuffBody.setTextSize(2);
    DisbuffBody.drawString("Updates", 15, 10);

    DisbuffBody.setFreeFont(&FreeMono9pt7b);
    DisbuffBody.setTextSize(1);

    String info0 = "Version: " + (String) VERSION_NUMBER;
    String info1 = "Newest: " + ((String) state->newest_version == "" ? "N/A" : (String) state->newest_version);
    DisbuffBody.drawString(info0, 80, 80);
    DisbuffBody.drawString(info1, 85, 100);

    if (state->update_info == infoUpdateFailed) {
        DisbuffBody.setTextColor(RED);
        DisbuffBody.drawString("Update failed!", 40, 120);
    }

    if (state->wifi_status != WL_CONNECTED) {
        DisbuffBody.setTextColor(RED);
        DisbuffBody.drawString("Can not update device", 35, 150);
        DisbuffBody.drawString("WiFi is not connected", 40, 170);
    }

    DisbuffBody.pushSprite(0, 26);

    // always push Disbuff before drawing buttons, otherwise button is not visible
    if (needFirmwareUpdate(VERSION_NUMBER, (const char *) state->newest_version)) {
        syncTimeButton.setLabel("Update Firmware");
        syncTimeButton.draw();
    }
}

void hideButtons() {
    for (Button *button : Button::instances) {
        if (button->getName() == M5.BtnA.getName() ||
            button->getName() == M5.BtnB.getName() ||
            button->getName() == M5.BtnC.getName() ||
            button->getName() == M5.background.getName())
            continue;

        button->hide();
    }
}

void clearScreen(struct state *oldstate, struct state *state) {
    if (oldstate->menu_mode != state->menu_mode) {
        hideButtons();
        DisbuffBody.fillRect(0, 0, 320, 214, BLACK);
        DisbuffBody.pushSprite(0, 26);
    }
}

bool needFirmwareUpdate(const char *deviceVersion, const char *remoteVersion) {
    if (!remoteVersion || remoteVersion[0] == '\0')
        return false;

    int device[3], remote[3];
    sscanf(deviceVersion, "%d.%d.%d", &device[0], &device[1], &device[2]);
    sscanf(remoteVersion, "%d.%d.%d", &remote[0], &remote[1], &remote[2]);

    for (int i = 0; i < 3; i++) {
        if (remote[i] > device[i])
            return true;
    }

    return false;
}

void writeSsd(struct state *state) {
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
    //Serial.println(dataMessage);
    appendFile(SD, "/data.txt", dataMessage.c_str());
}

String padTwo(String input) {
    if (input.length() == 2) {
        return input;
    }
    return "0" + input;
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char *path, const char *message) {
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

void syncData(struct state *state) {
    if (state->wifi_status != WL_CONNECTED)
        return;

    if (state->force_sync) {
        if (fetchRemoteVersion(state) && setRtc(state))
            state->time_info = infoTimeSyncSuccess;
        else 
            state->time_info = infoTimeSyncFailed;
        state->force_sync = false;
    } else if (state->is_sync_needed) {
         if (fetchRemoteVersion(state) && setRtc(state)) {
            state->time_info = infoTimeSyncSuccess;
            getLocalTime(&(state->current_time));
            state->next_time_sync = state->current_time;
            state->next_time_sync.tm_mday++;
            state->next_time_sync.tm_hour = TIME_SYNC_HOUR;
            state->next_time_sync.tm_min = TIME_SYNC_MIN;
         } else 
            state->time_info = infoTimeSyncFailed;
        
        state->is_sync_needed = false;
    }
}

bool setRtc(struct state *state) {
    configTime(0, 0, "pool.ntp.org");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return false;
    }

    RTC_TimeTypeDef rtctime;
    RTC_DateTypeDef rtcdate;

    rtctime.Seconds = timeinfo.tm_sec;
    rtctime.Minutes = timeinfo.tm_min;
    rtctime.Hours = timeinfo.tm_hour;
    rtcdate.Year = timeinfo.tm_year + 1900;
    rtcdate.Month = timeinfo.tm_mon + 1;
    rtcdate.Date = timeinfo.tm_mday;

    M5.Rtc.SetDate(&rtcdate);
    M5.Rtc.SetTime(&rtctime);
    Serial.print("Synced time to: ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    return true;
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
    struct timeval epoch = {.tv_sec = t};
    settimeofday(&epoch, NULL);
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char *path, const char *message) {
    //Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        //Serial.println("Failed to open file for appending");
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

uint32_t Read32bit(uint8_t Addr) {
    uint32_t ReData = 0;
    Wire1.beginTransmission(0x34);
    Wire1.write(Addr);
    Wire1.endTransmission();
    Wire1.requestFrom(0x34, 4);

    for (int i = 0; i < 4; i++) {
        ReData <<= 8;
        ReData |= Wire1.read();
    }

    return ReData;
}

uint32_t ReadByte(uint8_t Addr) {
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