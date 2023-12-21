import matplotlib.pyplot as plt
import numpy as np

def plot1():
    # 数据
    #packet_loss = [0, 0.01, 0.05, 0.1]
    packet_loss = [0, 0.01, 0.25]  # loss in ack
    time_data = [
        [18.7745, 22.8837, 27.3084],  # SR-4
        [19.3666, 20.0992, 19.839],   # GBN-4
        [98929.3, 81165, 68014.1],    # SR-4
        [95905.1, 92409.4, 93621.3],  # GBN-4

        # 时间
        # [13.0855, 20.5847, 39.4542, 65.203],      # Stop-and-Wait
        # [10.7758, 18.8082, 43.6927, 96.15],       # GBN-4
        # [9.08386, 21.1398, 58.6449, 277.037],     # GBN-8
        # [8.88086, 23.6618, 171.339, 705.231],     # GBN-16
        # [10.603, 19.001, 34.1657, 59.0788],         # SR-4
        # [8.84244, 18.1393, 31.4637, 51.9151],       # SR-8
        # [8.87745, 16.0463, 25.0965, 26.4301],       # SR-16

        # 吞吐率
        # [141940, 90229.78231, 47076.17947, 28485.7],      # Stop-and-Wait
        # [172363, 98752.29953, 42509.4581, 19317.2],       # GBN-4
        # [204467, 87860.48118, 31671.17686, 6704.34],      # GBN-8
        # [209141, 78495.84562, 10840.22318, 2045.77],      # GBN-16
        # [175173, 97750.2763, 54363.0893, 31438.6],          # SR-4
        # [210050, 102393.863, 59031.61421, 35776.8],         # SR-8
        # [209222, 115749.6121, 74008.44739, 70274.1],        # SR-16
    ]

    # 创建画布和子图
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    colors = ['purple', 'pink']  # 定义颜色列表

    # 绘制时间变化的折线图
    ax1.set_xlabel('Packet Loss')
    ax1.set_ylabel('Time (s)')
    for i in range(2):
        ax1.plot(packet_loss, time_data[i], marker='o', color=colors[i], label=f'{["SR-4", "GBN-4"][i]}')
    ax1.legend()
    ax1.set_title('Time Variation')

    # 绘制吞吐率变化的柱状图
    bar_width = 0.2  # 柱状图的宽度
    index = np.arange(len(packet_loss))  # 横坐标位置
    ax2.set_xlabel('Packet Loss')
    ax2.set_ylabel('Throughput (byte/s)')
    for i in range(2):
        ax2.bar(index + i * bar_width, time_data[i+2], width=bar_width, color=colors[i], label=f'{["SR-4", "GBN-4"][i]}', alpha=0.5)
    ax2.set_xticks(index + bar_width * 1.5)
    ax2.set_xticklabels(packet_loss)
    ax2.legend()
    ax2.set_title('Throughput Variation')

def plot2():
    # 新数据
    delay = [0, 3, 10, 50, 100]
    time_data = [
        #[10.0938, 20.5847, 23.2214, 40.3672, 63.2744],   # Stop-and-Wait
        [6.83888, 18.8082, 22.6763, 41.5457, 63.688],    # GBN-4
        [6.57671, 19.001, 22.6937, 39.1142, 59.2775],   # SR-4
        #[191228, 90229.78231, 80894.1707, 46011.5, 29353.9],   # Stop-and-Wait
        [271587, 98752.29953, 81907.23354, 44706.2, 29163.3],   # GBN-4
        [282414, 97750.2763, 81844.4326, 47485.4, 31333.2]    # SR-4
    ]

    # 创建画布和子图
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    colors = ['red', 'blue']  # 定义颜色列表

    # 绘制时间变化的折线图
    ax1.set_xlabel('Delay')
    ax1.set_ylabel('Time (s)')
    for i in range(2):
        ax1.plot(delay, time_data[i], marker='o', color=colors[i], label=f'{["GBN-4", "SR-4"][i]}')
    ax1.legend()
    ax1.set_title('Time Variation')

    # 绘制吞吐率变化的柱状图
    bar_width = 0.2  # 柱状图的宽度
    index = np.arange(len(delay))  # 横坐标位置
    ax2.set_xlabel('Delay')
    ax2.set_ylabel('Throughput (byte/s)')
    for i in range(2):
        ax2.bar(index + i * bar_width, time_data[i+2], width=bar_width, color=colors[i], label=f'{["GBN-4", "SR-4"][i]}', alpha=0.5)
    ax2.set_xticks(index + bar_width * 1.5)
    ax2.set_xticklabels(delay)
    ax2.legend()
    ax2.set_title('Throughput Variation')


plot1()
# 调整布局
plt.tight_layout()
# 显示图表
plt.show()
