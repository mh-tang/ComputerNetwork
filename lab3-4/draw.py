import matplotlib.pyplot as plt
import numpy as np

def plot1():
    # 数据
    packet_loss = [0, 0.01, 0.05, 0.1]
    time_data = [
        [13.0855, 20.5847, 39.4542, 65.203],   # Stop-and-Wait
        [10.7758, 18.8082, 43.6927, 96.15],    # GBN-4
        [10.603, 19.001, 34.1657, 59.0788],   # SR-4
        [141940, 90229.78231, 47076.17947, 28485.7],   # Stop-and-Wait
        [172363, 98752.29953, 42509.4581, 19317.2],   # GBN-4
        [175173, 97750.2763, 54363.0893, 31438.6]    # SR-4
    ]

    # 创建画布和子图
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

    # 绘制时间变化的折线图
    ax1.set_xlabel('Packet Loss')
    ax1.set_ylabel('Time (s)')
    for i in range(3):
        ax1.plot(packet_loss, time_data[i], marker='o', label=f'{["Stop-and-Wait", "GBN-4", "SR-4"][i]}')
    ax1.legend()
    ax1.set_title('Time Variation')

    # 绘制吞吐率变化的柱状图
    bar_width = 0.2  # 柱状图的宽度
    index = np.arange(len(packet_loss))  # 横坐标位置
    ax2.set_xlabel('Packet Loss')
    ax2.set_ylabel('Throughput (byte/s)')
    for i in range(3):
        ax2.bar(index + i * bar_width, time_data[i+3], width=bar_width, label=f'{["Stop-and-Wait", "GBN-4", "SR-4"][i]}', alpha=0.5)
    ax2.set_xticks(index + bar_width * 1.5)
    ax2.set_xticklabels(packet_loss)
    ax2.legend()
    ax2.set_title('Throughput Variation')

def plot2():
    # 新数据
    delay = [0, 3, 10, 50, 100]
    time_data = [
        [10.0938, 20.5847, 23.2214, 40.3672, 63.2744],   # Stop-and-Wait
        [6.83888, 18.8082, 22.6763, 41.5457, 63.688],    # GBN-4
        [6.57671, 19.001, 22.6937, 39.1142, 59.2775],   # SR-4
        [191228, 90229.78231, 80894.1707, 46011.5, 29353.9],   # Stop-and-Wait
        [271587, 98752.29953, 81907.23354, 44706.2, 29163.3],   # GBN-4
        [282414, 97750.2763, 81844.4326, 47485.4, 31333.2]    # SR-4
    ]

    # 创建画布和子图
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

    # 绘制时间变化的折线图
    ax1.set_xlabel('Delay')
    ax1.set_ylabel('Time (s)')
    for i in range(3):
        ax1.plot(delay, time_data[i], marker='o', label=f'{["Stop-and-Wait", "GBN-4", "SR-4"][i]}')
    ax1.legend()
    ax1.set_title('Time Variation')

    # 绘制吞吐率变化的柱状图
    bar_width = 0.2  # 柱状图的宽度
    index = np.arange(len(delay))  # 横坐标位置
    ax2.set_xlabel('Delay')
    ax2.set_ylabel('Throughput (byte/s)')
    for i in range(3):
        ax2.bar(index + i * bar_width, time_data[i+3], width=bar_width, label=f'{["Stop-and-Wait", "GBN-4", "SR-4"][i]}', alpha=0.5)
    ax2.set_xticks(index + bar_width * 1.5)
    ax2.set_xticklabels(delay)
    ax2.legend()
    ax2.set_title('Throughput Variation')


plot2()

# 调整布局
plt.tight_layout()
# 显示图表
plt.show()
