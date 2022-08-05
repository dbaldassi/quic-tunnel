#!/bin/bash

# Capture outgoing trafic of file transfer (scp output on port 22)
tcpdump -i nflog:1 -l -e -n | ./tcpdumpbitrate.py file.csv
