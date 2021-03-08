#!/bin/bash

# sends the TS to the RPi only
# starts 4 local VLC windows, but sends H.265 only to VLC1 and VLC3

# First kill any previous instances of Winterhill

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


cd /home/pi/winterhill/RPi-3v20
./winterhill-local-3v20.sh 127.0.0.1 9900 0

