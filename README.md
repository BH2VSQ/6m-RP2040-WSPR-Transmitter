# 树莓派RP2040 WSPR发射机

[![开源协议](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

基于RP2040微控制器的50.2945MHz WSPR信标发射机，集成GPS定位和RTC时钟同步功能。本设计符合WSPR协议规范v2.0，适用于业余无线电传播研究。

## 📌 核心功能

- **WSPR发射**：50.2945MHz ±1.4648Hz FSK调制
- **自动定位**：通过GPS获取网格坐标（Maidenhead）
- **高精度时钟**：DS3231 RTC（±2ppm精度）
- **状态显示**：0.96" OLED实时显示系统参数
- **配置接口**：USB串口命令行配置系统参数
- **低功耗模式**：平均功耗 <200mW

## 🛠️ 硬件需求

| 模块    | 型号                | 关键参数       |
| ----- | ----------------- | ---------- |
| 主控    | Raspberry Pi Pico | RP2040双核   |
| 频率合成器 | Si5351A           | 0-160MHz输出 |
| GPS模块 | Ublox NEO-6M      | 1PPS脉冲输出   |
| RTC时钟 | DS3231            | 温度补偿晶振     |
| 射频滤波  | 7阶切比雪夫滤波器         | 截止频率55MHz  |

## 🚀 快速开始

### 硬件连接

```text
RP2040引脚分配：
GP14 (SDA) → OLED/Si5351
GP15 (SCL) → OLED/Si5351  
GP16 (SDA) → DS3231
GP17 (SCL) → DS3231
GP8 (RX)   → GPS TX
GP9 (TX)   → GPS RX
GP22       → 低通滤波器使能
```

### 固件烧录

1. 下载最新[预编译固件](https://example.com/wspr.uf2)
2. 按住BOOTSEL键插入USB
3. 拖放`wspr.uf2`到虚拟磁盘
4. 自动重启后进入配置模式

## ⚙️ 系统配置

### 串口命令示例

```bash
# 设置呼号（需重启生效）
set callsign=N0CALL

# 查看系统状态
status

# 强制GPS定位
gps

# 校准频率（需信号源）
calibrate 50294500
```

### 配置文件 (`/settings.cfg`)

```ini
callsign = N0CALL
tx_power = 10    # dBm (0-20)
time_zone = 8     # UTC+8
```

# 

## 📊 性能指标

| 参数     | 指标         |
| ------ | ---------- |
| 频率稳定度  | ±2Hz @25°C |
| 定位精度   | <10米 (GPS) |
| 时间同步误差 | <1秒/月      |
| 发射周期   | 每2分钟       |

## 🔍 常见问题

**Q: 如何验证发射信号？**  
A: 使用WSJT-X软件接收模式，或访问[WSPRnet](https://wsprnet.org/)

**Q: GPS无法定位怎么办？**  

1. 检查天线朝向天空  
2. 等待冷启动（约3分钟）  
3. 执行`gps reset`命令

**Q: 如何降低功耗？**  

1. 设置`tx_power=0`  
2. 断开OLED供电  

## 📜 开源协议

本项目采用Apache 2.0协议，允许自由使用、修改和分发，但需满足以下要求：

1. 保留原始版权声明
2. 修改文件需明确标注更改内容
3. 包含NOTICE文件（如有）
4. 专利授权自动生效

完整协议内容请查看项目根目录[LICENSE](LICENSE)文件。

---

**BH2VSQ 业余无线电项目** © 2025 [bh2vsq@gmail.com](mailto:bh2vsq@gmail.com)
