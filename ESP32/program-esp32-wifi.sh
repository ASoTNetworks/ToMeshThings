#!/bin/bash
tty=$1
baudrate=115200

if [ "$EUID" -ne 0 ]
then
	echo "Please run as root"
	exit 1
fi
if [ ! $tty ]
then
	echo "USAGE: $0 device"
	exit 2
fi
if [ ! -c $tty ]
then
	echo "FATAL: cannot open $tty: No such file or directory"
	exit 3
fi

stty -F $tty $baudrate
ssid=$(cat /etc/hostapd/hostapd.conf | grep ssid | awk -F "=" '{ print $2 }' )
pass=$(cat /etc/hostapd/hostapd.conf | grep wpa_passphrase | awk -F "=" '{ print $2 }' )
echo "" > $tty
sleep 5
echo -n $ssid > $tty
sleep 5
echo -n $pass > $tty
