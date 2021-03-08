#!/bin/bash

# whlaunch-multihub-3v20.sh

# sends the TS to a multicast address
# does not start any local VLC windows
# the final zero is the interface IP address which should be set if the RPi has more than one network interface

./winterhill-multihub-3v20.sh 230.0.0.230 9900 0

