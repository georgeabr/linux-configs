#!/bin/bash

function print_status {

  # The configurations file in ~/.config/sway/config or
  # in ~/.config/i3/config can call this script.
  #
  # You should see changes to the status bar after saving this script.
  # For Sway:
  # If not, do "killall swaybar" and $mod+Shift+c to reload the configuration.
  # For i3:
  # If not, do "killall i3bar" and $mod+Shift+r to restart the i3 (workspaces and windows stay as they were).


  # The abbreviated weekday (e.g., "Sat"), followed by the ISO-formatted
  # date like 2018-10-06 and the time (e.g., 14:01). Check `man date`
  # on how to format time and date.
  date_formatted=$(date "+%a, %e %b, %H:%M")

  # symbolize the time of day (morning, midday, evening, night)
  # h=$(date "+%H")
  # if (($h>=22 || $h<=5)); then
  #    time_of_day_symbol=🌌
  # elif (($h>=17)); then
  #    time_of_day_symbol=🌆
  # elif (($h>=11)); then
  #    time_of_day_symbol=🌞
  # else
  #    time_of_day_symbol=🌄
  #  fi
  # Sun: 🌅 🌄 ☀️  🌞
  # Moon: 🌙 🌑 🌕 🌝 🌜 🌗 🌘 🌚 🌒 🌔 🌛 🌓 🌖
  # City: 🌇 🌃 🌆
  # Stars: 🌟 🌠 🌌

  # "upower --enumerate | grep 'BAT'" gets the battery name (e.g.,
  # "/org/freedesktop/UPower/devices/battery_BAT0") from all power devices.
  # "upower --show-info" prints battery information from which we get
  # the state (such as "charging" or "fully-charged") and the battery's
  # charge percentage. With awk, we cut away the column containing
  # identifiers. tr changes the newline between battery state and
  # the charge percentage to a space. Then, sed removes the trailing space,
  # producing a result like "charging 59%" or "fully-charged 100%".
  battery_info=$(upower --show-info $(upower --enumerate |\
  grep 'BAT') |\
  egrep "state|percentage" |\
  awk '{print $2}' |\
  tr '\n' ' ' |\
  sed 's/ $//'
  )
  # Alternative: Often shows the status as "Unknown" when plugged in
  # battery_info="$(cat /sys/class/power_supply/BAT0/status) $(cat /sys/class/power_supply/BAT0/capacity)%"

  # Get volume and mute status with PulseAudio
  volume=$(pactl list sinks | grep Volume | tail -n2 | head -n1 | awk '{print $5}')
  audio_info=$(pactl list sinks | grep Mute |tail -n1 | awk -v vol="${volume}" '{print $2=="no"? "🔉 " vol : "🔇 " vol}')
#  audio_info=$(pactl list sinks | grep Mute | awk -v vol="${volume}" '{print $2=="no"? "🔉 " vol : "🔇 " vol}')

  # Emojis and characters for the status bar:
  # Electricity: ⚡ 🗲 ↯ ⭍⭍ 🔌
  # Audio: 🔈 🔊 🎧 🎶 🎵 🎤🕨 🕩 🕪 🕫 🕬  🕭 🎙️🎙
  # Circles: 🔵 🔘 ⚫ ⚪ 🔴 ⭕
  # Time: https://stackoverflow.com/questions/5437674/what-unicode-characters-represent-time
  # Mail: ✉ 🖂  🖃  🖄  🖅  🖆  🖹 🖺 🖻 🗅 🗈 🗊 📫 📬 📭 📪 📧 ✉️ 📨 💌 📩 📤 📥 📮 📯 🏤 🏣
  # Folder: 🖿 🗀  🗁  📁 🗂 🗄 🏻🏾🏾🏾🏾🏾🏾🏾🏾🏾
  # Computer: 💻 🖥️ 🖳  🖥🖦  🖮  🖯 🖰 🖱🖨 🖪 🖫 🖬 💾 🖴 🖵 🖶 🖸 💽
  # Network, communication: 📶 🖧  📡 🖁 📱 ☎️  📞 📟
  # Keys and locks: 🗝 🔑 🗝️ 🔐 🔒 🔏 🔓
  # Checkmarks and crosses: ✅ 🗸 🗹 ❎ 🗙
  # Separators: \| ❘ ❙ ❚
  # Misc: 🐧 🗽 💎 💡 ⭐ ↑ ↓ 🌡🕮  🕯🕱 ⚠ 🕵 🕸 ⚙️  🧲 🌐 🌍 🏠 🤖 🧪 🛡️ 🔗 📦 🎁

  # To get signal strength, use iw wlp3s0 link
  # This is empty if we are not connected via WiFi
  # ssid=$(iw wlp3s0 info | grep -Po '(?<=ssid ).*')
  # default_gateway=$(ip route show default | awk '{print $3}' | uniq)
  
  # install inetutils in arch first
  private_ip=$(hostname -I)
  # iface=$(ip route show default | awk '{print $5}' | uniq)
  # network_info="$private_ip→ $default_gateway on $iface $ssid"
  network_info="$private_ip"

  disk_free=$(df -h | awk '$NF == "/" { print $4 }')
  # mem_free=$(awk '/MemFree/ { printf "%3.0fK \n", $2/1024 }' /proc/meminfo)
  mem_free=$(awk '/MemFree/ { printf "%3.0fM \n", $2/1024 }' /proc/meminfo)
  # cpu_load=$(uptime | awk '{print $8}'|sed 's/.$//')
  # cpu_load=$(uptime | awk '{print $8}'|tr  -d ,)
  # awk - print antepenultimate column value, as uptime output varies
  #					      <this>
  # 21:58:22 up  6:27,  1 user,  load average: 0.29, 0.74, 0.81
  #							<this>
  # 09:10:18 up 106 days, 32 min, 2 users, load average: 0.22, 0.41, 0.32
  cpu_load=$(uptime | awk '{print $(NF-2)}'|tr  -d ,)
  # keyboard_layout=$(localectl status|grep Keymap| awk '{print toupper($3)}')
  # use awk and cut to get keyboard layout
  # keyboard_layout=$(swaymsg -t get_inputs | jq '.[0].xkb_active_layout_name'|cut -c2-3|awk '{ print toupper($0) }')
  # use only awk to get the keyboard layout
  # keyboard_layout=$(swaymsg -t get_inputs | jq '.[2].xkb_active_layout_name'|awk '{ print toupper(substr($1,2,2)) }')
  keyboard_layout=$(swaymsg -t get_inputs | jq -r '.[] | select(.identifier == "1:1:AT_Translated_Set_2_keyboard") | .xkb_active_layout_name')
#  echo "🖧 $network_info $audio_info 🔋 $battery_info $time_of_day_symbol $date_formatted"
  echo " $mem_free| 🔲 $cpu_load | 🖴 / - $disk_free | 🖧 $network_info| $audio_info | 🖮 $keyboard_layout | ↯ $battery_info | $date_formatted "
}

# The argument to `sleep` is in seconds
while true; do print_status; sleep 2; done
