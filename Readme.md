# Build your own portable, graphing CO₂ sensor

![Device](pictures/device.png "The Device")

## Motivation

CO₂ levels measured indoors can be used as an indicator of air quality.

Outdoor air has a CO₂ ppm level of 400 ppm. Humans exhale air with 40000 ppm. You can calculate the percentage of exhaust air you are breathing by dividing measured CO₂ level above outdoor by the human exhaust level. Used air might contain aerosols which might contain the corona virus.

High indoor CO₂ levels also have an impact on the cognitive performance of humans.

Smoca AG decided to build this easy to reproduce CO₂ measuring device to help slow the spread of the corona virus in offices.
It supports its users in deciding when ventilating the room begin monitored is advisable. It also raises awareness for the correlation between CO₂ level and air quality.

## Hardware

The following hardware is needed:

| description                 | cost     | link                                                                                                                             |
| --------------------------- | -------- | -------------------------------------------------------------------------------------------------------------------------------- |
| Sensirion SCD30             | 54 USD   | https://www.digikey.com/en/products/detail/sensirion-ag/SCD30/8445334<br>Switzerland: https://www.bastelgarage.ch/co2-sensor-scd30-sensirion                                                            |
| M5 Stack Core2              | 39.9 USD | https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/K010/13151126?s=N4IgTCBcDaILYFYDOAXAhgYwNYAIMHsAnAUwgF0BfIA<br>Switzerland: https://www.bastelgarage.ch/m5stack-core2-esp32-iot-entwicklungs-kit |
| M5 Stack Proto Board        | 2.95 USD | https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/M001/10492145<br>Switzerland: https://www.bastelgarage.ch/m5stack-core-development-proto-board                                               |
| M5 Stack PLC Base           | 9.95 USD | https://m5stack.com/collections/m5-base/products/plc-proto-industrial-board-module<br>Switzerland: https://www.bastelgarage.ch/m5stack-plc-proto-base-industrial-board                                               |
| M5 Stack Battery (optional) | 7.95 USD | https://www.digikey.com/en/products/detail/m5stack-technology-co-ltd/M002/10492127<br>Switzerland: https://www.bastelgarage.ch/m5stack-battery-module-700mah-fur-esp32-core                                               |

Note: The optional battery increases the battery operation time from 6 to 10 hours.

### Building

|Description | Image
|----------- | -----------
|1. Power the Core2 module. A factory test application should be loaded. Test the device. |
|2. Remove the bottom of the Core2 module. Keep the battery there or replace it with the optional bigger one. | ![Front](pictures/front.jpg "Front")
|3. Remove the PCB from the proto module. |
|4. Connect the SCD30 module to the proto module. We used pin headers. | ![solder](pictures/solder.jpg "solder")
|5. Connect VDD with 3V3, GND with GND, TX/SCL with pin 5 and RX/SDA with pin 2. The other pins don't need to be connected to the proto module. | ![scd30](pictures/scd30.png "scd30")
|5. Screw the proto module to the PLC Base |
|6. Screw the PLC Base to the Core2 module | ![together](pictures/together.jpg "connected")
|7. HW Done, load the software |

## Software

To initialise the device do the following:

1. Install [Platformio IDE](https://platformio.org/platformio-ide) or [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html) to use another IDE
2. Open the `co2-sensor` project folder
3. Flash the `main.cpp` script to your M5 Stack Core2 with `pio run -t upload --upload-port {your_device}`. You can see a list of your devices with `pio device list`. Dependencies are installed automatically.

## Features

#### Calibration

The SCD30 can be calibrated manually or automatically. For manual calibration, the device must be placed outdoors for at least five minutes. Then the calibration can be adjusted. Fresh air has a CO₂ concentration of about 400 ppm. To use automatic calibration, the device must have access to fresh air for at least one hour a day.

#### WiFi

To use MQTT, synchronization and update functionality the device must have internet connection. To add WiFi credentials the device opens an access point with a web interface (Default IP is 192.168.4.1). 

#### Data sharing via MQTT

[MQTT](https://mqtt.org/) can be used to send the sensors data to a custom MQTT broker. Server, Port, Topic, Device name, Username and Password can also be configured in the web interface. Each minute the device sends its data in the following format: `{TOPIC}/{CATEGORY} {VALUE}`. Here an example: `sensor/co2 650`.  
Topics are able to have subtopics: `office2/meeting_room1/co2 1100`.  
The following categories are provided by the device: `/co2`, `/humidity`, `/temperature`.

#### Time synchronization

To keep time up to date the device synchronizes with a time server each night between two and three o'clock.

#### Firmware updates

Is a new firmware available, it can manually be updated via WiFi.

#### SNMP

Using SNMP you are able to read the values of the co2, temperature and humidity sensors and also check the battery status of the device:
1. First you have to make sure you have net-snmp installed on your system. You can do this using your packet manager or by downloading it from http://www.net-snmp.org/download.html
2. Second you have to find out where your mib files are stored using the command ```net-snmp-config --default-mibdirs```.
3. Then you have to copy the [mib file](./snmp-mib/sensorhub.mib) from ```/snmp-mib/sensorhub.mib``` into one of the listed directories. e.g. ```cp snmp-mib/sensorhub.mib ~/.snmp/mibs```
4. Done that, you can now use snmptable to list the sensors: 
    ```snmptable -m +SENSORHUB-MIB -c public {IP-Address} shSensorTable```
    \
    Which should output the following:
    ```
    SNMP table: SENSORHUB-MIB::shSensorTable

    shSensorName
            SCD30
            BATTERY
   ```
    or to list the measurement values: 
    ```snmptable -m +SENSORHUB-MIB -c public {IP-Address} shMeasurementTable```
    \
    Which should output something like this:
    ```
    SNMP table: SENSORHUB-MIB::shMeasurementTable

    shMeasurementType shMeasurementValue
                co2                  617
        temperature                  207
           humidity                  433
            voltage                  3
    electricCurrent                  276
   ```

## Usage

**Disclaimer This is not a safety device. Always follow the local regulations regarding corona safety. No warranty is provided**
![Device](pictures/flyer.png "Device usage")

### Graphs

- The graphs alway show the last six hours. Every line on the graph represents the measurement of one minute. The last value measured in the given minute is used for the graph.
- To switch between different graphs (temperature / humidity / battery / co2) touch the corresponding value in the display

### microsd

If a microsd card formated with fat is present, a csv file, data.txt will be written every 2 seconds with the timestamp, the co2 level, the humidity, the temperature and the current battery charge.

### CO₂

| level           | color  | description                                                                                               |
| --------------- | ------ | --------------------------------------------------------------------------------------------------------- |
| < 600 ppm       | blue   | outdoor air                                                                                               |
| 600 - 800 ppm   | green  | fresh indoor air                                                                                          |
| 800 - 1000 ppm  | yellow | indoor air, ventilate the room if possible, small possibility of virus transmission                       |
| 1000 - 1400 ppm | orange | used indoor air, ventilate the room, cognition is impacted, possibility of virus transmission             |
| > 1400 ppm      | red    | heavy used indoor air, heavy cognition impact, high possiblity of virus transmission, ventilation needed. |

## Extended uses (no code provided)

- We use it at our office to control a couple of exhaust / inlet pwm computer fans to keep the CO₂ level below 800 ppm.
- We log the measurement results to a influxdb with a grafana dashboard. The grafana instance sends alerts to telegram for CO₂ levels above 800 ppm.

## Contact

If you have any suggestions for the project feel free to file an issue. If you want to build the device on a bigger scale / have any other development / prototyping needs, feel free to contact us: info@smoca.ch

## License

Copyright (c) 2020 David Gunzinger / smoca AG www.smoca.ch \
Code: GPL v3 \
Text / Graphics / Photographs: CC BY-SA
