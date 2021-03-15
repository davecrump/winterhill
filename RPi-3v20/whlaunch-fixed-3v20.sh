#!/bin/bash

whlaunch-fixed-3v20.sh

# sends the TS to a fixed address
# no VLC windows are started on the RPi
# the final zero is the interface IP address which should be set if the RPi has more than one network interface

cd /home/pi/winterhill/RPi-3v20
./winterhill-fixed-3v20.sh 192.168.77.237 9900 0

