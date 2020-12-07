#!/bin/bash

# Winterhill for a 1920 x 1080 screen
#
#	starts 2 VLC windows for receivers 1 and 3 and positions them
#	positions this window
#	starts winterhill main application and positions it, passing the 4 xdotool window IDs
#
# Usage: ./winterhill-multicst-2v22.sh IPADDRESS IPPORT IPINTERFACEADDRESS


trap trapit SIGINT
trap trapit SIGTERM
trap trapit	SIGHUP

trapit()
{
	finished=1
}

# check for correct number of parameters

if [ $# -ne 3 ]
then
	echo "This program requires 3 parameters"
    echo ""
    sleep 5s
    exit  1
fi                

# get supplied parameters

WINTERHILL="winterhill-2v22"
MYNAME=$0
MYLAUNCH="whlaunch-multicast-2v22.sh"
IPADDRESS=$1
IPPORT=$2
IPINTERFACEADDRESS=$3
WINTERHILLCOMMAND="lxterminal -e ./$WINTERHILL"

# set port numbers for VLC; base + 20 + receiver number

IPPORT1=$(($IPPORT + 21))
IPPORT2=$(($IPPORT + 22))
IPPORT3=$(($IPPORT + 23))
IPPORT4=$(($IPPORT + 24))

# set window coordinates

VLCWIDTH=830
VLCHEIGHT=468

VLCLEFT13=20
VLCLEFT24=$(($VLCLEFT13 + $VLCWIDTH + 10))

VLCTOP12=40
VLCTOP34=$(($VLCTOP12 + $VLCHEIGHT + 40))

# initialise exit request variable set by 'trapit' signal handler

finished=0

# start 2 VLC windows

VLCOPTIONS="--qt-minimal-view --no-qt-video-autoresize --no-video-title-show --no-qt-system-tray"
VLCCODEC="--codec ffmpeg" 

#vlc $VLCOPTIONS 		  "udp://@"$IPADDRESS:$IPPORT1 &
vlc $VLCOPTIONS $VLCCODEC "udp://@"$IPADDRESS:$IPPORT1 &
vlc $VLCOPTIONS           "udp://@"$IPADDRESS:$IPPORT3 &

# wait for each VLC window to appear and put the IP address into the title bar

status=""
while [ "$status" = "" ]
do
    status=$(xdotool search --name $IPPORT1)
    echo "<$status>"
    sleep 0.1s
done
vlcwindow1=$status
xdotool windowfocus --sync $vlcwindow1 
xdotool windowsize --sync $vlcwindow1 $VLCWIDTH $VLCHEIGHT 
xdotool windowmove --sync $vlcwindow1 $VLCLEFT13 $VLCTOP12 
xdotool set_window  --name "1: @$IPADDRESS:$IPPORT1 " $vlcwindow1
xdotool windowmove --sync $vlcwindow1 $VLCLEFT13 $(($VLCTOP12 + 2)) 
# usually one of the windows fails to position correctly without the line above 


status=""
while [ "$status" = "" ]
do
    status=$(xdotool search --name $IPPORT3)
    echo "<$status>"
    sleep 0.1s
done
vlcwindow3=$status
xdotool windowfocus --sync $vlcwindow3 
xdotool windowsize --sync $vlcwindow3 $VLCWIDTH $VLCHEIGHT 
xdotool windowmove --sync $vlcwindow3 $VLCLEFT13 $VLCTOP34 
xdotool set_window --name "3: @$IPADDRESS:$IPPORT3" $vlcwindow3
xdotool windowmove --sync $vlcwindow3 $VLCLEFT13 $(($VLCTOP34 + 2))

# start the main WinterHill application

command="$WINTERHILLCOMMAND $IPADDRESS $IPPORT $IPINTERFACEADDRESS $vlcwindow1 0 $vlcwindow3 0"
sudo $command &

# position the main WinterHill window

status=""
while [ "$status" = "" ]
do
    status=$(xdotool search --name $WINTERHILL)
    echo "<$status>"
    sleep 0.1s
done
whwindow=$status
xdotool windowmove --sync $whwindow 240 580 
xdotool windowsize --sync $whwindow 1500 175 
xdotool set_window --name "$WINTERHILL $IPADDRESS $IPPORT $IPINTERFACEADDRESS" $whwindow 

# position this windows

status=""
while [ "$status" = "" ]
do
    sleep 0.1s
    status=$(xdotool search --name $MYLAUNCH)
    echo "<$status>"
done
whlaunchwindow=$status
xdotool windowmove $whlaunchwindow 240 800 windowsize $whlaunchwindow 1500 175 

# hide most of the whlaunch and winterhill windows

xdotool windowraise $vlcwindow3
xdotool windowraise $vlcwindow1

echo "**********"
echo $vlcwindow1
echo $vlcwindow3
echo "**********"

# idle

while [ $finished -eq 0 ]
do
	sleep 0.5s
done

sudo killall winterhill

