![winterhill banner](/configs/WH_Title.jpg)
# WinterHill
Production Build for the WinterHill Multi-channel DATV Receiver based on a Raspberry Pi 4 as described here: https://wiki.batc.org.uk/WinterHill_Receiver_Project

# Installation

The installation procedure is fully described in the Installation Manual that you will find here: https://wiki.batc.org.uk/WinterHill_Receiver_Project#Documentation

The instructions are based on the old Raspios Buster Desktop which can be downloaded from here: https://downloads.raspberrypi.org/raspios_oldstable_armhf/images/raspios_oldstable_armhf-2022-04-07/2022-04-04-raspios-buster-armhf.img.xz .  More recent software versions (Bullseye or Bookworm) will not work.

- Unzip the image (using 7zip as it is a .xz compressed file) and then transfer it to a Micro-SD Card using Win32diskimager https://sourceforge.net/projects/win32diskimager/

- Before you remove the card from your Windows PC, look at the card with windows explorer; the volume should be labeled "boot".  Create a new empty file called ssh in the top-level (root) directory by right-clicking, selecting New, Text Document, and then change the name to ssh (not ssh.txt).  You should get a window warning about changing the filename extension.  Click OK.  If you do not get this warning, you have created a file called ssh.txt and you need to rename it ssh.  IMPORTANT NOTE: by default, Windows (all versions) hides the .txt extension on the ssh file.  To change this, in Windows Explorer, select File, Options, click the View tab, and then untick "Hide extensions for known file types". Then click OK

- Connect a keyboard, mouse and HDMI screen to your Rasberry Pi, boot it up and follow the detailed instructions in the Installation Manual.  The script below will not work unless you have followed the instructions in the installation manual first.

- When called for in the instructions, from your Windows PC use Putty (http://www.chiark.greenend.org.uk/~sgtatham/putty/download.html) to log in to the Raspberry Pi.  You will get a Security warning the first time you try; this is normal.

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
