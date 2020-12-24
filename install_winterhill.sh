#!/bin/bash

# WinterHill v2.22 install file
# G8GKQ 5 Dec 2020

# Winterhill for a 1920 x 1080 screen
#

whoami | grep -q pi
if [ $? != 0 ]; then
  echo "Install must be performed as user pi"
  exit
fi

cd /home/pi

echo "----------------------------------------------------------"
echo "----- Installing the G8GKQ Build of WinterHill V2.22 -----"
echo "----------------------------------------------------------"
echo

echo "--------------------------------------------------------"
echo "----- Disabling the raspberry ssh password warning -----"
echo "--------------------------------------------------------"
echo
sudo mv /etc/xdg/lxsession/LXDE-pi/sshpwd.sh /etc/xdg/lxsession/LXDE-pi/sshpwd.sh.old

echo "------------------------------------"
echo "---- Loading required packages -----"
echo "------------------------------------"
echo
sudo apt-get -y install xdotool xterm raspberrypi-kernel-headers

echo "--------------------------------------------------------------"
echo "---- Put the Desktop Toolbar at the bottom of the screen -----"
echo "--------------------------------------------------------------"
echo
cd /home/pi/.config/lxpanel/LXDE-pi/panels
sed -i "/^  edge=top/c\  edge=bottom" panel
cd /home/pi

echo "----------------------------------------------------"
echo "---- Increasing gpu memory in /boot/config.txt -----"
echo "----------------------------------------------------"
echo
sudo bash -c 'echo -e "\n##Increase GPU Memory" >> /boot/config.txt'
sudo bash -c 'echo -e "gpu_mem=128\n" >> /boot/config.txt'

echo "-------------------------------------------------"
echo "---- Set force_turbo for constant spi speed -----"
echo "-------------------------------------------------"
echo
sudo bash -c 'echo -e "##Set force_turbo for constant spi speed" >> /boot/config.txt'
sudo bash -c 'echo -e "force_turbo=1\n" >> /boot/config.txt'

echo "--------------------------------------"
echo "---- Set the spi ports correctly -----"
echo "--------------------------------------"
echo
sudo sed -i "/^#dtparam=spi=on/c\dtparam=spi=off\ndtoverlay=spi5-1cs" /boot/config.txt

echo "----------------------------------------------"
echo "---- Setting Framebuffer to 32 bit depth -----"
echo "----------------------------------------------"
echo
sudo sed -i "/^dtoverlay=vc4-fkms-v3d/c\#dtoverlay=vc4-fkms-v3d" /boot/config.txt


echo "-------------------------------------------"
echo "---- Download the WinterHill Software -----"
echo "-------------------------------------------"
echo
cd /home/pi
wget https://github.com/davecrump/winterhill/archive/main.zip
unzip -o main.zip
mv winterhill-main winterhill
rm main.zip

echo "------------------------------------------"
echo "---- Testing spi driver installation -----"
echo "------------------------------------------"
echo
cd /home/pi/winterhill/whdriver-2v22
make
if [ $? != 0 ]; then
  echo failed to build WinterHill Driver
  exit
fi

sudo insmod whdriver-2v22.ko
if [ $? != 0 ]; then
  echo failed to load WinterHill Driver
  exit
fi

cat /proc/modules | grep -q 'whdriver_2v22'
if [ $? != 0 ]; then
  echo failed to load WinterHill Driver
  exit
else
  echo
  echo Succesful driver build and load
fi
cd /home/pi

echo "------------------------------------------------"
echo "---- Set up to load the spi driver at boot -----"
echo "------------------------------------------------"
echo
sudo sed -i "/^exit 0/c\cd /home/pi/winterhill/whdriver-2v22\nsudo insmod whdriver-2v22.ko\nexit 0" /etc/rc.local

echo "--------------------------------------------"
echo "---- Copy the shortcuts to the desktop -----"
echo "--------------------------------------------"
echo
cp /home/pi/winterhill/configs/Kill_WH      /home/pi/Desktop/Kill_WH
cp /home/pi/winterhill/configs/WH_Local     /home/pi/Desktop/WH_Local
cp /home/pi/winterhill/configs/WH_Anyhub    /home/pi/Desktop/WH_Anyhub
cp /home/pi/winterhill/configs/WH_Anywhere  /home/pi/Desktop/WH_Anywhere
cp /home/pi/winterhill/configs/WH_Multicast /home/pi/Desktop/WH_Multicast
cp /home/pi/winterhill/configs/WH_Multihub  /home/pi/Desktop/WH_Multihub
cp /home/pi/winterhill/configs/PIC_A        /home/pi/Desktop/PIC_A
cp /home/pi/winterhill/configs/PIC_B        /home/pi/Desktop/PIC_B

echo "--------------------------------------------------------------------"
echo "---- Enable Autostart for the selected WinterHill mode at boot -----"
echo "--------------------------------------------------------------------"
echo
mkdir /home/pi/.config/autostart
cp /home/pi/winterhill/configs/startup.desktop /home/pi/.config/autostart/startup.desktop

echo
echo "SD Card Serial:"
cat /sys/block/mmcblk0/device/cid

# Reboot
echo
echo "--------------------------------"
echo "----- Complete.  Rebooting -----"
echo "--------------------------------"
sleep 1

sudo reboot now
exit


