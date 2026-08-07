import serial
import matplotlib.pyplot as plt
from collections import deque
import numpy as np  # 引入numpy用于简单计算

# --- 配置 ---
SERIAL_PORT = 'COM5'  
BAUD_RATE = 921600 
MAX_POINTS = 300      # X轴显示的点数

# --- 初始化 ---
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
except Exception as e:
    print(f"无法打开串口 {SERIAL_PORT}: {e}")
    exit()

data_buffer = deque([0] * MAX_POINTS, maxlen=MAX_POINTS) 

plt.ion() # 开启交互模式
fig, ax = plt.subplots(figsize=(10, 6)) # 设置画布大小
line, = ax.plot(data_buffer, linewidth=0.8) # 线条稍微细一点看起来更清晰

# 【关键修改 1】固定 Y 轴范围，防止画面乱跳
# AD8232的输出通常在 0-1023 (10bit ADC) 或 0-4095 (12bit)
# 根据你的截图，数值大概在 0-100 之间波动，我们设定一个稍大的固定范围
ax.set_ylim(-50, 150) 
ax.set_xlim(0, MAX_POINTS)
ax.set_title("Real-time ECG Waveform (Filtered)")
ax.grid(True, linestyle='--', alpha=0.7) # 虚线网格看起来更专业
ax.set_xlabel("Samples")
ax.set_ylabel("Amplitude")

print("开始接收数据... 按 Ctrl+C 停止")

try:
    while True:
        try:
            if ser.in_waiting > 0:
                raw_data = ser.readline()
                line_str = raw_data.decode('utf-8', errors='ignore').strip()
            
                if line_str.lstrip('-').isdigit(): 
                    value = int(line_str)
                    
                    # 【关键修改 2】简单的异常值剔除 (去毛刺)
                    # 如果新值和上一个值差别太大（比如瞬间跳变50以上），认为是干扰，不予采纳
                    if len(data_buffer) > 0:
                        last_val = data_buffer[-1]
                        if abs(value - last_val) > 60: 
                            value = last_val # 保持上一个值，或者用平均值代替

                    data_buffer.append(value)     
                    line.set_ydata(data_buffer)   
                    
                    # 这里不再动态调整 set_ylim，保持固定窗口让眼睛适应
                    
                    fig.canvas.draw()
                    fig.canvas.flush_events()
                
        except serial.SerialException:
            print("串口连接断开")
            break
        except Exception as e:
            pass
                
except KeyboardInterrupt:
    print("\n停止监测")

finally:
    # 【关键修改 3】确保程序退出时关闭串口，否则下次会报错
    if ser.is_open:
        ser.close()
        print("串口已关闭")