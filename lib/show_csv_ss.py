#!/bin/python3

import matplotlib.pyplot as plt
import csv

x = []
y = []
z = []
r = []
b = []

with open('sender-ss.csv', 'r') as csvfile:
    lines = csv.reader(csvfile, delimiter=',')
    for row in lines:
        if(len(row) < 8):
            continue
        x.append(float(row[0]))
        y.append(int(row[5]))
        r.append(float(row[4]))
        b.append(int(row[7]))
        
        zz = int(row[6])
        if zz < 100:
            z.append(zz)
        else:
            z.append(0)
            
    m = x[0]
    # div = x[-1] - x[0]
    x = [i - x[0] for i in x]

px = 1/plt.rcParams['figure.dpi']  # pixel in inches
plt.figure(figsize=(1920*px, 1080*px))

plt.subplot(211)

plt.plot(x, y, color = 'g', linestyle = 'solid',
         marker = '.',label = "cwnd")
plt.plot(x, z, color = 'r', linestyle = 'solid',
         marker = '.',label = "sstresh")
plt.plot(x, b, color = 'b', linestyle = 'solid',
         marker = '.',label = "bytes in flight")

plt.title('ss report', fontsize = 20)
plt.grid()
plt.legend()

plt.subplot(212)
plt.plot(x, r, color = 'orange', linestyle = 'solid',
         marker = '.',label = "rtt")

plt.grid()
plt.legend()
# plt.show()  

plt.savefig('result_tcp.png')
