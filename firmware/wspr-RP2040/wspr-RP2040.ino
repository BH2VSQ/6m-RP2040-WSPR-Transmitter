#include <Wire.h>
#include <si5351.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <JTEncode.h>
#include <EEPROM.h>
#include <Arduino.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Si5351 si5351;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5); // RX, TX 连接 NEO-6M

// WSPR 频率（单位：Hz）
uint32_t wspr_freq = 50249500;
uint32_t wspr_interval = 120000; // 120秒
int8_t wspr_power = 27; // dBm 仅用于显示
int wspr_calib = 0; // 频率校准（Hz）

// FSK 频偏（单位：Hz）
const int WSPR_SHIFT[4] = {0, 146, 292, 438};

// 默认参数
char maidenhead[7] = "BH2VSQ";
char callsign[10] = "PN11";
uint8_t wspr_symbols[162]; // 由 JTEncode 生成


void saveConfig() {
    EEPROM.put(0, callsign);
    EEPROM.put(10, maidenhead);
    EEPROM.put(20, wspr_freq);
    EEPROM.put(24, wspr_interval);
    EEPROM.put(28, wspr_power);
    EEPROM.put(32, wspr_calib);
    Serial.println("Config saved to EEPROM.");
}

void loadConfig() {
    EEPROM.get(0, callsign);
    EEPROM.get(10, maidenhead);
    EEPROM.get(20, wspr_freq);
    EEPROM.get(24, wspr_interval);
    EEPROM.get(28, wspr_power);
    EEPROM.get(32, wspr_calib);
}

void generate_wspr_symbols() {
    memset(wspr_symbols, 0, sizeof(wspr_symbols));
    JTEncode jtencode;
    jtencode.wspr_encode(callsign, maidenhead, wspr_power, wspr_symbols);
}

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600);
    EEPROM.begin(512);
    loadConfig();

    Serial.println("Initializing Si5351, GPS, and OLED...");
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed!");
        while (1);
    }
    display.clearDisplay();
    display.display();
    
    if (si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0)) {
        Serial.println("Si5351 initialization failed!");
        while (1);
    }
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    generate_wspr_symbols();
}

void processSerialCommand() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.startsWith("SET CALLSIGN ")) {
            command.substring(13).toCharArray(callsign, sizeof(callsign));
            Serial.println("Callsign updated: " + String(callsign));
            generate_wspr_symbols();
        } else if (command.startsWith("SET GRID ")) {
            command.substring(9).toCharArray(maidenhead, sizeof(maidenhead));
            Serial.println("Grid updated: " + String(maidenhead));
            generate_wspr_symbols();
        } else if (command.startsWith("SET FREQ ")) {
            wspr_freq = command.substring(9).toInt();
            Serial.println("Frequency updated: " + String(wspr_freq));
        } else if (command.startsWith("SET INTERVAL ")) {
            wspr_interval = command.substring(13).toInt() * 60000;
            Serial.println("Interval updated: " + String(wspr_interval / 60000) + " min");
        } else if (command.startsWith("SET POWER ")) {
            wspr_power = command.substring(10).toInt();
            Serial.println("Power updated: " + String(wspr_power) + " dBm");
            generate_wspr_symbols();
        } else if (command.startsWith("SET CALIB ")) {
            wspr_calib = command.substring(10).toInt();
            Serial.println("Calibration updated: " + String(wspr_calib) + " Hz");
        } else if (command == "SAVE") {
            saveConfig();
        } else if (command == "GET STATUS") {
            Serial.println("Callsign: " + String(callsign));
            Serial.println("Grid: " + String(maidenhead));
            Serial.println("Freq: " + String(wspr_freq));
            Serial.println("Interval: " + String(wspr_interval / 60000) + " min");
            Serial.println("Power: " + String(wspr_power) + " dBm");
            Serial.println("GPS: " + String(gps.location.isValid() ? "LOCKED" : "NO SIGNAL"));
        } else if (command == "LIST CONFIG") {
            Serial.println("Configuration Values:");
            Serial.println("CALLSIGN: " + String(callsign));
            Serial.println("GRID: " + String(maidenhead));
            Serial.println("FREQ: " + String(wspr_freq));
            Serial.println("INTERVAL: " + String(wspr_interval / 60000) + " min");
            Serial.println("POWER: " + String(wspr_power) + " dBm");
            Serial.println("CALIB: " + String(wspr_calib) + " Hz");
        } else if (command == "HELP") {
            Serial.println("Available commands:");
            Serial.println("SET CALLSIGN <XX1XXX>");
            Serial.println("SET GRID <FN31AA>");
            Serial.println("SET FREQ <Hz>");
            Serial.println("SET INTERVAL <minutes>");
            Serial.println("SET POWER <dBm>");
            Serial.println("SET CALIB <Hz>");
            Serial.println("SAVE");
            Serial.println("RESET");
            Serial.println("GET STATUS");
            Serial.println("LIST CONFIG");
        } else {
            Serial.println("Unknown command. Type HELP for a list of commands.");
        }
    }
}

void loop() {
    processSerialCommand();
}
