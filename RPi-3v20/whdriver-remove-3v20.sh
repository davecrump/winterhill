#!/bin/bash

echo "Removing the whdriver-3v20.ko WinterHill device driver module from the kernel"
sudo rmmod whdriver-3v20.ko
sleep 5s

