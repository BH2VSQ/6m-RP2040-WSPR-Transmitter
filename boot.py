# boot.py - 系统启动配置
import board
import digitalio
import busio
import storage
import usb_cdc
import microcontroller
import time
from adafruit_ds3231 import DS3231

# 配置USB设备描述符
microcontroller.cpu.usb_product = "WSPR Transmitter"
microcontroller.cpu.usb_vendor = "HamLab"
microcontroller.cpu.usb_serial = "v1.0-RP2040"

# 禁用USB存储模式（仅启用串口）
storage.disable_usb_drive()

# 启用串口控制台
usb_cdc.enable(console=True, data=False)

# 初始化主I2C总线（OLED和SI5351）
try:
    i2c_main = busio.I2C(board.GP15, board.GP14, frequency=400_000)
except RuntimeError as e:
    print(f"主I2C初始化失败: {e}")
    i2c_main = None

# 初始化备用I2C总线（RTC专用）
try:
    i2c_rtc = busio.I2C(board.GP17, board.GP16, frequency=100_000)
    rtc = DS3231(i2c_rtc)
except Exception as e:
    print(f"RTC初始化失败: {e}")
    rtc = None

# RTC电池状态检查
if rtc:
    batt_status = "OK" if rtc.lost_power else "LOW"
    print(f"[RTC] 电池状态: {batt_status}")
    if rtc.lost_power:
        # 闪烁板载LED报警
        led = digitalio.DigitalInOut(board.LED)
        led.switch_to_output()
        for _ in range(5):
            led.value = True
            time.sleep(0.2)
            led.value = False
            time.sleep(0.2)

# 文件系统保护机制
write_pin = digitalio.DigitalInOut(board.GP28)
write_pin.switch_to_input(pull=digitalio.Pull.UP)
if not write_pin.value:
    print("进入写模式")
    storage.remount("/", False)
else:
    print("进入只读模式")
    storage.remount("/", True)

# 系统时钟初始化
if rtc:
    sys_time = rtc.datetime
    if sys_time.tm_year > 2023:  # 验证RTC时间有效性
        microcontroller.rtc.datetime = sys_time
        print(f"系统时钟已同步RTC: {sys_time}")
    else:
        print("RTC时间未设置，使用默认时间")

# 硬件状态LED指示
status_led = digitalio.DigitalInOut(board.GP25)
status_led.switch_to_output()
status_led.value = True  # 启动完成指示灯

# 保存启动日志
try:
    with open("/boot.log", "a") as f:
        from adafruit_ntp import NTP
        ts = NTP().datetime if rtc else time.time()
        f.write(f"\n[{ts}] 系统启动完成")
except Exception as e:
    print(f"启动日志记录失败: {e}")