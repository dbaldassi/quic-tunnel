#!/bin/bash

function end_in_tunnel
{
    echo "Ending ..."
    cp build/*.qlog /root/log_dir/
}

if [ "$1" == "out" ]
then
    echo "Starting quic server"
    ./build/out-tunnel 
else
    trap end_in_tunnel SIGTERM
    trap end_in_tunnel SIGKILL
    trap end_in_tunnel SIGINT
    echo "Starting quic client"
    ./build/in-tunnel
    echo "AAAAAAAAAAAAAAAH"
fi
