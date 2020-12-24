#!/bin/bash

# Called to program PIC B

cd /home/pi/winterhill/PIC-2v22/

sudo ./whpicprog-2v22 B whpic-2v22.hex

read -rsp $'Press any key to continue...\n' -n1 key

exit