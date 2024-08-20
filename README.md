# BambuFlag for ESP32
## Overview 

The BambuFlag is an application designed display a Bambu X1 3D printer status. Using WiFi and MQTT protocols to monitor the printer's status and control a servo that moves some flags.

## List of supplies
- ESP32 board: https://amzn.eu/d/gnoAfbd
- Servo https://amzn.eu/d/5UxaUpT
- Wires: [https://amzn.eu/d/bfbP5ST](https://amzn.eu/d/5hwFUJG)

# BambuFlag Makerworld files
- **BambuFlag:** https://makerworld.com/en/models/594156#profileId-515995
## Features

- **WiFi and MQTT Connectivity:** Connects to a local WiFi network and communicates with the printer via MQTT
- **Servo Control:** Activates a servo to show printer statuses
- **Web Server:** Hosts a web server to provide manual control and configuration of the system
- **Stage Monitoring:** Monitors various stages of the printer to determine when to activate the motor

## Setup
Short video about setup https://youtu.be/CrnCKuUL-qE

In the case of having to change the boundaries of the motor they can be changed and saved into the eeprom memory of the chip from the Setting landing page:
![image](https://github.com/user-attachments/assets/d4d777a2-afcb-419f-bd70-896939426058)

### WiFi and MQTT Configuration

Enter your WiFi and MQTT credentials in the following variables:

```cpp
// WiFi credentials
char ssid[40] = "your-ssid";
char password[40] = "your-password";

// MQTT credentials
char mqtt_server[40] = "your-bambu-printer-ip";
char mqtt_password[30] = "your-bambu-printer-accesscode";
char serial_number[20] = "your-bambu-printer-serial-number";

```
### Note:
- **ssid** is your WIFI name
- **password** is the WIFI password
- **mqtt_server** is your Bambu Printers IP address
- **mqtt_password** is your Bambu printer access code as found on your printer
- **serial_number** is your Bambu printer serial number as found on your printer

## GPIO Pins

The application uses the following GPIO pin for the servo:

```cpp

int pinServo = 27;

```

## Installation

1. Connect the ESP32 to your computer.
2. Open the code in the Arduino IDE.
3. Enter your WiFi and MQTT credentials in the respective variables.
4. Upload the code to the ESP32.
5. Access the web server via the IP address assigned to the ESP32 to configure and control the application.

## Wiring
Connect servo to ESP32 poles acordingly and the control pin into the GPIO27. Connection from the servo into the board is done using 3 male-female DuPont cables.

## Usage

### Web Server

The application hosts a web server to provide manual control and configuration. Access the following URLs for different functionalities:

- **Root URL:** Opens Configuration page (`/`)

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Thanks!

Thanks to t0nyz for this repo in wich my code is based on!
https://t0nyz.com/projects/bambuconveyor