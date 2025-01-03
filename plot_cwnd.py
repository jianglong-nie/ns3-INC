import matplotlib.pyplot as plt
import numpy as np

# 读取数据
data = np.loadtxt('my-fifth-cwnd.txt')
time = data[:, 0]
cwnd = data[:, 1]

# 创建图表
plt.figure(figsize=(10, 6))
plt.plot(time, cwnd, 'b-', label='Congestion Window')
plt.grid(True)
plt.xlabel('Time (s)')
plt.ylabel('Congestion Window Size (packets)')
plt.title('TCP Congestion Window vs Time')
plt.legend()

# 保存图片
plt.savefig('my-fifth-cwnd.png')
plt.show()