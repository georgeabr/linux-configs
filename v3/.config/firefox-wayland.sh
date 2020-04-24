#!/bin/bash
# download and extract firefox beta to Downloads directory
# copy this script to /usr/bin
gsettings set org.gnome.desktop.interface cursor-size 32
gsettings set org.gnome.desktop.interface cursor-blink false
MOZ_ENABLE_WAYLAND=1 ~/Downloads/firefox/firefox