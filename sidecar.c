/* A sidecar tool to watch debug chatter and resources from a simple
 * "side car" terminal drawer window. 
 *
 * s = swap
 * i = iowait over sampling time
 *
 * Still very much a work-in-progress.
 *
 * Usage: sidecar [optional file to watch "tail -f" style]
 * Then watch pretty graphs update while debug messages roll by.
 * To use with a splinter store (in memory) just prefix the 
 * name with spl: - e.g.
 * 
 * sidecar spl:my_bus
 * 
 * ... would attach it to /dev/shm/my_bus
 * 
 * sidecar /var/log/some_log.txt 
 * 
 * .... would watch a text file instead.
 *  
 * Copyright 2025 Tim Post <timthepost@protonmail.com>
 *
 * License: Apache 2
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>
#include <splinter.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define HISTORY_HEIGHT 10
#define REFRESH 500000
#define HISTORY_DIVISOR 4
#define MAXW 512
#define MAX_DEBUG_LINES 12

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} cpu_stats;

typedef struct {
    double load_1min;
    double load_5min;
    double load_15min;
    int running_processes;
    int total_processes;
    int last_pid;
} loadavg_t;

typedef struct {
    // battery percentage (0-100, -1 if no battery)
    int battery_percent;
    // 1 if on AC charger, 0 if not
    unsigned int on_ac;
} power_status_t;

static int term_cols = 80;
static int term_rows = 24;
static int graph_width = 50;

static double hist_cpu[MAXW] = {0};
static double hist_mem[MAXW] = {0};

static double last_io_pct = 0.0;
static volatile sig_atomic_t resize_pending = 0;

static char *debug_lines[MAX_DEBUG_LINES];
static int debug_line_count = 0;
static FILE *dbg_fp = NULL;

static unsigned int debug_signal_group = 63;
static uint64_t debug_bloom = (0x800000000000000);
static struct termios orig_termios;

// circular buffer - add an item in
static void debug_log_append(const char *line) {
    if (debug_line_count >= MAX_DEBUG_LINES) {
        free(debug_lines[0]);
        debug_lines[0] = '\0';
        memmove(&debug_lines[0], &debug_lines[1],
                (MAX_DEBUG_LINES-1)*sizeof(char*));
        debug_line_count--;
    }
    int head = debug_line_count ++;
    debug_lines[head] = strdup(line);
}

// circular buffer - cleanup
static void debug_log_destroy() {
    if (debug_line_count && debug_lines[0]) {
        for (int i = 0; i < debug_line_count; i++) {
            if (debug_lines[i]) {
                free(debug_lines[i]);
                debug_lines[i] = '\0';
            }
        }
    }
}

// callback for bloom matches on cleanup
void cause_key_unwatch(const char *key, uint64_t epoch, void *data) {
    (void) data;
    (void) epoch;
    if (! key) return;
    splinter_watch_unregister(key, debug_bloom);
}

// callback for bloom matches on slots
void on_key_match(const char *key, uint64_t epoch, void *data) {
    (void) data;
    if (!key) return;
    int rc = -1;

    char msg[MAXW] = { 0 }, buff[4096] = { 0 };
    size_t buff_sz = 0;
    rc = splinter_get(key, &buff, sizeof(buff) - 1, &buff_sz);
    snprintf(msg, MAXW, "(%lu) %s", epoch, rc == 0 ? buff : "(no value set)");
    debug_log_append(msg);
}

static void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    // disable echo and canonical mode, keep signal chars (Ctrl+C) intact
    raw.c_lflag &= ~(ECHO | ICANON);
    // non-blocking read
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void handle_winch(int sig) {
    (void)sig;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_cols  = ws.ws_col;
        term_rows  = ws.ws_row;
        graph_width = term_cols - 12;
        if (graph_width > MAXW) graph_width = MAXW;
        if (graph_width < 20)   graph_width = 20;
    }
    resize_pending = 1;
}

static void install_winch_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGWINCH, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

// the tail -f style watch
static int init_debug_file(const char *path) {
    dbg_fp = fopen(path, "r");
    if (!dbg_fp) {
        fprintf(stderr, "Unable to load debug file %s\n", path);
        perror("file");
        return -1;
    }
    setvbuf(dbg_fp, NULL, _IONBF, 0);
    fseek(dbg_fp, 0, SEEK_END);
    return 0;
}

// read the 'tail' file 
static int read_debug_file() {
    if (!dbg_fp) return 0;
    char line[1024];
    int updated = 0;
    while (fgets(line, sizeof(line), dbg_fp)) {
        line[strcspn(line, "\r\n")] = 0;
        debug_log_append(line);
        updated = 1;
    }
    clearerr(dbg_fp);
    return updated;
}

static void get_cpu_stats(cpu_stats *s) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) exit(1);
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
           &s->user, &s->nice, &s->system, &s->idle,
           &s->iowait, &s->irq, &s->softirq, &s->steal);
    fclose(f);
}

static int parse_loadavg(loadavg_t *loadavg) {
    FILE *file;
    char buffer[256];
    
    if (!loadavg) return -1;

    file = fopen("/proc/loadavg", "r");
    if (!file) return -1;

    if (!fgets(buffer, sizeof(buffer), file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    
    int parsed = sscanf(buffer, "%lf %lf %lf %d/%d %d",
            &loadavg->load_1min, &loadavg->load_5min, &loadavg->load_15min,
            &loadavg->running_processes, &loadavg->total_processes, &loadavg->last_pid);
    if (parsed != 6) return -1;
    return 0;
}

static double get_cpu_usage(cpu_stats *prev) {
    cpu_stats cur;
    get_cpu_stats(&cur);
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long idle = cur.idle + cur.iowait;
    unsigned long long prev_non = (prev->user + prev->nice + prev->system +
                                   prev->irq + prev->softirq + prev->steal);
    unsigned long long non = (cur.user + cur.nice + cur.system +
                              cur.irq + cur.softirq + cur.steal);
    unsigned long long prev_total = prev_idle + prev_non;
    unsigned long long total = idle + non;
    unsigned long long diff_total = total - prev_total;
    unsigned long long diff_idle = idle - prev_idle;
    unsigned long long diff_iowait = cur.iowait - prev->iowait;

    *prev = cur;

    if (diff_total > 0) {
        last_io_pct = (double)diff_iowait / diff_total * 100.0;
        return (double)(diff_total - diff_idle) / diff_total * 100.0;
    } else {
        last_io_pct = 0.0;
        return 0.0;
    }
}

static double get_mem_usage(double *swap_pct) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) exit(1);
    char key[32]; unsigned long val; char unit[8];
    unsigned long mem_total=1, mem_free=0, buffers=0, cached=0;
    unsigned long swap_total=1, swap_free=0;
    while (fscanf(f, "%31s %lu %7s\n", key, &val, unit) == 3) {
        if (!strcmp(key, "MemTotal:")) mem_total = val;
        else if (!strcmp(key, "MemFree:")) mem_free = val;
        else if (!strcmp(key, "Buffers:")) buffers = val;
        else if (!strcmp(key, "Cached:")) cached = val;
        else if (!strcmp(key, "SwapTotal:")) swap_total = val;
        else if (!strcmp(key, "SwapFree:")) swap_free = val;
    }
    fclose(f);
    unsigned long used = mem_total - mem_free - buffers - cached;
    *swap_pct = (swap_total > 0) ?
                (double)(swap_total - swap_free) / swap_total * 100.0 : 0.0;
    return (double)used / mem_total * 100.0;
}

static int read_sys_int(const char *path) {
    FILE *file;
    int value = -1;
    file = fopen(path, "r");
    if (file) {
        fscanf(file, "%d", &value);
        fclose(file);
    }
    return value;
}

static int parse_power_status(power_status_t *power) {
    DIR *dir;
    struct dirent *entry;
    char path[512], type_path[1024] = { 0 }, value_path[1024] = { 0 };
    FILE *file;
    char type_buf[32];
    int found_battery = 0, found_ac = 0;

    if (!power) return -1;

    power->battery_percent = -1;
    power->on_ac = 0;

    dir = opendir("/sys/class/power_supply");
    if (!dir) return -1;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s", entry->d_name);
        snprintf(type_path, sizeof(type_path), "%s/type", path);

        file = fopen(type_path, "r");
        if (!file) continue;

        if (!fgets(type_buf, sizeof(type_buf), file)) {
            fclose(file);
            continue;
        }
        fclose(file);
        type_buf[strcspn(type_buf, "\n")] = '\0';

        if (strcmp(type_buf, "Battery") == 0 && !found_battery) {
            snprintf(value_path, sizeof(value_path), "%s/capacity", path);
            power->battery_percent = read_sys_int(value_path);
            if (power->battery_percent >= 0) found_battery = 1;
        }
        else if ((strcmp(type_buf, "Mains") == 0 || 
                  strcmp(type_buf, "ADP1") == 0 ||
                  strstr(entry->d_name, "ADP") != NULL ||
                  strstr(entry->d_name, "AC") != NULL) && !found_ac) {
            snprintf(value_path, sizeof(value_path), "%s/online", path);
            int online = read_sys_int(value_path);
            if (online > 0) { power->on_ac = 1; found_ac = 1; }
        }
    }
    closedir(dir);

    if (!found_ac && found_battery) {
        dir = opendir("/sys/class/power_supply");
        if (dir) {
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                snprintf(path, sizeof(path), "/sys/class/power_supply/%s", entry->d_name);
                snprintf(type_path, sizeof(type_path), "%s/type", path);
                file = fopen(type_path, "r");
                if (!file) continue;

                if (fgets(type_buf, sizeof(type_buf), file)) {
                    type_buf[strcspn(type_buf, "\n")] = '\0';
                    if (strcmp(type_buf, "Battery") == 0) {
                        snprintf(value_path, sizeof(value_path), "%s/status", path);
                        file = freopen(value_path, "r", file);
                        if (file && fgets(type_buf, sizeof(type_buf), file)) {
                            type_buf[strcspn(type_buf, "\n")] = '\0';
                            if (strcmp(type_buf, "Charging") == 0) power->on_ac = 1;
                        }
                        fclose(file);
                        break;
                    }
                }
                fclose(file);
            }
            closedir(dir);
        }
    }
    return 0;
}

static void draw_bar(const char *label, double percent) {
    int filled = (int)(percent / 100.0 * graph_width);
    printf("┌> ");
    for (int i = 0; i < graph_width; i++) {
        if (i < filled) printf("■");
        else printf(" ");
    }
    printf("%-3s\n└> %-5.1f%%\n", label, percent);
}

int main(int argc, char **argv) {
    char *store = NULL;
    const char *needle = "spl:";
    
    if (argc > 1) {
        char *match = strstr(argv[1], needle);
        if (match != NULL) {
            store = match + strlen(needle);
            if (splinter_open_or_create(store, 1024, 4096) != 0) {
                fprintf(stderr,"Unable to open splinter store: %s\n", store); 
                perror("splinter_open_or_create()");
                exit(1);
            }
            dbg_fp = NULL;
        } else if (init_debug_file(argv[1]) != 0) {
            dbg_fp = NULL; 
        }
    }

    // enter non-blocking / non-echoing terminal state
    enable_raw_mode();

    install_winch_handler();
    handle_winch(0);

    cpu_stats prev;
    loadavg_t loads;
    power_status_t power;
    int hist_counter = 0;

    // primitive to track state for keystroke (symlink) 
    // jobs (0 through 9)
    pid_t jobs[10] = {0};
    char job_results[10][64] = {{0}};

    get_cpu_stats(&prev);
    parse_loadavg(&loads);
    parse_power_status(&power);

    // tell splinter we want any changes to debug keys to pulse us
    if (splinter_watch_label_register(debug_bloom, debug_signal_group) != 0) {
        fprintf(stderr,"* spl:watch(!)\n");
    }

    uint64_t old_signal_count = splinter_get_signal_count(debug_signal_group);

    printf("\033[2J");

    do {
        // completed keystroke-spawned jobs
        for (int i = 0; i < 10; i++) {
            if (jobs[i] > 0) {
                int status;
                // WNOHANG guarantees we don't freeze if the job is still running
                if (waitpid(jobs[i], &status, WNOHANG) > 0) {
                    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                    snprintf(job_results[i], sizeof(job_results[i]), ".sidecar.%d:%d", i, exit_code);
                    jobs[i] = 0; // Free up the slot
                }
            }
        }

        // keystroke handling  

        char c;
        // non-blocking loop reading buffered inputs
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c >= '0' && c <= '9') {
                int idx = c - '0';
                if (jobs[idx] == 0) { // disallow firing duplicates before finished
                    char script_path[32];
                    snprintf(script_path, sizeof(script_path), ".sidecar.%d", idx);

                    struct stat st;
                    if (lstat(script_path, &st) == 0 && S_ISLNK(st.st_mode)) {
                        pid_t pid = fork();
                        if (pid == 0) {
                            
                            int null_fd = open("/dev/null", O_WRONLY);
                            if (null_fd != -1) {
                                dup2(null_fd, STDOUT_FILENO);
                                dup2(null_fd, STDERR_FILENO);
                                close(null_fd);
                            }
                            
                            char exec_path[34];
                            snprintf(exec_path, sizeof(exec_path), "./%s", script_path);
                            execl("/bin/sh", "sh", "-c", exec_path, (char *)NULL);
                            exit(127); // failsafe if execl drops
                        } else if (pid > 0) {
                            jobs[idx] = pid;
                            job_results[idx][0] = '\0'; // clear out older results
                        }
                    }
                }
            } else if (c == 'q' || c == 'Q') {
                // ctrl-c can sometimes behave oddly in raw mode
                printf("\033[2J\033[H");
                exit(0);
            }
        }

        double swap_pct;
        double cpu = get_cpu_usage(&prev);
        double mem = get_mem_usage(&swap_pct);
        parse_loadavg(&loads);
        parse_power_status(&power);

        int new_debug = 0;
        if (dbg_fp) {
            new_debug = read_debug_file();
        }

        if (hist_counter == 0) {
            memmove(hist_cpu, hist_cpu+1, (MAXW-1)*sizeof(double));
            memmove(hist_mem, hist_mem+1, (MAXW-1)*sizeof(double));
            hist_cpu[MAXW-1] = cpu;
            hist_mem[MAXW-1] = mem;
        }
        hist_counter = (hist_counter + 1) % HISTORY_DIVISOR;

        if (resize_pending || new_debug) {
            printf("\033[2J");
            resize_pending = 0;
        }

        printf("\033[H");

        printf("History (CPU=█, RAM=░)\n");
        for (int row=HISTORY_HEIGHT; row>=0; row--) {
            for (int col=MAXW-graph_width; col<MAXW; col++) {
                int c = (int)(hist_cpu[col] / 100.0 * HISTORY_HEIGHT);
                int m = (int)(hist_mem[col] / 100.0 * HISTORY_HEIGHT);
                if (c >= row && m >= row) printf("▓");
                else if (c >= row) printf("█");
                else if (m >= row) printf("░");
                else printf(" ");
            }
            printf("\n");
        }

        printf("\n");
        draw_bar("cpu", cpu);
        printf(" > s=%-.1f%% | i=%-.1f%% | 1=%-.2f | 5=%-.2f | 15=%-.2f\n",
            swap_pct, last_io_pct, loads.load_1min, loads.load_5min, loads.load_15min);
        printf(" > [%d/%d] :: (%d%% %s\n",
            loads.running_processes,
            loads.total_processes,
            power.battery_percent == -1 ? 0 : power.battery_percent, 
            power.on_ac == 1 ? "on ac)  " : "on batt)");
        draw_bar("mem", mem);

        // how much do we have to work with? 
        int used_above_debug = 1 + HISTORY_HEIGHT + 1 + (2*2) + 1;
        int used_below_debug = 2;
        int available = term_rows - used_above_debug - used_below_debug;
        int max_debug_rows = (available > 0 ? available : 0);
        
        // are we doing tail -f like behavior?
        if (dbg_fp) {
            printf(" > tail: %s\n", argv[1]);
            int start = debug_line_count > max_debug_rows ?
                        debug_line_count - max_debug_rows : 0;
            for (int i = start; i < debug_line_count; i++) {
                printf("%.*s\n", term_cols > 1 ? term_cols - 1 : 1, debug_lines[i]);
            }
        } 
        
        // are we watching a bus ?
        if (store) {
            printf(" > bus: %s\n", argv[1]);
            uint64_t new_signal_count = splinter_get_signal_count(debug_signal_group);
            if (new_signal_count != old_signal_count) {
                splinter_enumerate_matches(debug_bloom, on_key_match, NULL);
            }
            
            // and now print the circular buffer
            for (int pulse = 0; pulse < debug_line_count; pulse ++) {
                if (debug_lines[pulse] != NULL) {
                    fprintf(stdout, "%s\n", debug_lines[pulse]);
                    fflush(stdout);
                }
            }            
            // new becomes old
            old_signal_count = new_signal_count;
        }

        // Do we have any job results from key presses? These come last.
        for (int i = 0; i < 10; i++) {
            if (job_results[i][0] != '\0') {
                printf("Job result: %s\n", job_results[i]);
            }
        }
        
        // clear anything remaining to the end of the screen to prevent artifact ghosts 
        printf("\033[J");

        fflush(stdout);
        struct timespec ts = {0, REFRESH * 1000};
        nanosleep(&ts, NULL);
    } while (1);

    splinter_enumerate_matches(debug_bloom, cause_key_unwatch, NULL);
    splinter_close();
    debug_log_destroy();
}
