#include <Wire.h>
#include <si5351.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <JTEncode.h>

#define WSPR_SYMBOL_COUNT 162  // 明确宏定义
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Si5351 si5351;
TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5); // RX, TX
JTEncode jtencode;  // 全局实例

// WSPR配置
uint32_t wspr_freq = 50294500;  // 50.2945 MHz
const double WSPR_SHIFT[4] = {0.0, 1.4648, 2.9296, 4.3944};
uint8_t wspr_symbols[WSPR_SYMBOL_COUNT];
char callsign[7] = "KN4NLL";    // 最大6字符
char maidenhead[5] = "FN31";    // 4字符网格
const uint32_t wspr_interval = 120000;  // 120秒

void generate_wspr_symbols() {
    memset(wspr_symbols, 0, sizeof(wspr_symbols));
    jtencode.wspr_encode(callsign, maidenhead, 27, wspr_symbols);
}

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600);
    
    // OLED初始化
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed");
        while(1);
    }
    display.clearDisplay();
    
    // Si5351初始化
    if(si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0)) {
        Serial.println("Si5351 init failed");
        while(1);
    }
    si5351.set_correction(0, static_cast<si5351_pll_input>(SI5351_PLLA));  // 根据实际校准值设置
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    
    generate_wspr_symbols();
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
        generate_wspr_symbols();
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
    for(int i = 0; i < WSPR_SYMBOL_COUNT; i++) {
        uint64_t freq_hz = wspr_freq + (uint32_t)(WSPR_SHIFT[wspr_symbols[i]] * 100);
        si5351.set_freq(freq_hz * 100ULL, SI5351_CLK0);
        delay(682);  // 精确的682ms符号持续时间
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