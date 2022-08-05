#!/usr/bin/python3

import os
import sys
import time

file_name = sys.argv[1]
print(file_name)

last_timestamp = time.time()
last_length = 0 
sec=0

file1 = open(file_name, "w")

for line in sys.stdin:
    # print("-----------------------------------------------------------")
    # print(line)
    timestamp = time.time()
    args = line.split(' ')
    index = args.index('length')
    length = args[index+1]
    length = int(length[:len(length)-1])

    last_length += length
    elapsed = timestamp - last_timestamp
    
    if(elapsed >= 1):
        bitrate = last_length / (timestamp - last_timestamp)
        last_timestamp = timestamp
        last_length = 0
        sec += elapsed
        file1.write(str(sec) + "," + str(bitrate) + "\n")
        file1.flush()

file1.close()
