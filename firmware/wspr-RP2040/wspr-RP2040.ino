#include <Wire.h>
#include <si5351.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Si5351 si5351;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5); // RX, TX 连接 NEO-6M

// WSPR 频率（单位：Hz）
uint32_t wspr_freq = 50249500;

// WSPR 符号（4-FSK 调制）
const uint8_t wspr_symbols[162] = {3, 0, 1, 2, 3, 1, 2, 0, 3, 2, 1, 0, 3, 0, 2, 1,
                                   0, 1, 3, 2, 1, 0, 3, 2, 1, 3, 0, 2, 1, 3, 0, 2,
                                   3, 1, 2, 0, 3, 1, 0, 2, 3, 1, 2, 0, 3, 2, 1, 0,
                                   3, 0, 2, 1, 0, 1, 3, 2, 1, 0, 3, 2, 1, 3, 0, 2,
                                   1, 3, 0, 2, 3, 1, 2, 0, 3, 1, 0, 2, 3, 1, 2, 0,
                                   3, 2, 1, 0, 3, 0, 2, 1, 0, 1, 3, 2, 1, 0, 3, 2,
                                   1, 3, 0, 2, 1, 3, 0, 2, 3, 1, 2, 0, 3, 1, 0, 2,
                                   3, 1, 2, 0, 3, 2, 1, 0, 3, 0, 2, 1, 0, 1, 3, 2,
                                   1, 0, 3, 2, 1, 3, 0, 2, 1, 3, 0, 2, 3, 1, 2, 0,
                                   3, 1, 0, 2, 3, 1, 2, 0, 3, 2, 1, 0, 3, 0, 2, 1,
                                   0, 1, 3, 2, 1, 0};

// FSK 频偏（单位：Hz）
const int WSPR_SHIFT[4] = {0, 146, 292, 438};

// 默认参数
char maidenhead[7] = "FN31AA";
char callsign[10] = "XX1XXX";
uint32_t wspr_interval = 120000; // 120秒

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600);
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
}

void updateGPS() {
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }
    if (gps.location.isValid()) {
        double lat = gps.location.lat();
        double lon = gps.location.lng();
        int lon_i = (lon + 180) / 20;
        int lat_i = (lat + 90) / 10;
        int lon_r = ((lon + 180) - lon_i * 20) / 2;
        int lat_r = ((lat + 90) - lat_i * 10) / 1;
        maidenhead[0] = 'A' + lon_i;
        maidenhead[1] = 'A' + lat_i;
        maidenhead[2] = '0' + lon_r;
        maidenhead[3] = '0' + lat_r;
        maidenhead[4] = '\0';
    }
}

void updateOLED() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Callsign: "); display.println(callsign);
    display.print("Grid: "); display.println(maidenhead);
    display.print("Freq: "); display.println(wspr_freq / 1000000.0, 6);
    display.print("GPS: ");
    if (gps.location.isValid()) {
        display.println("LOCKED");
    } else {
        display.println("NO SIGNAL");
    }
    display.display();
}

void send_wspr() {
    for (int i = 0; i < 162; i++) {
        uint32_t freq = wspr_freq + WSPR_SHIFT[wspr_symbols[i]];
        si5351.set_freq(freq * 100ULL, SI5351_CLK0);
        delay(682);
    }
}

void loop() {
    updateGPS();
    updateOLED();
    Serial.print("Current Grid Locator: ");
    Serial.println(maidenhead);
    Serial.println("Transmitting WSPR...");
    send_wspr();
    Serial.println("WSPR transmission complete. Waiting for next cycle...");
    delay(wspr_interval);
}