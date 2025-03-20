# code.py - 主系统程序
import time
import os
import microcontroller
import supervisor
from board import *
import busio
import digitalio
import adafruit_ds3231
from ssd1306 import SSD1306_I2C
from si5351 import Si5351
import neopixel

# 硬件配置
CONFIG_FILE = '/settings.cfg'
DEFAULT_CONFIG = {
    'callsign': 'N0CALL',
    'grid': 'AA00',
    'power': '10',
    'freq': '50294500',
    'tz': '0'
}

# 初始化硬件接口
i2c_main = busio.I2C(GP15, GP14)  # 主I2C(OLED/Si5351)
i2c_rtc = busio.I2C(GP17, GP16)   # RTC专用I2C
uart_gps = busio.UART(GP8, GP9, baudrate=9600)
pixel = neopixel.NeoPixel(GP28, 1)

# 外设初始化
try:
    oled = SSD1306_I2C(128, 64, i2c_main, addr=0x3C)
    si = Si5351(i2c_main)
    rtc = adafruit_ds3231.DS3231(i2c_rtc)
except Exception as e:
    print("硬件初始化失败:", e)
    microcontroller.reset()

# 全局状态
gps_lock = False
tx_status = "INIT"
current_grid = DEFAULT_CONFIG['grid']
sys_config = {}

# ================== 辅助函数 ==================
def load_config():
    try:
        config = DEFAULT_CONFIG.copy()
        with open(CONFIG_FILE, 'r') as f:
            for line in f:
                k, v = line.strip().split('=', 1)
                config[k] = v
        return config
    except Exception as e:
        print("配置加载失败:", e)
        return DEFAULT_CONFIG

def save_config(config):
    try:
        with open(CONFIG_FILE, 'w') as f:
            for k, v in config.items():
                f.write(f"{k}={v}\n")
        return True
    except Exception as e:
        print("配置保存失败:", e)
        return False

# ================== GPS处理 ==================
def parse_gps():
    global gps_lock, current_grid
    while uart_gps.in_waiting > 0:
        line = uart_gps.readline()
        if line.startswith(b'$GPRMC'):
            data = line.split(b',')
            if data[2] == b'A' and len(data[1]) >= 6:
                utc_time = data[1].decode()
                date = data[9].decode()
                rtc.datetime = time.struct_time((
                    int(date[4:6])+2000, int(date[2:4]), int(date[0:2]),
                    int(utc_time[0:2]), int(utc_time[2:4]), int(utc_time[4:6]),
                    0, -1, -1))
                return True
        elif line.startswith(b'$GPGGA'):
            data = line.split(b',')
            if data[6] != b'0':
                lat = float(data[2][:2]) + float(data[2][2:])/60
                if data[3] == b'S': lat = -lat
                lon = float(data[4][:3]) + float(data[4][3:])/60
                if data[5] == b'W': lon = -lon
                current_grid = to_maidenhead(lat, lon)
                gps_lock = True
                return True
    return False

def to_maidenhead(lat, lon):
    lon += 180
    lat += 90
    return f"{chr(65+int(lon/20))}{chr(65+int(lat/10))}" \
           f"{int(lon%20/2)}{int(lat%10)}"

# ================== OLED显示 ==================
def update_display():
    oled.fill(0)
    oled.text(f"Freq: {sys_config['freq'][:6]}MHz", 0, 0)
    oled.text(f"{sys_config['callsign']} {tx_status}", 0, 12)
    oled.text(f"GPS: {'LOCK' if gps_lock else '----'}", 0, 24)
    oled.text(f"Grid: {current_grid}", 0, 36)
    t = rtc.datetime
    oled.text(f"UTC: {t.tm_hour:02}:{t.tm_min:02}:{t.tm_sec:02}", 0, 48)
    oled.show()

# ================== WSPR编码 ==================
def wspr_encode(callsign, grid, power):
    # --- 参数验证 ---
    if len(grid) != 4 or not grid[0].isalpha() or not grid[1].isalpha() \
       or not grid[2].isdigit() or not grid[3].isdigit():
        raise ValueError("无效的网格坐标")
    
    # --- 呼号编码 ---
    def encode_callsign(call):
        if call.count('/') > 1:
            raise ValueError("呼号格式错误")
        
        if '/' in call:
            prefix, suffix = call.split('/')
            if len(prefix) > 3 or len(suffix) > 3:
                raise ValueError("特殊呼号格式错误")
            return _encode_packed_call(prefix, True) << 18 | _encode_packed_call(suffix, False)
        else:
            return _encode_packed_call(call, False)
    
    def _encode_packed_call(call, is_prefix):
        if call[0].isdigit():
            raise ValueError("呼号首字符不能为数字")
        
        code = 0
        for i, c in enumerate(call.upper()):
            if c == ' ':
                val = 36
            elif c.isdigit():
                val = int(c)
            elif c.isalpha():
                val = ord(c) - ord('A') + 10
            else:
                raise ValueError(f"无效字符: {c}")
            
            code = code * 37 + val
        
        for _ in range(3 - len(call)):
            code = code * 37 + 36  # 填充空格
        
        if is_prefix:
            code += 1000 * 37**3  # 前缀标识
        return code

    # --- 网格和功率编码 ---
    def encode_grid_power(grid, power):
        lat = (ord(grid[1].upper()) - ord('A')) * 10 + int(grid[3])
        lon = (ord(grid[0].upper()) - ord('A')) * 20 + int(grid[2]) * 2
        pwr = [0, 3, 7, 10, 13, 17, 20, 23, 
               27, 30, 33, 37, 40, 43, 47, 50,
               53, 57, 60].index(power)
        return (lat << 12) | (lon << 4) | pwr

    # --- 主编码流程 ---
    # 1. 生成信息位
    call_code = encode_callsign(callsign)
    grid_pwr_code = encode_grid_power(grid, power)
    data = (call_code << 32) | grid_pwr_code
    bits = [(data >> (49 - i)) & 1 for i in range(50)]

    # 2. 卷积编码 (K=32, r=1/2)
    G1 = 0xf2d05351
    G2 = 0xe4613c47
    reg = 0
    coded_bits = []
    for b in bits:
        reg = (reg << 1) | b
        for _ in range(2):  # 编码率1/2
            c = 0
            for i in range(32):
                if (reg >> i) & 1:
                    c ^= (G1 >> (31 - i)) & 1
                    c ^= (G2 >> (31 - i)) & 1
            coded_bits.append((c & 1))
    
    # 3. 交织
    interleave_order = [
        0, 31, 62, 28, 60, 24, 56, 16, 48, 8, 40, 1, 33, 63, 30, 58,
        26, 54, 18, 50, 10, 44, 3, 35, 61, 29, 57, 17, 49, 9, 41, 2,
        34, 59, 27, 55, 19, 51, 11, 45, 4, 36, 25, 52, 20, 53, 21, 37,
        5, 46, 6, 38, 22, 43, 7, 39, 23, 47, 15, 42, 14, 51, 13, 35
    ]
    interleaved = [coded_bits[i] for i in interleave_order[:162]]

    # 4. 插入同步向量
    sync_vector = [
        1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,
        0,0,0,0,1,0,0,1,0,1,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
        1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,1,0,0,0,1,
        1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,1,0,1,0,0,0,0,0,0,0,1,1,0,
        1,0,1,0,0,0,1,1,0,0,0,0,0,0,1,0,1,0,0,1,0,0,0,1,1,0,0,0,0,0
    ]
    symbols = []
    for i in range(162):
        sym = interleaved[i] ^ sync_vector[i]
        symbols.append(sym * 2 + (interleaved[i] & 1))

    # 5. 符号映射到频移
    fsk_symbols = []
    for s in symbols:
        if s == 0:   # +0.0Hz
            fsk_symbols.append(0)
        elif s == 1: # +1.4648Hz
            fsk_symbols.append(1)
        elif s == 2: # -1.4648Hz
            fsk_symbols.append(2)
        elif s == 3: # +0.0Hz (占位)
            fsk_symbols.append(3)
    
    return fsk_symbols[:162]  # 确保返回162个符号

# ================== 发射控制 ==================
def transmit():
    global tx_status
    tx_status = "TX"
    pixel.fill((0, 50, 0))
    
    symbols = wspr_encode(
        sys_config['callsign'],
        current_grid,
        sys_config['power']
    )
    
    base_freq = int(sys_config['freq'])
    for sym in symbols:
        si.set_frequency(0, base_freq + sym * 146)
        time.sleep(0.682)
    
    pixel.fill((0, 0, 0))
    tx_status = "IDLE"

# ================== 命令行接口 ==================
def process_command(cmd):
    cmd = cmd.strip().lower()
    
    if cmd == 'help':
        return """命令列表:
status    - 系统状态
config    - 显示配置
set <k>=<v> - 修改配置
reboot    - 重启
gps       - 强制GPS更新
"""
    
    elif cmd == 'status':
        return f"""系统状态:
温度: {microcontroller.cpu.temperature:.1f}C
存储: {os.statvfs('/')[0]*os.statvfs('/')[3]}B
GPS: {'锁定' if gps_lock else '未锁定'}"""
    
    elif cmd == 'config':
        return '\n'.join([f"{k}: {v}" for k,v in sys_config.items()])
    
    elif cmd.startswith('set '):
        try:
            k, v = cmd[4:].split('=', 1)
            sys_config[k] = v
            save_config(sys_config)
            return f"{k} 已更新"
        except:
            return "参数错误"
    
    elif cmd == 'gps':
        parse_gps()
        return "GPS已更新"
    
    elif cmd == 'reboot':
        microcontroller.reset()
    
    else:
        return "未知命令"

# ================== 主程序 ==================
sys_config = load_config()
pixel.fill((50, 0, 0))

while True:
    # 处理串口命令
    if supervisor.runtime.serial_bytes_available:
        cmd = input().strip()
        resp = process_command(cmd)
        print(resp)
    
    # GPS处理
    parse_gps()
    
    # 发射定时
    t = rtc.datetime
    if t.tm_sec == 0 and t.tm_min % 2 == 0:
        transmit()
        time.sleep(120)  # 防止重复发射
    
    # 更新显示
    update_display()
    time.sleep(0.5)