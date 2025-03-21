# 树莓派RP2040 6m波段WSPR发射机

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)


基于RP2040的低功耗WSPR信标发射系统，支持GPS时间同步和Si5351频率校准，符合ITU 6m波段规范。



## 目录

- [核心特性](#核心特性)
- [硬件需求](#硬件需求)
- [开发环境](#开发环境)
- [接线指南](#接线指南)
- [固件部署](#固件部署)
- [系统校准](#系统校准)
- [协议声明](#协议声明)
- [技术支持](#技术支持)

---

## 核心特性

- 📡 **WSPR-2协议支持**：兼容WSJT-X解码规范
- ⏱️ **PPS时间同步**：GPS模块提供±1ms精度
- 📶 **低相位噪声**：-145dBc/Hz @1kHz偏移（Si5351A优化配置）
- 🔋 **电源管理**：待机功耗<5mA，支持锂电池供电
- 🖥️ **状态显示**：128x64 OLED实时显示工作参数

---

## 硬件需求

### 主要组件

| 模块    | 规格                | 数量  | 备注       |
| ----- | ----------------- | --- | -------- |
| 主控板   | Raspberry Pi Pico | 1   | RP2040芯片 |
| 频率合成器 | Si5351A模块         | 1   | 需25MHz晶振 |
| GPS模块 | NEO-M8N           | 1   | 带PPS输出   |
| 显示屏   | SSD1306 0.96"     | 1   | I²C接口    |
| 电源模块  | HT7333            | 1   | 射频电路专用   |

### 辅助材料

- 50Ω SMA天线接口
- 铁氧体磁珠（100MHz）
- 0.1μF陶瓷电容（0805封装）

---

## 开发环境

### 1. 软件要求

- Arduino IDE 2.3+
- Python 3.10+（用于UF2转换）

### 2. 开发板配置

1. 添加RP2040支持包：
   
   ```text
   文件 > 首选项 > 附加开发板管理器URL：
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```

2. 安装依赖库：
   
   ```bash
   arduino-cli lib install "Si5351Arduino" "TinyGPSPlus" "Adafruit SSD1306"
   ```

---

## 接线指南

#### 电源部分

| RP2040引脚 | 功能      | 目标模块            | 配置说明             |
| -------- | ------- | --------------- | ---------------- |
| VBUS     | 5V输出    | Si5351 VCC      | 需100μF+0.1μF去耦电容 |
| 3V3(OUT) | 3.3V主电源 | OLED/DS3231 VCC | LC滤波（10μH+47μF）  |
| GND      | 共地      | 所有模块GND         | 星型接地设计           |

#### I²C总线1（主总线）

| RP2040引脚   | 功能   | 目标模块       | 配置说明         |
| ---------- | ---- | ---------- | ------------ |
| GP0 (SDA1) | 主SDA | Si5351 SDA | 4.7kΩ上拉至3.3V |
| GP1 (SCL1) | 主SCL | Si5351 SCL | 4.7kΩ上拉至3.3V |

#### I²C总线2（外设总线）

| RP2040引脚   | 功能    | 目标模块       | 配置说明          |
| ---------- | ----- | ---------- | ------------- |
| GP2 (SDA2) | 外设SDA | OLED SDA   | 独立通道，速率400kHz |
| GP3 (SCL2) | 外设SCL | OLED SCL   |               |
| GP2 (SDA2) | 外设SDA | DS3231 SDA | 总线共享          |
| GP3 (SCL2) | 外设SCL | DS3231 SCL | 总线共享          |

#### 专用接口

| RP2040引脚 | 功能    | 目标模块    | 配置说明     |
| -------- | ----- | ------- | -------- |
| GP12     | PPS输入 | GPS PPS | 施密特触发器整形 |
| GP16     | 状态LED | LED指示灯  | 220Ω限流电阻 |

### DS3231连接细节

```
DS3231模块接线
  ├── VCC → 3.3V（独立供电）
  ├── GND → 系统GND
  ├── SDA → GP2 (SDA2)
  └── SCL → GP3 (SCL2)
```

- 需在DS3231模块背面焊接4.7kΩ上拉电阻
- 电池接口建议连接CR1220纽扣电池

### 0.96寸OLED配置

```cpp
// 在代码中指定OLED参数
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // 若地址冲突可改为0x3D
#define OLED_RESET -1    // 共用复位信号时指定复位引脚
```

### 多I²C设备注意事项

1. **地址分配表**：
   
   | 设备      | I²C地址 |
   | ------- | ----- |
   | Si5351A | 0x60  |
   | SSD1306 | 0x3C  |
   | DS3231  | 0x68  |

2. **总线负载计算**：
   
   - 总线电容：<200pF（建议每设备加10pF补偿）
   - 总线上拉电阻：4.7kΩ（3.3V系统）

3. **推荐线序**：
   
   ```
   RP2040 ──┬─ OLED（0.96"）
         ├─ DS3231
         └─ 温度传感器（可选）
   ```

### PCB布局建议

```
射频区               数字区
+---------------+   +---------------+
| Si5351        |   | RP2040        |
| 时钟输出─◯◯◯──┼───┤ GP0-GP1       |
|               |   |               |
| 滤波电路      |   | GPS/OLED接口  |
+---------------+   +---------------+
```

---

## 固件部署

### 编译步骤

1. 克隆仓库：
   
   ```bash
   git clone https://github.com/BH2VSQ/6m-RP2040-WSPR-Transmitter.git
   ```

2. 编译固件：
   
   ```bash
   arduino-cli compile --fqbn rp2040:rp2040:rpipico firmware/wspr
   ```

3. 生成UF2文件：
   
   ```bash
   elf2uf2 -o wspr.uf2 firmware/wspr.ino.elf
   ```

### 烧录流程

1. 进入BOOTSEL模式
2. 挂载UF2磁盘
3. 复制生成的文件到设备

---

## 系统校准

### 频率校准

```bash
> CALIBRATE 7038600  # 使用频率计测量实际输出
[INFO] 校准偏移量: +2.3Hz
```

### GPS同步验证

```bash
> CHECK_SYNC
[PPS] 误差: 0.8ms
[UTC] 2023-09-01 12:00:00Z
```

---

## 协议声明

### 开源协议

本项目采用 [Apache License 2.0](LICENSE)，使用须知：

1. 保留所有版权声明
2. 修改文件需添加变更说明
3. 包含NOTICE文件

### 无线电合规性

- 发射功率限制：<+20dBm
- 频率容限：±50Hz
- 遵守本地无线电法规

---

## 技术支持

**维护者**: BH2VSQ  
**问题追踪**: [GitHub Issues](https://github.com/BH2VSQ/6m-RP2040-WSPR-Transmitter/issues)  

```
> **警告**：操作射频设备需持有相应资质，严禁在未授权频段发射！
```
