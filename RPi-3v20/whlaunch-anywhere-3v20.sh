#!/bin/bash

# whlaunch-anywhere-3v20.sh

# sends the TS to wherever the command came from
# starts 4 local VLC windows, but sends H.265 only to VLC1 and VLC2
# the final zero is the interface IP address which should be set if the RPi has more than one network interface

cd /home/pi/winterhill/RPi-3v20
./winterhill-anywhere-3v20.sh 0 9900 0

