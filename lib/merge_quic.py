#!/usr/bin/python3

import csv
import sys

stats = []

sys.argv.pop(0)

for filename in sys.argv:
    print(filename)
    
    with open(filename, 'r') as csvfile:
        lines = csv.reader(csvfile, delimiter=',')
        
        for row in lines:
            index = int(row[0])
            while(index >= len(stats)):
                stats.append([])
                
            if(len(stats[index]) == 0):
                stats[index].append(row[0])
                    
            stats[index].append(row[1])

resultfile = open('bitrate_merged.csv', 'w')

for s in stats:
    if(len(s) > 0):
        resultfile.write(','.join(s) + "\n")
        resultfile.flush()

resultfile.close()
