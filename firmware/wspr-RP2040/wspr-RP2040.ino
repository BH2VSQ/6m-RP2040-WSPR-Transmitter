/*
  WSPR 信标 (6m波段) - 基于 RP2040
  硬件:
  - RP2040
  - Si5351 频率合成器 (I2C)
  - NEO-6M GPS 模块 (UART)
  - DS3231 实时时钟 (I2C)
  - 0.96寸 OLED 屏幕 (I2C)
  - EEPROM (用于存储配置参数)

  接线:
  - OLED (SSD1306) -> SDA (GP20), SCL (GP21)
  - Si5351 -> SDA (GP20), SCL (GP21)
  - NEO-6M -> RX (GP4), TX (GP5)
  - EEPROM (内部存储)
*/

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
int8_t wspr_power = 20; // dBm 仅用于显示
int wspr_calib = 0; // 频率校准（Hz）

// FSK 频偏（单位：Hz）
const int WSPR_SHIFT[4] = {0, 146, 292, 438};

// 默认参数
char maidenhead[7] = "BH2VSQ";
char callsign[10] = "PN11";
uint8_t wspr_symbols[162]; // 由 JTEncode 生成

void generate_wspr_symbols() {
    memset(wspr_symbols, 0, sizeof(wspr_symbols));
    JTEncode jtencode;
    jtencode.wspr_encode(callsign, maidenhead, wspr_power, wspr_symbols);
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WSPR Beacon");
    display.print("Call: "); display.println(callsign);
    display.print("Grid: "); display.println(maidenhead);
    display.print("Freq: "); display.print(wspr_freq / 1000000.0, 6); display.println(" MHz");
    display.print("Power: "); display.print(wspr_power); display.println(" dBm");
    display.print("GPS: "); display.println(gps.location.isValid() ? "LOCKED" : "NO SIGNAL");
    display.display();
}

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600);
    Serial.println("Initializing Si5351, GPS, and OLED...");
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed!");
        while (1);
    }
    updateDisplay();
    
    if (si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0)) {
        Serial.println("Si5351 initialization failed!");
        while (1);
    }
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    generate_wspr_symbols();
}

void loop() {
    updateDisplay();
}
