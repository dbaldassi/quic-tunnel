#!/bin/python3

import matplotlib.pyplot as plt
import csv
import sys

time = []
rtc_receive_stats = []
quic_server_sent = []
quic_server_time = []
total_server_sent = []
scp_transfer = []
scp_transfer_time = []
link = []

# params
show_limit = True

if len(sys.argv) >= 2:
    fig_name = sys.argv[1]
else:
    fig_name = "result"

# Parse csv file

## parse time and target bitrate
with open('bitrate.csv', 'r') as csvfile:
    lines = csv.reader(csvfile, delimiter=',')

    for row in lines:
        # -- Parse timestamp and target
        time.append(float(row[0]))
        rtc_receive_stats.append(int(row[1]))
        link.append(int(row[2]))
        
    # Convert timestamp in seconds (120 seconds long)
    # t0 = time[0]
    # time = [float(t - t0)/float(1000) for t in time]

try:
    with open('quic.csv', 'r') as csvfile:
        lines = csv.reader(csvfile, delimiter=',')
        for row in lines:
            quic_server_time.append(float(row[0]))
            quic_server_sent.append(float(row[1]) * 8. / 1000.)
except:
    pass

try:
    with open('file.csv', 'r') as csvfile:
        lines = csv.reader(csvfile, delimiter=',')
        for row in lines:
            scp_transfer_time.append(float(row[0]))
            scp_transfer.append(float(row[1]) * 8. / 1000.)
except:
    pass
    
# Plot results

if(show_limit):
    plt.plot(time, link, label = "link", color = 'k')

plt.plot(time, rtc_receive_stats, color = 'r', label = "video (webrtc getstats)")
# plt.plot(

plt.legend()
plt.grid()

if len(sys.argv) == 3 and sys.argv[2] == 'save':
    plt.savefig(fig_name + '.png')
else:
    plt.show()
