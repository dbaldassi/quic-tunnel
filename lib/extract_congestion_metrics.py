#!/usr/bin/python3

import sys
import json
import traceback

file_name = ''
data = ''

if(len(sys.argv) == 2):
    file_name = sys.argv[1]
else:
    print("Error, you must provide a filename")
    exit()

try:
    with open(file_name) as json_file:
       data = json.load(json_file)
except:
    exit()

# root -> traces[0] -> events[]
## data -> name: "recovery:metrics_updated:"
### 

f_cwnd = open('quic_cwnd.csv', "w")
f_rtt = open('quic_rtt.csv', "w")
time_0 = -1

for trace in data["traces"]:
    for event in trace["events"]:
        if event["name"] != "recovery:metrics_updated":
            continue

        try:
            if(time_0 == -1):
                time_0 = event['time']

            time = event['time'] - time_0
                
            d = event["data"]            
            line = ','.join((str(time), str(d['bytes_in_flight']), str(d['congestion_window'])))
            f_cwnd.write(line + '\n')
        except:
            # traceback.print_exc()
            pass
        
        try:
            if(time_0 == -1):
                time_0 = event['time']

            time = event['time'] - time_0
                
            d = event["data"]            
            line = ','.join((str(time), str(d['latest_rtt']), str(d['min_rtt']), str(d['smoothed_rtt']), str(d['ack_delay'])))
            f_rtt.write(line + '\n')
        except:
            # traceback.print_exc()
            pass

f_cwnd.close()
f_rtt.close()
