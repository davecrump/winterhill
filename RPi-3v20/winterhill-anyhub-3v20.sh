#!/bin/bash

#
#	does not start any local VLC windows
#	positions this window
#	starts winterhill main application and positions it
#
# Usage: ./winterhill-anyhub-3v20.sh IPADDRESS IPPORT IPINTERFACEADDRESS


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

WINTERHILL="winterhill-3v20"
MYNAME=$0
MYLAUNCH="whlaunch-anyhub-3v20.sh"
IPADDRESS=$1
IPPORT=$2
IPINTERFACEADDRESS=$3
WINTERHILLCOMMAND="lxterminal -e ./$WINTERHILL"

# initialise exit request variable set by 'trapit' signal handler

finished=0

# start the main WinterHill application

command="$WINTERHILLCOMMAND $IPADDRESS $IPPORT $IPINTERFACEADDRESS 0 0 0 0"
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
xdotool windowmove --sync $whwindow 260 608
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
xdotool windowmove $whlaunchwindow 260 839 windowsize $whlaunchwindow 1500 130

# idle

while [ $finished -eq 0 ]
do
	sleep 0.5s
done

sudo killall winterhill

