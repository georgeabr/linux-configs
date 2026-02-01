// intellibar.cpp
// Fully featured, ultra-lean swaybar status command
// - Fixed-width fields for stable layout
// - EINTR-safe file reads
// - Robust sway IPC (partial I/O + little-endian packing)
// - Persistent PulseAudio connection using pa_threaded_mainloop
// - Default sink resolution, short cache, and timeouts

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
#include <errno.h>

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

    size_t off = 0;
    while (off + 1 < buflen) {
        ssize_t n = read(fd, buf + off, (ssize_t)(buflen - 1 - off));
        if (n > 0) {
            off += (size_t)n;
            if (off + 1 >= buflen) break;
            continue;
        } else if (n == 0) {
            break;
        } else {
            if (errno == EINTR) continue;
            break;
        }
    }
    buf[off] = '\0';
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
    snprintf(out, outlen, "%2lldGi/%2lldGi", used_gib, total_gib);
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
    snprintf(out, outlen, "%3lldGi/%3lldGi", avail, total);
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
    else snprintf(out, outlen, "%3d°C", max_temp);
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

/* ---------- Audio via libpulse (persistent threaded mainloop, default sink, cache, timeouts) ---------- */

#define PA_CACHE_TTL_SEC 2
#define PA_OP_TIMEOUT_MS 400
#define PA_INIT_TIMEOUT_MS 2000
#define PA_SERVERINFO_TIMEOUT_MS 500

static struct {
    pa_threaded_mainloop *ml;
    pa_context *ctx;
    int initialized;
    char default_sink[256];
    char cached_vol[16];
    time_t cached_at;
} pa_handle = { NULL, NULL, 0, {0}, "0%%", 0 };

struct pa_vol_ctx {
    int done;
    char vol[16];
};

/* callbacks: mainloop lock is already held by PulseAudio when these run */

static void pa_state_cb(pa_context *c, void *userdata) {
    (void)c;
    pa_threaded_mainloop *ml = (pa_threaded_mainloop *)userdata;
    pa_threaded_mainloop_signal(ml, 0);
}

static void pa_server_info_cb(pa_context *c, const pa_server_info *i, void *userdata) {
    (void)c;
    pa_threaded_mainloop *ml = (pa_threaded_mainloop *)userdata;
    if (i && i->default_sink_name) {
        strncpy(pa_handle.default_sink, i->default_sink_name,
                sizeof(pa_handle.default_sink) - 1);
        pa_handle.default_sink[sizeof(pa_handle.default_sink) - 1] = '\0';
    }
    pa_threaded_mainloop_signal(ml, 0);
}

static void pa_sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
    (void)c;
    struct pa_vol_ctx *ctx = (struct pa_vol_ctx *)userdata;

    if (eol > 0 || !i) {
        if (!ctx->done) {
            snprintf(ctx->vol, sizeof(ctx->vol), "0%%");
            ctx->done = 1;
        }
        pa_threaded_mainloop_signal(pa_handle.ml, 0);
        return;
    }

    if (!ctx->done) {
        pa_volume_t v = pa_cvolume_avg(&i->volume);
        int pct = (int)((100 * (long long)v) / PA_VOLUME_NORM);
        if (pct < 0) pct = 0;
        if (pct > 150) pct = 150;
        snprintf(ctx->vol, sizeof(ctx->vol), "%d%%", pct);
        ctx->done = 1;
    }
    pa_threaded_mainloop_signal(pa_handle.ml, 0);
}

static void fini_pulseaudio(void) {
    if (!pa_handle.initialized) return;
    pa_threaded_mainloop_lock(pa_handle.ml);
    pa_context_disconnect(pa_handle.ctx);
    pa_threaded_mainloop_unlock(pa_handle.ml);

    pa_threaded_mainloop_stop(pa_handle.ml);
    pa_context_unref(pa_handle.ctx);
    pa_threaded_mainloop_free(pa_handle.ml);

    pa_handle.ctx = NULL;
    pa_handle.ml = NULL;
    pa_handle.initialized = 0;
    pa_handle.default_sink[0] = '\0';
    pa_handle.cached_vol[0] = '\0';
    pa_handle.cached_at = 0;
}

static int init_pulseaudio(void) {
    if (pa_handle.initialized) return 0;

    pa_handle.ml = pa_threaded_mainloop_new();
    if (!pa_handle.ml) return -1;

    pa_mainloop_api *api = pa_threaded_mainloop_get_api(pa_handle.ml);
    pa_handle.ctx = pa_context_new(api, "intellibar");
    if (!pa_handle.ctx) {
        pa_threaded_mainloop_free(pa_handle.ml);
        pa_handle.ml = NULL;
        return -1;
    }

    pa_context_set_state_callback(pa_handle.ctx, pa_state_cb, pa_handle.ml);

    if (pa_threaded_mainloop_start(pa_handle.ml) != 0) {
        pa_context_unref(pa_handle.ctx);
        pa_threaded_mainloop_free(pa_handle.ml);
        pa_handle.ctx = NULL;
        pa_handle.ml = NULL;
        return -1;
    }

    pa_threaded_mainloop_lock(pa_handle.ml);
    if (pa_context_connect(pa_handle.ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        pa_threaded_mainloop_unlock(pa_handle.ml);
        pa_threaded_mainloop_stop(pa_handle.ml);
        pa_context_unref(pa_handle.ctx);
        pa_threaded_mainloop_free(pa_handle.ml);
        pa_handle.ctx = NULL;
        pa_handle.ml = NULL;
        return -1;
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        pa_context_state_t st = pa_context_get_state(pa_handle.ctx);
        if (st == PA_CONTEXT_READY) break;
        if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) {
            pa_threaded_mainloop_unlock(pa_handle.ml);
            pa_threaded_mainloop_stop(pa_handle.ml);
            pa_context_unref(pa_handle.ctx);
            pa_threaded_mainloop_free(pa_handle.ml);
            pa_handle.ctx = NULL;
            pa_handle.ml = NULL;
            return -1;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms >= PA_INIT_TIMEOUT_MS) {
            fprintf(stderr, "intellibar: pulseaudio init timeout\n");
            pa_threaded_mainloop_unlock(pa_handle.ml);
            pa_threaded_mainloop_stop(pa_handle.ml);
            pa_context_unref(pa_handle.ctx);
            pa_threaded_mainloop_free(pa_handle.ml);
            pa_handle.ctx = NULL;
            pa_handle.ml = NULL;
            return -1;
        }
        pa_threaded_mainloop_wait(pa_handle.ml);
    }

    pa_threaded_mainloop_unlock(pa_handle.ml);

    atexit(fini_pulseaudio);
    pa_handle.initialized = 1;
    return 0;
}

/* refresh default sink name (used when cache is stale) */
static void refresh_default_sink(void) {
    if (!pa_handle.initialized || !pa_handle.ml) return;

    pa_operation *op = NULL;
    struct timespec start;

    pa_threaded_mainloop_lock(pa_handle.ml);
    pa_handle.default_sink[0] = '\0';

    op = pa_context_get_server_info(pa_handle.ctx, pa_server_info_cb, pa_handle.ml);
    if (!op) {
        pa_threaded_mainloop_unlock(pa_handle.ml);
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING &&
           pa_handle.default_sink[0] == '\0') {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms >= PA_SERVERINFO_TIMEOUT_MS) break;
        pa_threaded_mainloop_wait(pa_handle.ml);
    }
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(pa_handle.ml);
}

/* get_audio: short cache, refresh default sink on cache miss, timeout per op */
static void get_audio(char *out, size_t outlen) {
    time_t now = time(NULL);

    if (pa_handle.cached_at != 0 &&
        now - pa_handle.cached_at <= PA_CACHE_TTL_SEC &&
        pa_handle.cached_vol[0]) {
        snprintf(out, outlen, "%4s", pa_handle.cached_vol);
        return;
    }

    if (init_pulseaudio() != 0) {
        snprintf(out, outlen, "0%%");
        return;
    }

    refresh_default_sink();

    struct pa_vol_ctx vctx;
    vctx.done = 0;
    snprintf(vctx.vol, sizeof(vctx.vol), "0%%");

    pa_operation *op = NULL;
    struct timespec start;

    pa_threaded_mainloop_lock(pa_handle.ml);
    if (pa_handle.default_sink[0]) {
        op = pa_context_get_sink_info_by_name(pa_handle.ctx,
                                              pa_handle.default_sink,
                                              pa_sink_info_cb, &vctx);
    } else {
        op = pa_context_get_sink_info_list(pa_handle.ctx,
                                           pa_sink_info_cb, &vctx);
    }

    if (op) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING &&
               !vctx.done) {
            struct timespec nowts;
            clock_gettime(CLOCK_MONOTONIC, &nowts);
            long elapsed_ms = (nowts.tv_sec - start.tv_sec) * 1000 +
                              (nowts.tv_nsec - start.tv_nsec) / 1000000;
            if (elapsed_ms >= PA_OP_TIMEOUT_MS) {
                pa_operation_cancel(op);
                break;
            }
            pa_threaded_mainloop_wait(pa_handle.ml);
        }
        pa_operation_unref(op);
    }

    strncpy(pa_handle.cached_vol, vctx.vol, sizeof(pa_handle.cached_vol) - 1);
    pa_handle.cached_vol[sizeof(pa_handle.cached_vol) - 1] = '\0';
    pa_handle.cached_at = time(NULL);

    pa_threaded_mainloop_unlock(pa_handle.ml);

    snprintf(out, outlen, "%4s", pa_handle.cached_vol);
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
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    unsigned char hdr[14];
    memcpy(hdr, "i3-ipc", 6);
    hdr[6] = hdr[7] = hdr[8] = hdr[9] = 0;
    uint32_t type = I3_IPC_MESSAGE_TYPE_GET_INPUTS;
    hdr[10] = (unsigned char)(type & 0xff);
    hdr[11] = (unsigned char)((type >> 8) & 0xff);
    hdr[12] = (unsigned char)((type >> 16) & 0xff);
    hdr[13] = (unsigned char)((type >> 24) & 0xff);

    size_t to_write = sizeof(hdr);
    size_t off = 0;
    while (to_write > 0) {
        ssize_t w = write(fd, hdr + off, to_write);
        if (w < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        }
        off += (size_t)w;
        to_write -= (size_t)w;
    }

    unsigned char rh[14];
    size_t need = sizeof(rh);
    off = 0;
    while (need > 0) {
        ssize_t r = read(fd, rh + off, need);
        if (r < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        } else if (r == 0) {
            close(fd);
            return -1;
        }
        off += (size_t)r;
        need -= (size_t)r;
    }

    if (memcmp(rh, "i3-ipc", 6) != 0) {
        close(fd);
        return -1;
    }

    uint32_t rsize = (uint32_t)rh[6] |
                     ((uint32_t)rh[7] << 8) |
                     ((uint32_t)rh[8] << 16) |
                     ((uint32_t)rh[9] << 24);

    size_t to_read = (size_t)rsize;
    if (to_read >= buflen) to_read = buflen - 1;

    off = 0;
    while (to_read > 0) {
        ssize_t r = read(fd, buf + off, to_read);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        } else if (r == 0) break;
        off += (size_t)r;
        to_read -= (size_t)r;
    }
    buf[off] = '\0';
    close(fd);
    return (off > 0) ? 0 : -1;
}

static void get_kb(char *out, size_t outlen) {
    static char buf[262144];
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

