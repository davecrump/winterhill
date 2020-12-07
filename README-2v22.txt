-----------------------------------------------
WinterHill 2v22 Quick Start (1920x1080 display)
-----------------------------------------------


Make whpicprog-2v22, winterhill-2v22 and all .sh files executable


-------------------
Re-program the PICs
-------------------

whpic-2v22.hex				sudo ./whpicprog-2v22 A whpic-2v22.hex
							sudo ./whpicprog-2v22 B whpic-2v22.hex

The 2v22 PIC code is NOT compatible with 2v1x versions of the WH main application, so do not mix versions


----------------------
SPI Port Configuration
----------------------

A loadable kernel module (whdriver-2v22.ko) provides an interrupt driven interface to SPI0 and SPI6
This reduces CPU load and increases throughput compared to 2v1x versions of WH
A throughput of 10M bit/s should be possible per NIM

The SPI ports must be configured in a particular way, or else the interrupts are stolen before whdriver gets them:

	SPI0 must be disabled via RPi Preferences or by editing /boot/config.txt
	SPI6 must not be enabled in /boot/config.txt
	SPI5 must be enabled in /boot/config.txt

/boot/config.txt should look like:

	dtparam=i2c_arm=on
	#dtparam=i2s=on
	dtparam=spi=off
	dtoverlay=spi5-1cs

Reboot after making the changes

ls /dev should show spidev3.0 (sic) as the only SPI device

cat /proc/interrupts should show the SPI5 interrupt handler, fe204a00.spi (IRQ values other than 40 are OK):

	40:          0          0          0          0     GICv2 150 Level     fe204a00.spi

 	
------------------------------------
Inserting the Loadable Kernel Module
------------------------------------

This needs to be done after each boot up
It can go into rc.local eventually

whdriver-insert-2v22.sh		execute in terminal to insert the driver into the kernel
							ls /dev will show the driver; whdriver-2v22
	
Any bugs in whdriver can cause hangups and crashes of the whole system, so expect the unexpected	

Use whdriver-remove-2v22.sh to remove whdriver if required, having first shut down the WH application

The driver can be left installed when running WH 2v1x versions, assuming it is not causing problems

The dmesg command can be used to view messages from kernel drivers. The first time whdriver 
(and possibly other drivers) is removed, dmesg shows lots of complaints, but it doesn't happen 
after a second insertion and removal.

After starting the main WH application, cat /proc/interrupts should show the whdriver interrupt handlers
(Other IRQ numbers on the left are OK)
The whdriver interrupt handlers disappear when the main WH application exits

 40:          0          0          0          0     GICv2 150 Level     fe204a00.spi, whdriver-2v22_spi_handler
 59:          0          0          0          0  pinctrl-bcm2835  10 Edge      whdriver-2v22_picA_handler
 60:          0          0          0          0  pinctrl-bcm2835  20 Edge      whdriver-2v22_picB_handler

				
------------------
Running WinterHill
------------------

whlaunch-local.sh			choose a launcher
							execute in terminal to create VLC windows and start the main application
							VLC1 uses --codec ffmpeg
							H.265 will be displayed only on VLC1 and VLC3

QuickTune					send receive commands to ports 9911-9914

Closing the launch window will close all other windows


-----------------------------
NIM RX Frequency Calibration
-----------------------------

This needs to be done every time the main WH application is started
To see the frequency error for each NIM + LNB, receive the beacon on RX2 and RX4 before doing the calibration
Receive the beacon on RX1 and RX3 to perform the frequency calibration
After the calibration, receiving the beacon on RX2 and RX4 should give frequencies within 1kHz of nominal


------------------------
NIM Tuning Modifications
------------------------

If a NIM has been calibrated on the beacon, the signal search bandwidth is set to +/- SR/2
If there are SR333 signals on .250 and .750, setting the frequency to .500 should not find either of them
If the NIM has not been calibrated, the search bandwidth is +/- 1MHz


----------------
Launcher details
----------------

For hub launchers, no VLC windows are displayed on the RPi
In all cases, receive commands are accepted from any location
In all cases, windows are displayed on the RPi for winterhill-2v22 . . . and whlaunch . . .
If you do not see these, something went wrong
If so, try running sudo ./winterhill-2v22 0 9900 0 0 0 0 0 and see if it complains

whlaunch-anyhub-2v22.sh		the TS is sent to wherever the QT receive command came from
							no VLC windows are displayed on the RPi

whlaunch-anywhere-2v22.sh	the TS is sent to wherever the QT receive command came from
							4 VLC windows are displayed on the RPi
							VLC1 uses --codec ffmpeg
							on the RPi, H.265 can be displayed only on VLC1 and VLC3 

whlaunch-local-2v22.sh		the TS is sent only to the RPi
							4 VLC windows are displayed on the RPi
							VLC1 uses --codec ffmpeg
							H.265 can be displayed only on VLC1 and VLC3 

whlaunch-multicast-2v22.sh	the TS is sent to the multicast address
							VLC1 and VLC3 windows are displayed on the RPi
							VLC1 uses --codec ffmpeg
							on the RPi, H.265 can be displayed on both VLC1 and VLC3 

whlaunch-multihub-2v22.sh	the TS is sent to the multicast address
							no VLC windows are displayed on the RPi

------------------------------------------------------------------------------------------------------------

