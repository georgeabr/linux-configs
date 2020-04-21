#!/bin/bash
# NetworkManager does not remember wireless passwords in sway
# To connect to wireless in sway, without help from gnome-keyring or kde equivalent
# You need wpa_supplicant, dhclient installed
# first, run below as root, not sudo:
# # wpa_passphrase MYSSID passphrase > /etc/wpa_supplicant.conf
#BTHub6-RXC3-5
sudo systemctl stop NetworkManager
sudo dhclient wlp5s0 -r
sudo killall wpa_supplicant
sudo -b wpa_supplicant -c /etc/wpa_supplicant.conf -i wlp5s0
sudo dhclient wlp5s0
