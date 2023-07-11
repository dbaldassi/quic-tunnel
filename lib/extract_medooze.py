#!/usr/bin/python3

import csv
import sys

fb=0
transportSeqNum=1
feedbackNum=2
size=3
sent=4
recv=5
deltaSent=6
deltaRecv=7
delta=8
bwe=9
targetBitrate=10
availableBitrate=1
rtt=12
minrtt=13
mark=14
rtx=15
probing=16

if len(sys.argv) >= 2:
    medooze = sys.argv[1]
else:
    print("Not enough args")
    exit()

def add_value(tab, time, cat, value=1):
    while(time >= len(tab)):
        tab.append([0,0,0,0])

    tab[time][cat] += value
    
try:
    media_acc = 0
    probing_acc = 1
    rtx_acc = 2
    recv_acc = 3

    fbdelay = []
    rttmean = []
    minrttmean = []
    target = []
    
    prev = 0
    count = 0
    
    result = []
    
    with open(medooze, 'r') as csvfile:
        lines = csv.reader(csvfile, delimiter='|')

        for row in lines:
            ts = int(row[4]) // 1000000
            add_value(result, ts, media_acc, int(row[size]) * 8 if row[rtx] == "0" and row[probing] == "0" else 0)
            add_value(result, ts, rtx_acc, int(row[size]) * 8 if row[rtx] == "1" else 0)
            add_value(result, ts, probing_acc, int(row[size]) * 8 if row[probing] == "1" else 0)
            add_value(result, ts, recv_acc, int(row[size]) * 8 if row[sent] != "0" and row[recv] != "0" else 0)
            
            if(prev != row[fb]):
                if(prev != 0):
                    fbdelay[count][0] /= fbdelay[count][1]
                    target[count][0] /= float(target[count][1])
                    minrttmean[count][0] /= float(minrttmean[count][1])
                    rttmean[count][0] /= float(rttmean[count][1])
                    count += 1
            
                prev = row[fb]
                fbdelay.append([0,0])
                target.append([0,0])
                rttmean.append([0,0])
                minrttmean.append([0,0])
                
            fbdelay[count][0] += (float(row[fb]) - float(row[sent])) / 1000.
            fbdelay[count][1] += 1

            target[count][0] += float(row[targetBitrate])
            target[count][1] += 1

            minrttmean[count][0] += float(row[minrtt])
            minrttmean[count][1] += 1

            rttmean[count][0] += float(row[rtt])
            rttmean[count][1] += 1
                           
        f = open("medooze_results.csv", 'w')
        csvfile.seek(0,0)

        prev = 0
        count = 0
        
        for row in lines:
            ts = int(row[4]) // 1000000
            line = ','.join([s for s in row]) + ',' + ','.join([str(s) for s in result[ts]])
            
            if(prev != row[fb]):
                if(prev != 0):
                    count += 1
                prev = row[fb]
                
            line += ',' + ','.join([str(s) for s in [ target[count][0], fbdelay[count][0], rttmean[count][0], minrttmean[count][0] ]])
            
            f.write(line + '\n')
            
            
        f.close()
except:
    print("can't open", medooze)
    traceback.print_exc()
    exit()
