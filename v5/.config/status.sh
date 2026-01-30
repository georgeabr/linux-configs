#!/usr/bin/env bash
set -u

# --- Config ---
NET_INTERFACE="wlp59s0"
NET_INTERVAL=2
CPU_PREV_FILE="/tmp/cpu_prev_state"
NET_PREV_FILE="/tmp/net_prev_${NET_INTERFACE}"
NET_LOCK_FILE="${NET_PREV_FILE}.lock"

# Open lock FD once
exec 200>"$NET_LOCK_FILE"

# --- CPU usage ---
cpu_usage_raw() {
    local cpu_label u n s i w x y z rest
    if ! read -r cpu_label u n s i w x y z rest < /proc/stat 2>/dev/null; then
        printf "  0%%"
        return
    fi

    local cpu_total=$((u + n + s + i + w + x + y + z))
    local cpu_active=$((cpu_total - i))
    local cpu_usage="  0%%"

    if [ -f "$CPU_PREV_FILE" ]; then
        if read -r cpu_p_active cpu_p_total < "$CPU_PREV_FILE" 2>/dev/null; then
            case "$cpu_p_active" in ''|*[!0-9]*) cpu_p_active=0 ;; esac
            case "$cpu_p_total" in ''|*[!0-9]*) cpu_p_total=0 ;; esac

            local diff_a=$((cpu_active - cpu_p_active))
            local diff_t=$((cpu_total - cpu_p_total))

            if [ "$diff_t" -gt 0 ] && [ "$diff_a" -ge 0 ]; then
                local pct=$(( (100 * diff_a + diff_t / 2) / diff_t ))
                [ "$pct" -gt 100 ] && pct=100
                [ "$pct" -lt 0 ] && pct=0
                cpu_usage=$(printf "%3d%%" "$pct")
            fi
        fi
    fi

    printf "%s" "$cpu_usage"
    printf "%s %s\n" "$cpu_active" "$cpu_total" > "${CPU_PREV_FILE}.tmp" && mv "${CPU_PREV_FILE}.tmp" "$CPU_PREV_FILE"
}

# --- Network speed (flock-protected) ---
net_speed_raw() {
    # lock if flock exists (blocks)
    if command -v flock >/dev/null 2>&1; then
        flock 200
        local locked=1
    else
        local locked=0
    fi

    local n_rx=0 n_tx=0 line data
    while IFS= read -r line; do
        case "$line" in
            *"${NET_INTERFACE}:"*)
                data=${line#*:}
                set -- $data
                n_rx=${1:-0}
                n_tx=${9:-0}
                break
                ;;
        esac
    done < /proc/net/dev

    case "$n_rx" in ''|*[!0-9]*) n_rx=0 ;; esac
    case "$n_tx" in ''|*[!0-9]*) n_tx=0 ;; esac

    local p_rx=0 p_tx=0
    if [ -f "$NET_PREV_FILE" ]; then
        if read -r p_rx p_tx < "$NET_PREV_FILE" 2>/dev/null; then
            case "$p_rx" in ''|*[!0-9]*) p_rx=0 ;; esac
            case "$p_tx" in ''|*[!0-9]*) p_tx=0 ;; esac
        fi
    fi

    local d_rx=$((n_rx - p_rx)); [ "$d_rx" -lt 0 ] && d_rx=0
    local d_tx=$((n_tx - p_tx)); [ "$d_tx" -lt 0 ] && d_tx=0

    local denom=$((1024 * NET_INTERVAL))
    local rx_kib=$(( (d_rx + denom/2) / denom ))
    local tx_kib=$(( (d_tx + denom/2) / denom ))

    printf "↓%5d KiB/s ↑%4d KiB/s" "$rx_kib" "$tx_kib"

    printf "%s %s\n" "$n_rx" "$n_tx" > "${NET_PREV_FILE}.tmp" && mv "${NET_PREV_FILE}.tmp" "$NET_PREV_FILE"

    if [ "$locked" -eq 1 ]; then
        flock -u 200
    fi
}

# --- Audio (robust) ---
get_audio() {
    if ! command -v pactl >/dev/null 2>&1; then
        printf "🔇 0%%"
        return
    fi

    # default sink may be @DEFAULT_SINK@; use pactl get-sink-mute/volume
    local sink
    sink=$(pactl info 2>/dev/null | awk -F': ' '/Default Sink/ {print $2; exit}' || true)
    [ -z "$sink" ] && sink="@DEFAULT_SINK@"

    local vol mute
    vol=$(pactl get-sink-volume "$sink" 2>/dev/null | awk '{for(i=1;i<=NF;i++) if ($i ~ /%/) {print $i; exit}}' || true)
    mute=$(pactl get-sink-mute "$sink" 2>/dev/null | awk -F': ' '/Mute/ {print $2; exit}' || true)

    if [ "$mute" = "no" ]; then
        printf "🔉 %s" "${vol:-0%}"
    else
        printf "🔇 %s" "${vol:-0%}"
    fi
}

# --- Battery (safe) ---
battery_info_raw() {
    if command -v upower >/dev/null 2>&1; then
        local dev
        dev=$(upower --enumerate 2>/dev/null | grep -E 'BAT' | head -n1 || true)
        if [ -n "$dev" ]; then
            upower --show-info "$dev" 2>/dev/null | awk '/state|percentage/ {print $2}' | tr '\n' ' ' | \
                sed -e 's/discharging/BATT/' -e 's/charging/CHRG/' -e 's/fully-charged/FULL/' | awk '{print $1, $2}'
            return
        fi
    fi
    printf "N/A N/A"
}

# --- Keyboard layout without jq ---
keyboard_layout_raw() {
    local out layout
    if command -v swaymsg >/dev/null 2>&1; then
        out=$(swaymsg -t get_inputs 2>/dev/null || true)
    elif command -v i3-msg >/dev/null 2>&1; then
        out=$(i3-msg -t get_inputs 2>/dev/null || true)
    else
        out=""
    fi

    if [ -n "$out" ]; then
        layout=$(printf "%s" "$out" | grep -o '"xkb_active_layout_name"[[:space:]]*:[[:space:]]*"[^"]*"' | sed -E 's/.*:"([^"]+)".*/\1/' | head -n1 || true)
    else
        layout=""
    fi

    case "$layout" in
        *"UK"*) printf "UK" ;;
        *"Romanian"*) printf "RO" ;;
        "")
            printf "??"
            ;;
        *)
            printf "%s" "$(printf "%s" "$layout" | cut -c1-2 | tr '[:lower:]' '[:upper:]')"
            ;;
    esac
}

cpu_temp_raw() {
    local hw d name f v max=0 val

    # Prefer hwmon named coretemp
    for d in /sys/class/hwmon/hwmon*; do
        [ -r "$d/name" ] || continue
        name=$(cat "$d/name" 2>/dev/null || true)
        if [ "$name" = "coretemp" ] || [ "$name" = "k10temp" ]; then
            hw="$d"
            break
        fi
    done

    # If we found a coretemp hwmon, read its temp*_input files
    if [ -n "${hw:-}" ]; then
        for f in "$hw"/temp*_input; do
            [ -r "$f" ] || continue
            v=$(cat "$f" 2>/dev/null || true)
            [ -n "$v" ] || continue
            # handle millidegrees or plain degrees
            if [ "${#v}" -ge 4 ]; then
                val=$((v / 1000))
            else
                val=$v
            fi
            [ "$val" -gt "$max" ] && max=$val
        done
    fi

    # Fallback: check thermal_zone sensors if no hwmon coretemp found
    if [ "$max" -eq 0 ]; then
        for f in /sys/class/thermal/thermal_zone*/temp; do
            [ -r "$f" ] || continue
            v=$(cat "$f" 2>/dev/null || true)
            [ -n "$v" ] || continue
            if [ "${#v}" -ge 4 ]; then
                val=$((v / 1000))
            else
                val=$v
            fi
            [ "$val" -gt "$max" ] && max=$val
        done
    fi

    if [ "$max" -gt 0 ]; then
        printf "%2d°C" "$max"
    else
        printf "N/A"
    fi
}


# --- Main loop ---
while true; do
    date_formatted=$(date "+%a, %e %b, %H:%M")
    mem_usage=$(free -b | awk '/^Mem:/ {printf "%2dGi/%2dGi", ($3/1024/1024/1024)+0.5, ($2/1024/1024/1024)+0.5}')
    disk_info=$(df -h / | awk 'NR==2 {sub(/G/,"Gi",$4); sub(/G/,"Gi",$2); printf "%5s/%s", $4, $2}')
    cpu_load=$(cpu_usage_raw)
    net_speed=$(net_speed_raw)
    cpu_temp=$(cpu_temp_raw)
    audio_info=$(get_audio)
    battery_info=$(battery_info_raw)
    keyboard_layout=$(keyboard_layout_raw)

    printf "| RAM: %s | CPU: %s | Temp: %s | Disk: %s | %s | Vol: %s | %s | ↯ %s | %s\n" \
        "$mem_usage" "$cpu_load" "$cpu_temp" "$disk_info" "$net_speed" "$audio_info" "🖮 $keyboard_layout" "$battery_info" "$date_formatted"

    sleep "$NET_INTERVAL"
done
