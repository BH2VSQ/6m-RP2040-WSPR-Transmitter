#include <Wire.h>
#include "si5351.h"

Si5351 si5351;
#define I2C_SDA 4
#define I2C_SCL 5

// CW配置参数
const int CW_WPM = 15;               // 发送速度（字/分钟）
const int DOT_DURATION = 1200 / CW_WPM;  // 点基准时长（ms）
const char CALLSIGN[] = "RPICO5351-BEACON-BH2VSQ-PN11-CHINA-BEACON"; // 全大写发送内容

// 扩展的莫尔斯编码表（ITU标准）
const struct {
  char character;
  const char *code;
} morseTable[] = {
  // 字母
  {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
  {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
  {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
  {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
  {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
  {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
  {'Y', "-.--"},  {'Z', "--.."},
  
  // 数字
  {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
  {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."},
  {'9', "----."}, {'0', "-----"},
  
  // 标点符号
  {'.', ".-.-.-"}, // 句号
  {',', "--..--"}, // 逗号
  {'?', "..--.."}, // 问号
  {'\'', ".----."},// 单引号
  {'!', "-.-.--"}, // 感叹号
  {'/', "-..-."},  // 斜杠
  {'(', "-.--."},  // 左括号
  {')', "-.--.-"}, // 右括号
  {'&', ".-..."},  // AND符
  {':', "---..."}, // 冒号
  {';', "-.-.-."}, // 分号
  {'=', "-...-"},  // 等号
  {'-', "-....-"}, // 连字符
  {'_', "..--.-"}, // 下划线
  {'\"', ".-..-."},// 双引号
  {'$', "...-..-"},// 美元符
  {'@', ".--.-."}, // @符号
  {'+', ".-.-."},  // 加号
  {' ', " "}       // 单词空格
};

void setup() {
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  // 初始化Si5351（25MHz晶振）
  if (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0)) {
    while(true); // 初始化失败时停机
  }

  // 配置14.280MHz输出
  si5351.set_freq(5009600000ULL, SI5351_CLK0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, false);

  // 取消注释进行频率校准（需实际测量）
  // si5351.set_correction(12345); 
}

void loop() {
  sendCW(CALLSIGN);
  delay(7000 / CW_WPM); // 单词间隔（7单位时间）
}

// 莫尔斯编码发送主函数
void sendCW(const char *text) {
  for (int i = 0; text[i] != '\0'; i++) {
    char c = toupper(text[i]);
    
    if (c == ' ') { // 处理单词空格
      delay(7 * DOT_DURATION);
      continue;
    }

    const char *code = getMorseCode(c);
    if (code) sendMorse(code);
    
    delay(3 * DOT_DURATION); // 字符间隔（3单位时间）
  }
}

// 获取字符对应的莫尔斯码
const char* getMorseCode(char c) {
  for (unsigned int i = 0; i < sizeof(morseTable)/sizeof(morseTable[0]); i++) {
    if (morseTable[i].character == c) {
      return morseTable[i].code;
    }
  }
  return NULL; // 未定义字符静默处理
}

// 发送单个莫尔斯字符
void sendMorse(const char *code) {
  for (int i = 0; code[i] != '\0'; i++) {
    if (code[i] == ' ') continue; // 跳过空格
    
    // 开启载波
    si5351.output_enable(SI5351_CLK0, true);
    delay((code[i] == '.') ? DOT_DURATION : 3 * DOT_DURATION);
    
    // 关闭载波并添加符号间隔
    si5351.output_enable(SI5351_CLK0, false);
    if (code[i+1] != '\0') delay(DOT_DURATION);
  }
}