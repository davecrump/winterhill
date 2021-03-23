#!/bin/bash

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

pgrep lxterminal
LXTRUNS=$?

while [ $LXTRUNS = 0 ]
do
  PID=$(pgrep lxterminal | head -n 1)

  echo $PID

  sudo kill "$PID"

  sleep 1

  PID=$(pgrep lxterminal | head -n 1)

  echo $PID

  sudo kill -9 "$PID"

  pgrep lxterminal
  LXTRUNS=$?
done

sudo killall vlc

sleep 1

sudo killall -9 vlc

exit

