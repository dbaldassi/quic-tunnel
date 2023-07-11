#!/usr/bin/python3

import csv
import sys
import traceback
import json
import ndjson

MEDOOZE_SENT_CAT=0
MEDOOZE_LOSS_CAT=1
QLOG_SENT_CAT=2
QLOG_LOSS_CAT=3

def add_value(tab, time, cat, value=1):
    while(time >= len(tab)):
        tab.append([0,0,0,0])

    tab[time][cat] += value

def parse_quicgo_qlog(data, result):
    sent = 0
    lost = 0
    
    for line in data:
        if "time" in line and "name" in line:
            time = int(line["time"] / 1000)
            name = line["name"]

            if name == "transport:packet_sent":
                add_value(result, time, QLOG_SENT_CAT)
                sent += 1
            elif name == "recovery:packet_lost":
                add_value(result, time, QLOG_LOSS_CAT)
                lost += 1

    return (sent, lost)
    
def parse_mvfst_qlog(data, result):
    time_0 = -1

    lost = 0
    total = 0
    
    for trace in data['traces']:
        for event in trace['events']:
            if(time_0 == -1):
                time_0 = event['time']

            t = (event['time'] - time_0) // 1000000
            if event['name'] == "loss:packets_lost":
                d = event["data"]
                lost += d['lost_packets']
            
                add_value(result, t, QLOG_LOSS_CAT, d['lost_packets'])
            elif event['name'] == "transport:packet_sent":
                total += 1
                add_value(result, t, QLOG_SENT_CAT)

    return (total, lost)
            
if len(sys.argv) >= 3:
    medooze = sys.argv[1]
    qlog = sys.argv[2]
else:
    print("Not enough args")
    exit()

medooze_total=0
medooze_lost=0

result = []

try:
    with open(medooze, 'r') as csvfile:
        lines = csv.reader(csvfile, delimiter='|')

        for row in lines:
            ts = int(row[4]) // 1000000
            if(int(row[4]) != 0 and int(row[5]) == 0):
                medooze_lost += 1
                add_value(result, ts, MEDOOZE_LOSS_CAT)

            medooze_total += 1
            add_value(result, ts, MEDOOZE_SENT_CAT)
            
except:    
    print("can't open", medooze)
    traceback.print_exc()
    exit()

quic_total = 0
quic_lost = 0
    
try:
    with open(qlog, 'r') as jsonfile:
        data = json.load(jsonfile)
        (quic_total, quic_lost) = parse_mvfst_qlog(data, result)

except:
    try:
        with open(qlog, 'r') as ndjsonfile:
            reader = ndjson.reader(ndjsonfile)
            (quic_total, quic_lost) = parse_quicgo_qlog(reader, result)
        
    except:
        print("can't open", qlog)
        traceback.print_exc()
        # exit()

f = open("lost.csv", 'w')
acc = [0,0,0,0]

for i in range(len(result)):
    for j in range(len(acc)):
        acc[j] += result[i][j]
        result[i].append(acc[j])

    line = ','.join([ str(s) for s in result[i] ])
    line = str(i) + ',' + line + '\n'
    
    f.write(line)

f.close()

print("Medooze loss -> total: ", medooze_total, " lost: ", medooze_lost, " percent: ", medooze_lost*100./ medooze_total if medooze_total > 0 else 1)
print("QUIC loss -> total: ", quic_total, " lost: ", quic_lost, " percent: ", quic_lost*100./ quic_total if quic_total > 0 else 1)
