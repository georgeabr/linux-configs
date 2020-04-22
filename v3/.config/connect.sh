#!/bin/bash
# NetworkManager does not remember wireless passwords in sway
# To connect to _wireless_ in sway, without help from gnome-keyring or kde equivalent
# You need wpa_supplicant, dhclient installed
# first, run below as root, not sudo:
# # wpa_passphrase MYSSID passphrase > /etc/wpa_supplicant.conf
#BTHub6-RXC3-5

if [ $# -lt 1 ]
then
        echo "Usage : sudo $0 wired    - connect to wired network"
        echo "Usage : sudo $0 wifi     - connect to wifi netwwork"
        exit

fi

# echo $1
# set wifi/wired devices
wifi_device="wlp5s0"
wired_device="enp4s0"

connect_wifi()
{
	printf "Connecting to _wifi_ network\n"
	printf "============================\n"
	sudo systemctl stop NetworkManager
	sudo dhclient $wifi_device -r
	sudo dhclient $wired_device -r
	sudo killall wpa_supplicant
	sudo -b wpa_supplicant -c /etc/wpa_supplicant.conf -i $wifi_device
	sudo dhclient $wifi_device
	printf "\n"
}

connect_wired()
{
	printf "Connecting to _wired_ network\n"
	printf "=============================\n"
	sudo systemctl stop NetworkManager
	sudo dhclient $wifi_device -r
	sudo dhclient $wired_device -r
	sudo killall wpa_supplicant
	# sudo -b wpa_supplicant -c /etc/wpa_supplicant.conf -i $wifi_device
	sudo dhclient $wired_device
	printf "\n"
}



case "$1" in 
	wired)
		connect_wired; exit
	;;
	wifi)
		connect_wifi; exit
	;;
esac
