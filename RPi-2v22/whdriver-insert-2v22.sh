#!/bin/bash

echo "Inserting the whdriver-2v22.ko WinterHill device driver module into the kernel"
echo "ls /dev   will show the device"
sudo insmod whdriver-2v22.ko
ls -l /dev/whdriver-2v22
sleep 5s

