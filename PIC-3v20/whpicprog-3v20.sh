#!/bin/bash

cd /home/pi/winterhill/whsource-3v20/whpicprog-3v20

sudo ./whpicprog-3v20 --check
PIC_RETURN=$?

if [ "$PIC_RETURN" == 34 ]; then
  echo Both PICs Programmed with software 3v20
  echo No Action Required
  echo
  read -n 1 -s -r -p "Press any key to continue"
  exit
fi

if [ "$PIC_RETURN" == 18 ]; then
  echo PIC A Programmed with software 3v20
  echo PIC B not fitted
  echo No Action Required
  echo
  read -n 1 -s -r -p "Press any key to continue"
  exit
fi

echo Attempting to Program both PICs

sudo ./whpicprog-3v20 whpic-3v20.hex

echo Complete
echo
read -n 1 -s -r -p "Press any key to continue"
exit

