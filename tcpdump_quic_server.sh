#!/bin/bash

# capture outgoing trafic of quic server
tcpdump -i nflog:2 -l -e -n | ./tcpdumpbitrate.py quic.csv
