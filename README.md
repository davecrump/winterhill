# winterhill
Test Build for a Multi-channel DATV Receiver based on a Raspberry Pi 4

# Installation

First prepare the raspios Buster build based on version 2020-12-02.  The modify as in the detailsed instructions. 

The preferred installation method only needs a Windows PC connected to the same (internet-connected) network as your Raspberry Pi.

- From your windows PC use Putty (http://www.chiark.greenend.org.uk/~sgtatham/putty/download.html) to log in to the IP address that you noted earlier.  You will get a Security warning the first time you try; this is normal.

- Log in (user: pi, password: raspberry) then cut and paste the following code in, one line at a time:

```sh
wget https://raw.githubusercontent.com/davecrump/winterhill/main/install_winterhill.sh
chmod +x install_winterhill.sh
./install_winterhill.sh
```
After reboot, resume the detailed instructions.