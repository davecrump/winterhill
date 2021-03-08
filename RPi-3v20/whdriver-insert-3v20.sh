#!/bin/bash

echo "Inserting the whdriver-3v20.ko WinterHill device driver module into the kernel"
echo "ls /dev   will show the device"
sudo insmod whdriver-3v20.ko
ls -l /dev/whdriver-3v20
sleep 5s

