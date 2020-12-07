#!/bin/bash

# whlaunch-anyhub.sh

# sends the TS to wherever the command came from
# does not start any local VLC windows
# the final zero is the interface IP address which should be set if the RPi has more than one network interface

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


./winterhill-anyhub-2v22.sh 0 9900 0

