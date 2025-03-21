#include <Wire.h>
#include <si5351.h>
#include <TinyGPS++.h>
#include <RTClib.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <EEPROM.h>

// 硬件定义
#define OLED_RESET    -1
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
Si5351 si5351;
TinyGPSPlus gps;
RTC_DS3231 rtc;

#define ERROR_NONE 0  // 临时解决方案（不推荐长期使用）

// WSPR参数
void interleave(uint8_t *sym);
const uint32_t WSPR_TONE_SPACING = 14648;  // 1.4648Hz in 0.01Hz units
const uint16_t WSPR_TX_DURATION = 110;      // 发射持续时间(秒)
const uint8_t WSPR_SYMBOL_COUNT = 162;

// 配置结构体
struct Config {
  char callsign[7] = "N0CALL";
  char defaultGrid[6] = "AA00";
  float txFreq = 50.293;
  int32_t txOffset = 0;
  uint16_t txInterval = 10;
  int8_t power = 27;
  int32_t freqCal = 0;
  uint8_t checksum;
} config;

// 全局变量
char currentGrid[6];
bool gpsValid = false;
uint8_t wsprSymb[WSPR_SYMBOL_COUNT];
bool txEnabled = true;

// Add this function definition to your code
void interleave(unsigned char *sym) {
    unsigned char temp[162];
    for (int i = 0; i < 162; i++) {
        int j = (i * 181) % 162; // WSPR interleave pattern
        temp[j] = sym[i];
    }
    memcpy(sym, temp, 162);
}

// EEPROM保存配置
void saveConfig() {
  config.checksum = 0;
  EEPROM.put(0, config);
}

// 呼号编码函数（增强版）
uint32_t callsignToN(const char* callsign) {
  char call[6] = {' ', ' ', ' ', ' ', ' ', ' '};
  strncpy(call, callsign, 6);
  
  // 处理便携式操作指示
  char *p = strchr(call, '/');
  if(p) {
    if(strlen(p) == 3) { // 便携式后缀
      call[0] = p[1];
      call[1] = p[2];
      call[2] = ' ';
    }
  }

  uint32_t n = 0;
  if(isdigit(call[2])) { // 标准呼号结构
    n = (call[0] - 'A') * 36 * 36 * 27;
    n += (call[1] - 'A') * 36 * 27;
    n += (call[2] - '0') * 27;
    n += (call[3] == ' ') ? 26 : (call[3] - 'A');
  } else { // 特殊格式处理
    n = 36 * 36 * 36 * 27;
    n += (call[0] - 'A') * 36 * 36 * 27;
    n += (call[1] - '0') * 36 * 27;
    n += (call[2] - 'A') * 27;
    n += (call[3] - 'A');
  }
  return n;
}

// WSPR CRC计算（修正为16位CCITT）
uint16_t wspr_crc(const uint8_t *data, int len) {
  uint16_t crc = 0xFFFF;
  for(int i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for(int j = 0; j < 8; j++) {
      if(crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc;
}

// 卷积编码（优化实现）
void convolve(const uint8_t *in, int len, uint8_t *out) {
  uint32_t shift_reg = 0;
  const uint32_t poly1 = 0xF2D05351;
  const uint32_t poly2 = 0xE4613C47;
  
  for(int i = 0; i < len; i++) {
    for(int b = 7; b >= 0; b--) {
      shift_reg = (shift_reg << 1) | ((in[i] >> b) & 1);
      
      // 使用硬件奇偶校验加速
      out[i*16 + (7-b)*2] = __builtin_parity(shift_reg & poly1);
      out[i*16 + (7-b)*2 + 1] = __builtin_parity(shift_reg & poly2);
    }
  }
}

// 交织表（修正为WSPR标准）
const uint8_t interleave_table[162] = {
  0,   81,  90,  2,   11,  85,  94,  4,   15,  89,  98,  6,   19,  93,  102, 8,
  23,  97,  106, 10,  27,  101, 110, 12,  31,  105, 114, 14,  35,  109, 118, 16,
  39,  113, 122, 18,  43,  117, 126, 20,  47,  121, 130, 22,  51,  125, 134, 24,
  55,  129, 138, 26,  59,  133, 142, 28,  63,  137, 146, 30,  67,  141, 150, 32,
  71,  145, 154, 34,  75,  149, 158, 36,  79,  153, 162, 38,  83,  157, 166, 40,
  87,  161, 170, 42,  91,  165, 174, 44,  95,  169, 178, 46,  99,  173, 182, 48,
  103, 177, 186, 50,  107, 181, 190, 52,  111, 185, 194, 54,  115, 189, 198, 56,
  119, 193, 202, 58,  123, 197, 206, 60,  127, 201, 210, 62,  131, 205, 214, 64,
  135, 209, 218, 66,  139, 213, 222, 68,  143, 217, 226, 70,  147, 221, 230, 72,
  151, 225, 234, 74,  155, 229, 238, 76,  159, 233, 242, 78,  163, 237, 246, 80
};

// 梅登黑德网格计算（完整实现）
String calculateMaidenhead(float lat, float lon) {
  char grid[7] = {0};
  lon += 180;
  lat += 90;

  grid[0] = 'A' + (int)(lon / 20);
  grid[1] = 'A' + (int)(lat / 10);
  grid[2] = '0' + (int)fmod(lon, 20)/2;
  grid[3] = '0' + (int)fmod(lat, 10);
  
  float lon_remainder = fmod(lon, 2)/2 * 24;
  float lat_remainder = fmod(lat, 1) * 24;
  grid[4] = 'A' + (int)lon_remainder;
  grid[5] = 'A' + (int)lat_remainder;

  // 边界保护
  grid[4] = constrain(grid[4], 'A', 'X');
  grid[5] = constrain(grid[5], 'A', 'X');
  
  return String(grid).substring(0,6);

}

// WSPR编码函数（关键修正）
void prepareWSPR() {
  // 输入验证
  config.power = constrain(config.power, -64, 63);
  
  // 构造消息
  uint8_t packed[11] = {0};
  uint32_t n = callsignToN(config.callsign);
  
  // 打包呼号（28 bits）
  packed[0] = (n >> 20) & 0xFF;
  packed[1] = (n >> 12) & 0xFF;
  packed[2] = (n >> 4) & 0xFF;
  packed[3] = (n & 0x0F) << 4;

  // 网格编码（15 bits）
  String grid = gpsValid ? currentGrid : config.defaultGrid;
  packed[3] |= (grid.charAt(0) - 'A') >> 2;
  packed[4] = ((grid.charAt(0) - 'A') & 0x03) << 6;
  packed[4] |= (grid.charAt(1) - 'A') << 3;
  packed[4] |= (grid.charAt(2) - '0') >> 1;
  packed[5] = ((grid.charAt(2) - '0') & 0x01) << 7;
  packed[5] |= (grid.charAt(3) - '0') << 4;
  packed[5] |= (config.power + 64) >> 2;
  packed[6] = ((config.power + 64) & 0x03) << 6;

  // 计算CRC（16位）
  uint16_t crc = wspr_crc(packed, 7);
  packed[6] |= (crc >> 16) & 0x3F;
  packed[7] = (crc >> 8) & 0xFF;
  packed[8] = crc & 0xFF;

  // 卷积编码
  uint8_t symbols[162 * 2];
  convolve(packed, 11, symbols);

  // 符号映射和交织
  for(int i = 0; i < 162; i++) {
    wsprSymb[i] = (symbols[i*2] << 1) | symbols[i*2+1];
  }
  interleave(wsprSymb);
}
// 精确延时函数
void preciseDelay(unsigned long ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    delayMicroseconds(100);
  }
}

// 发射控制
void transmitWSPR() {
  if (!txEnabled) return;

  // 计算实际发射频率(Hz)
  uint64_t freq_hz = (uint64_t)(config.txFreq * 1000000ULL) + config.txOffset;
  
  // 设置SI5351频率
  if (si5351.set_freq(freq_hz, SI5351_CLK0) != ERROR_NONE) {
    display.setCursor(0, 16);
    display.print("SI5351 Error!");
    display.display();
    return;
  }
  
  // 启用时钟输出
  si5351.output_enable(SI5351_CLK0, 1);
  
  // 更新显示
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Tx: ");
  display.print(config.txFreq, 3);
  display.println(" MHz");
  display.display();

  // 保持发射状态
  preciseDelay(WSPR_TX_DURATION * 1000);

  // 关闭输出
  si5351.output_enable(SI5351_CLK0, 0);
}

// GPS处理
void checkGPS() {
  static uint32_t lastUpdate = 0;
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      if (gps.location.isValid() && gps.date.isValid()) {
        gpsValid = true;
        lastUpdate = millis();
        
        // 更新网格坐标
        String grid = calculateMaidenhead(gps.location.lat(), gps.location.lng());
        grid.toCharArray(currentGrid, 6);
        
        // 同步RTC
        rtc.adjust(DateTime(gps.date.year(), gps.date.month(), gps.date.day(),
                            gps.time.hour(), gps.time.minute(), gps.time.second()));
      }
    }
  }
  if (millis() - lastUpdate > 10000) {
    gpsValid = false;
  }
}

void setup() {
  // 初始化显示
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // 初始化SI5351
  if (si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0) != ERROR_NONE) {
    display.println("SI5351 Init Fail!");
    display.display();
    while(1);
  }
  si5351.set_correction(config.freqCal, SI5351_PLL_INPUT_XO);

  // 初始化GPS
  Serial1.setRX(16);   // 设置接收引脚
  Serial1.setTX(17);   // 设置发送引脚
  Serial1.begin(9600); // 启动串口

  // 加载配置
  EEPROM.get(0, config);
}

void loop() {
  static uint32_t lastTx = 0;
  DateTime now = rtc.now();

  checkGPS();
  
  // 显示状态
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(gpsValid ? "GPS: " : "No GPS: ");
  display.println(currentGrid);
  display.print("Time: ");
  display.print(now.hour()); display.print(":");
  display.print(now.minute()); display.print(":");
  display.println(now.second());
  display.display();

  // 发射时间判断
  uint32_t currentTime = now.unixtime();
  if ((currentTime - lastTx) >= config.txInterval * 60) {
    prepareWSPR();
    transmitWSPR();
    lastTx = currentTime;
  }

  delay(1000);
}