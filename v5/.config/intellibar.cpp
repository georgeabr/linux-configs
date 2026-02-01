// intellibar.cpp
// Fully featured, ultra-lean swaybar status command
// FIXED: Keyboard detection now skips "null" layouts from dummy devices (Power/Lid) 
// and grabs the first actual layout string found.

#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pulse/pulseaudio.h>

#define STATS_INTERVAL 2
#define NET_IFACE "wlp59s0"

struct StatusData {
    char line[512];
    pthread_mutex_t mtx;
} shared_data;

/* ---------- small helpers ---------- */

static void read_file(const char *path, char *buf, size_t buflen) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { buf[0] = '\0'; return; }
    ssize_t n = read(fd, buf, buflen - 1);
    if (n < 0) n = 0;
    buf[n] = '\0';
    close(fd);
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
        s[--len] = '\0';
    }
}

/* ---------- RAM ---------- */

static void get_mem(char *out, size_t outlen) {
    char buf[1024];
    read_file("/proc/meminfo", buf, sizeof(buf));
    long long total = 0, avail = 0;
    char *p = buf;
    while (*p) {
        if (strncmp(p, "MemTotal:", 9) == 0) {
            sscanf(p + 9, "%lld", &total);
        } else if (strncmp(p, "MemAvailable:", 13) == 0) {
            sscanf(p + 13, "%lld", &avail);
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    if (total <= 0) {
        snprintf(out, outlen, "N/A");
        return;
    }
    long long used_gib = (total - avail) / 1048576;
    long long total_gib = total / 1048576;
    snprintf(out, outlen, "%lldGi/%lldGi", used_gib, total_gib);
}

/* ---------- Disk ---------- */

static void get_disk(char *out, size_t outlen) {
    struct statvfs st;
    if (statvfs("/", &st) != 0) {
        snprintf(out, outlen, "N/A");
        return;
    }
    long long avail = (long long)st.f_bavail * st.f_frsize / (1024LL * 1024LL * 1024LL);
    long long total = (long long)st.f_blocks * st.f_frsize / (1024LL * 1024LL * 1024LL);
    snprintf(out, outlen, "%lldGi/%lldGi", avail, total);
}

/* ---------- CPU ---------- */

static void get_cpu_usage(char *out, size_t outlen) {
    static long long prev_total = 0, prev_active = 0;

    char buf[256];
    read_file("/proc/stat", buf, sizeof(buf));
    long long u, n, s, i, w, x, y, z;
    char label[16];
    if (sscanf(buf, "%15s %lld %lld %lld %lld %lld %lld %lld %lld",
               label, &u, &n, &s, &i, &w, &x, &y, &z) != 9) {
        snprintf(out, outlen, "  0%%");
        return;
    }

    long long total = u + n + s + i + w + x + y + z;
    long long active = u + n + s + w + x + y + z;

    if (prev_total == 0) {
        prev_total = total;
        prev_active = active;
        snprintf(out, outlen, "  0%%");
        return;
    }

    long long diff_t = total - prev_total;
    long long diff_a = active - prev_active;
    prev_total = total;
    prev_active = active;

    int pct = 0;
    if (diff_t > 0) {
        pct = (int)(100 * diff_a / diff_t);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
    }
    snprintf(out, outlen, "%3d%%", pct);
}

/* ---------- Net ---------- */

static void get_net_speed(char *out, size_t outlen) {
    static long long prev_rx = 0, prev_tx = 0;

    char buf[2048];
    read_file("/proc/net/dev", buf, sizeof(buf));
    char *p = buf;
    long long rx = 0, tx = 0;
    while (*p) {
        while (*p == ' ' || *p == '\n') p++;
        if (!*p) break;
        char *line = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') *p++ = '\0';

        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *iface = line;
        while (*iface == ' ') iface++;
        if (strcmp(iface, NET_IFACE) != 0) continue;

        char *stats = colon + 1;
        long long vals[16];
        int n = 0;
        char *q = stats;
        while (*q && n < 16) {
            while (*q == ' ') q++;
            if (!*q) break;
            char *end = q;
            while (*end && *end != ' ') end++;
            char tmp = *end;
            *end = '\0';
            vals[n++] = atoll(q);
            *end = tmp;
            q = end;
        }
        if (n >= 9) {
            rx = vals[0];
            tx = vals[8];
        }
        break;
    }

    if (prev_rx == 0) {
        prev_rx = rx;
        prev_tx = tx;
        snprintf(out, outlen, "↓    0 KiB/s ↑   0 KiB/s");
        return;
    }

    long long drx = (rx - prev_rx) / (1024 * STATS_INTERVAL);
    long long dtx = (tx - prev_tx) / (1024 * STATS_INTERVAL);
    prev_rx = rx;
    prev_tx = tx;
    if (drx < 0) drx = 0;
    if (dtx < 0) dtx = 0;

    snprintf(out, outlen, "↓%5lld KiB/s ↑%4lld KiB/s", drx, dtx);
}

/* ---------- Temp ---------- */

static void get_temp(char *out, size_t outlen) {
    int max_temp = 0;
    char path[128], buf[32];
    for (int i = 0; i < 15; ++i) {
        for (int j = 1; j <= 10; ++j) {
            snprintf(path, sizeof(path), "/sys/class/hwmon/hwmon%d/temp%d_input", i, j);
            int fd = open(path, O_RDONLY);
            if (fd < 0) continue;
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n <= 0) continue;
            buf[n] = '\0';
            int t = atoi(buf);
            if (t > 1000000) t /= 1000000;
            else if (t > 1000) t /= 1000;
            if (t > 0 && t < 150 && t > max_temp) max_temp = t;
        }
    }
    if (max_temp <= 0) snprintf(out, outlen, "N/A");
    else snprintf(out, outlen, "%d°C", max_temp);
}

/* ---------- Battery via /sys ---------- */

static void get_battery(char *out, size_t outlen) {
    char path[128], buf[64];
    struct stat st;
    if (stat("/sys/class/power_supply/BAT0", &st) != 0) {
        snprintf(out, outlen, "N/A N/A");
        return;
    }

    snprintf(path, sizeof(path), "/sys/class/power_supply/BAT0/status");
    read_file(path, buf, sizeof(buf));
    trim_newline(buf);
    char state[5] = "BATT";
    if (strstr(buf, "Charging"))  strcpy(state, "CHRG");
    else if (strstr(buf, "Full")) strcpy(state, "FULL");

    snprintf(path, sizeof(path), "/sys/class/power_supply/BAT0/capacity");
    read_file(path, buf, sizeof(buf));
    trim_newline(buf);
    if (buf[0] == '\0') {
        snprintf(out, outlen, "N/A N/A");
        return;
    }

    char perc[8];
    snprintf(perc, sizeof(perc), "%s%%", buf);
    snprintf(out, outlen, "%s %s", state, perc);
}

/* ---------- Audio via libpulse ---------- */

struct pa_vol_ctx {
    int done;
    char vol[16];
};

static void pa_sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
    (void)c;
    if (eol > 0 || !i) {
        struct pa_vol_ctx *ctx = (struct pa_vol_ctx *)userdata;
        if (!ctx->done) {
            snprintf(ctx->vol, sizeof(ctx->vol), "0%%");
            ctx->done = 1;
        }
        return;
    }
    struct pa_vol_ctx *ctx = (struct pa_vol_ctx *)userdata;
    if (ctx->done) return;

    pa_volume_t v = pa_cvolume_avg(&i->volume);
    int pct = (int)((100 * (long long)v) / PA_VOLUME_NORM);
    if (pct < 0) pct = 0;
    if (pct > 150) pct = 150;
    snprintf(ctx->vol, sizeof(ctx->vol), "%d%%", pct);
    ctx->done = 1;
}

static void pa_state_cb(pa_context *c, void *userdata) {
    (void)userdata;
    pa_context_state_t st = pa_context_get_state(c);
    if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) {
        // nothing, mainloop will exit
    }
}

static void get_audio(char *out, size_t outlen) {
    pa_mainloop *ml = pa_mainloop_new();
    if (!ml) { snprintf(out, outlen, "0%%"); return; }

    pa_mainloop_api *api = pa_mainloop_get_api(ml);
    pa_context *ctx = pa_context_new(api, "intellibar");
    if (!ctx) {
        pa_mainloop_free(ml);
        snprintf(out, outlen, "0%%");
        return;
    }

    pa_context_set_state_callback(ctx, pa_state_cb, NULL);
    if (pa_context_connect(ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        pa_context_unref(ctx);
        pa_mainloop_free(ml);
        snprintf(out, outlen, "0%%");
        return;
    }

    int ready = 0;
    while (!ready) {
        int r;
        if (pa_mainloop_iterate(ml, 1, &r) < 0) break;
        pa_context_state_t st = pa_context_get_state(ctx);
        if (st == PA_CONTEXT_READY) ready = 1;
        if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) break;
    }

    struct pa_vol_ctx vctx;
    vctx.done = 0;
    snprintf(vctx.vol, sizeof(vctx.vol), "0%%");

    if (ready) {
        pa_operation *op = pa_context_get_sink_info_by_index(ctx, PA_INVALID_INDEX, pa_sink_info_cb, &vctx);
        if (op) {
            while (!vctx.done) {
                int r;
                if (pa_mainloop_iterate(ml, 1, &r) < 0) break;
            }
            pa_operation_unref(op);
        }
    }

    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(ml);

    snprintf(out, outlen, "%s", vctx.vol);
}

/* ---------- Keyboard layout via sway IPC ---------- */

struct sway_ipc_header {
    char magic[6];
    uint32_t size;
    uint32_t type;
};

#define I3_IPC_MESSAGE_TYPE_GET_INPUTS 100


static int sway_ipc_request(char *buf, size_t buflen) {
    const char *sock = getenv("SWAYSOCK");
    if (!sock || !*sock) return -1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    // Build exact 14-byte header: "i3-ipc" + uint32 size + uint32 type
    unsigned char hdr[14];
    memcpy(hdr, "i3-ipc", 6);
    uint32_t size = 0;
    uint32_t type = I3_IPC_MESSAGE_TYPE_GET_INPUTS;
    memcpy(hdr + 6,  &size, 4);
    memcpy(hdr + 10, &type, 4);

    if (write(fd, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
        close(fd);
        return -1;
    }

    // Read reply header (also 14 bytes)
    unsigned char rh[14];
    ssize_t n = read(fd, rh, sizeof(rh));
    if (n != (ssize_t)sizeof(rh)) {
        close(fd);
        return -1;
    }
    if (memcmp(rh, "i3-ipc", 6) != 0) {
        close(fd);
        return -1;
    }

    uint32_t rsize;
    memcpy(&rsize, rh + 6, 4);

    size_t to_read = rsize;
    if (to_read >= buflen) to_read = buflen - 1;

    size_t off = 0;
    while (to_read > 0) {
        n = read(fd, buf + off, to_read);
        if (n <= 0) break;
        off += (size_t)n;
        to_read -= (size_t)n;
    }
    buf[off] = '\0';
    close(fd);
    return (off > 0) ? 0 : -1;
}


static void get_kb(char *out, size_t outlen) {
    static char buf[262144]; // 256 KB
    if (sway_ipc_request(buf, sizeof(buf)) != 0) {
        snprintf(out, outlen, "??");
        return;
    }

    const char *key = "\"xkb_active_layout_name\"";
    const char *p = buf;
    char best[64] = {0};

    while ((p = strstr(p, key)) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) { p += strlen(key); continue; }

        const char *val = colon + 1;
        while (*val == ' ' || *val == '\t' || *val == '\n') val++;

        if (strncmp(val, "null", 4) == 0) {
            p = val + 4;
            continue;
        }

        if (*val != '"') { p = val + 1; continue; }
        val++;
        const char *end = strchr(val, '"');
        if (!end) { p = val; continue; }

        size_t len = (size_t)(end - val);
        if (len == 0) { p = end + 1; continue; }
        if (len >= sizeof(best)) len = sizeof(best) - 1;

        memcpy(best, val, len);
        best[len] = '\0';
        break;
    }

    if (best[0] == '\0') {
        snprintf(out, outlen, "??");
        return;
    }

    if (strstr(best, "UK") || strstr(best, "United Kingdom") || strstr(best, "British")) {
        snprintf(out, outlen, "UK");
        return;
    }
    if (strstr(best, "Romanian")) {
        snprintf(out, outlen, "RO");
        return;
    }

    char s[3] = {'?', '?', '\0'};
    if (best[0]) s[0] = (best[0] >= 'a' && best[0] <= 'z') ? (char)(best[0] - 32) : best[0];
    if (best[1]) s[1] = (best[1] >= 'a' && best[1] <= 'z') ? (char)(best[1] - 32) : best[1];

    snprintf(out, outlen, "%s", s);
}


/* ---------- stats thread ---------- */

static void *gather_stats_loop(void *) {
    char mem[32], cpu[16], temp[16], disk[32], net[64], audio[16], kb[8], batt[32];

    while (1) {
        get_mem(mem, sizeof(mem));
        get_cpu_usage(cpu, sizeof(cpu));
        get_temp(temp, sizeof(temp));
        get_disk(disk, sizeof(disk));
        get_net_speed(net, sizeof(net));
        get_audio(audio, sizeof(audio));
        get_kb(kb, sizeof(kb));
        get_battery(batt, sizeof(batt));

        char line[512];
        snprintf(line, sizeof(line),
                 "| RAM: %s | CPU: %s | Temp: %s | Disk: %s | %s | Vol: %s | 🖮  %s | ↯ %s",
                 mem, cpu, temp, disk, net, audio, kb, batt);

        pthread_mutex_lock(&shared_data.mtx);
        strncpy(shared_data.line, line, sizeof(shared_data.line) - 1);
        shared_data.line[sizeof(shared_data.line) - 1] = '\0';
        pthread_mutex_unlock(&shared_data.mtx);

        sleep(STATS_INTERVAL);
    }
    return NULL;
}

/* ---------- main loop ---------- */

int main() {
    pthread_mutex_init(&shared_data.mtx, NULL);
    shared_data.line[0] = '\0';

    pthread_t th;
    pthread_create(&th, NULL, gather_stats_loop, NULL);
    pthread_detach(th);

    while (1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        ts.tv_nsec = 0;
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

        char line[512];
        pthread_mutex_lock(&shared_data.mtx);
        strncpy(line, shared_data.line, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        pthread_mutex_unlock(&shared_data.mtx);

        if (line[0] == '\0') continue;

        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char date_buf[64];
        strftime(date_buf, sizeof(date_buf), "%a, %e %b, %H:%M", &tm);

        char out[640];
        int n = snprintf(out, sizeof(out), "%s | %s\n", line, date_buf);
        if (n > 0) write(1, out, (size_t)n);
    }

    return 0;
}
