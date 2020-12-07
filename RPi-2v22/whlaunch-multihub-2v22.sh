#!/bin/bash

# whlaunch-multihub.sh

# sends the TS to a multicast address
# does not start any local VLC windows
# the final zero is the interface IP address which should be set if the RPi has more than one network interface

# sends the TS to the RPi only
# starts 4 local VLC windows, but sends H.265 only to VLC1 and VLC3

# First kill any previous instancess of Winterhill

pgrep winterhill
WHRUNS=$?

while [ $WHRUNS = 0 ]
do
  PID=$(pgrep winterhill | head -n 1)

  echo $PID

  sudo kill "$PID"

  sleep 1

  PID=$(pgrep winterhill | head -n 1)

  echo $PID

  sudo kill -9 "$PID"

  pgrep winterhill
  WHRUNS=$?
done

# Now launch
cd /home/pi/winterhill/RPi-2v22/

./winterhill-multihub-2v22.sh 230.0.0.230 9900 0

