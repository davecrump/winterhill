#!/bin/bash

echo "Removing the whdriver-2v22.ko WinterHill device driver module from the kernel"
sudo rmmod whdriver-2v22.ko
sleep 5s

