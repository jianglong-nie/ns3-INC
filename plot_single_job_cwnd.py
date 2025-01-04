import matplotlib.pyplot as plt
import numpy as np

# 读取4个文件的数据
n0job1_data = np.loadtxt('n0-job1-cwnd.txt')
n1job1_data = np.loadtxt('n1-job1-cwnd.txt')


# 创建图表
plt.figure(figsize=(12, 8))

# 绘制4条线，使用不同的颜色和线型
plt.plot(n0job1_data[:, 0], n0job1_data[:, 1], 'b-', label='n0-job1', linewidth=1)
plt.plot(n1job1_data[:, 0], n1job1_data[:, 1], 'r--', label='n1-job1', linewidth=1)


# 添加网格
plt.grid(True, linestyle='--', alpha=0.7)

# 设置标签和标题
plt.xlabel('Time (s)')
plt.ylabel('Congestion Window Size (bytes)')
plt.title('TCP Congestion Window vs Time')

# 添加图例
plt.legend()

# 保存图片
plt.savefig('job1-cwnd.png', dpi=300, bbox_inches='tight')

# 显示图片
plt.show()