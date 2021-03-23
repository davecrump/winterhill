#!/bin/bash

# Used to check the PICS are programmed and set the startup behaviour on boot

#### PIC Programming ####

# Check whether programmed and, if not, program them

cd /home/pi/winterhill/whsource-3v20/whpicprog-3v20
sudo ./whpicprog-3v20 --check >/dev/null 2>/dev/null
PIC_RETURN=$?
if [ "$PIC_RETURN" != 34 ] && [ "$PIC_RETURN" != 18 ]; then       # Not programmed correctly
  echo Attempting to Program both PICs
  sudo ./whpicprog-3v20 whpic-3v20.hex
  echo Complete, starting WinterHill
  echo
  sleep 1
fi

cd /home/pi

#### Now Start WinterHill as set in the winterhill.ini file ####

grep -q 'BOOT = local' winterhill/winterhill.ini
if [ $? == 0 ]; then
  lxterminal -t "whlaunch-local-3v20.sh" --working-directory=/home/pi/winterhill/RPi-3v20/ -e ./whlaunch-local-3v20.sh
else
  grep -q 'BOOT = anyhub' winterhill/winterhill.ini
  if [ $? == 0 ]; then
    lxterminal -t "whlaunch-anyhub-3v20.sh" --working-directory=/home/pi/winterhill/RPi-3v20/ -e ./whlaunch-anyhub-3v20.sh
  else
    grep -q 'BOOT = anywhere' winterhill/winterhill.ini
    if [ $? == 0 ]; then
      lxterminal -t "whlaunch-anywhere-3v20.sh" --working-directory=/home/pi/winterhill/RPi-3v20/ -e ./whlaunch-anywhere-3v20.sh
    else
      grep -q 'BOOT = multihub' winterhill/winterhill.ini
      if [ $? == 0 ]; then
        lxterminal -t "whlaunch-multihub-3v20.sh" --working-directory=/home/pi/winterhill/RPi-3v20/ -e ./whlaunch-multihub-3v20.sh
      else
        grep -q 'BOOT = fixed' winterhill/winterhill.ini
        if [ $? == 0 ]; then
          lxterminal -t "whlaunch-fixed-3v20.sh" --working-directory=/home/pi/winterhill/RPi-3v20/ -e ./whlaunch-fixed-3v20.sh
        fi
      fi
    fi
  fi
fi

exit

