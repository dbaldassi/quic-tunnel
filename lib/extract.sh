#!/bin/bash

for i in $(find . -name "bitrate.csv")
do
    cd $(dirname "$i")

    echo "-> $PWD"
    extract_medooze.py quic-relay*
    extract_congestion_metrics.py *.qlog
    lost.py quic-relay*.csv *.qlog > lost.txt
    ffmpeg -i bitstream.264 -i quic-relay-*.mp4 -lavfi ssim=stats_file=ssim_logfile.txt -f null - 2>&1| grep "Parsed_ssim" > ssim.txt

    cd - > /dev/null 
done

echo "" > ssim.txt
echo "" > lost.txt

for i in $(find . -name "ssim.txt")
do
    echo "$i $(cat $i | grep All | cut -d" " -f11 | cut -d":" -f2)" >> ssim.txt
done 

for i in $(find . -name "lost.txt")
do
    echo "$i $(cat $i)" >> lost.txt
done 
