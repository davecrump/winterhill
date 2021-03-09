![winterhill banner](/configs/WH_Title.jpg)
# winterhill
Production Build for the WinterHill Multi-channel DATV Receiver based on a Raspberry Pi 4

# Installation

First prepare the raspios Buster build based on version 2021-01-11.  Add a file called ssh in the /boot volume.  Then modify as in the detailed instructions. 

The preferred installation method only needs a Windows PC connected to the same (internet-connected) network as your Raspberry Pi.

- From your Windows PC use Putty (http://www.chiark.greenend.org.uk/~sgtatham/putty/download.html) to log in to the Raspberry Pi.  You will get a Security warning the first time you try; this is normal.

- Log in (user: pi, password: raspberry) then cut and paste the following code in, one line at a time:

```sh
wget https://raw.githubusercontent.com/BritishAmateurTelevisionClub/winterhill/main/install_winterhill.sh
chmod +x install_winterhill.sh
./install_winterhill.sh
```
After reboot, resume the detailed instructions.

# Advanced notes

To load the development version, cut and paste in the following lines:

```sh
wget https://raw.githubusercontent.com/davecrump//winterhill/main/install_winterhill.sh
chmod +x install_winterhill.sh
./install_winterhill.sh -d
```
