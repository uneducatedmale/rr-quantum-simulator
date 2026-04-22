#define _POSIX_C_SOURCE 199309L

#include "scheduler.h"

#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

static const char *ANSI_RESET = "\033[0m";

static void print_trace_dispatch(int time, const Process *process) {
    printf("t=%d dispatch %s\n", time, process->pid);
}

static void print_trace_finish(int time, const Process *process) {
    printf("t=%d finish %s\n", time, process->pid);
}

static void print_trace_requeue(int time, const Process *process) {
    printf("t=%d quantum-expire %s\n", time, process->pid);
}

static const char *color_for_index(int index) {
    static const char *colors[] = {
        "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m",
        "\033[91m", "\033[92m", "\033[93m", "\033[94m", "\033[95m", "\033[96m",
    };
    return colors[index % (int)(sizeof(colors) / sizeof(colors[0]))];
}

static void sleep_ms(int delay_ms) {
    if (delay_ms <= 0) {
        return;
    }
#ifdef _WIN32
    Sleep((DWORD)delay_ms);
#else
    struct timespec ts;
    ts.tv_sec = delay_ms / 1000;
    ts.tv_nsec = (long)(delay_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static void clear_screen(void) {
    printf("\033[H\033[2J");
}

static void cursor_hide(void) {
    printf("\033[?25l");
}

static void cursor_show(void) {
    printf("\033[?25h");
}

typedef struct {
    int w;
    int h;
    char *cells;
    int *colors;
} Canvas;

static int canvas_init(Canvas *c, int w, int h) {
    c->w = 0;
    c->h = 0;
    c->cells = NULL;
    c->colors = NULL;

    if (w <= 0 || h <= 0) {
        return 0;
    }
    c->cells = (char *)malloc((size_t)w * (size_t)h);
    if (c->cells == NULL) {
        return 0;
    }
    c->colors = (int *)malloc(sizeof(int) * (size_t)w * (size_t)h);
    if (c->colors == NULL) {
        free(c->cells);
        c->cells = NULL;
        return 0;
    }
    c->w = w;
    c->h = h;
    memset(c->cells, ' ', (size_t)w * (size_t)h);
    {
        int i;
        for (i = 0; i < w * h; i++) {
            c->colors[i] = -1;
        }
    }
    return 1;
}

static void canvas_free(Canvas *c) {
    free(c->cells);
    free(c->colors);
    c->cells = NULL;
    c->colors = NULL;
    c->w = 0;
    c->h = 0;
}

static void canvas_clear(Canvas *c, char fill) {
    memset(c->cells, fill, (size_t)c->w * (size_t)c->h);
    {
        int i;
        for (i = 0; i < c->w * c->h; i++) {
            c->colors[i] = -1;
        }
    }
}

static void canvas_put(Canvas *c, int x, int y, char ch) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) {
        return;
    }
    c->cells[(size_t)y * (size_t)c->w + (size_t)x] = ch;
}

static void canvas_put_colored(Canvas *c, int x, int y, char ch, int color_code) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) {
        return;
    }
    c->cells[(size_t)y * (size_t)c->w + (size_t)x] = ch;
    c->colors[(size_t)y * (size_t)c->w + (size_t)x] = color_code;
}

static void canvas_put_str_colored(Canvas *c, int x, int y, const char *s, int color_code) {
    int i;
    if (s == NULL) {
        return;
    }
    for (i = 0; s[i] != '\0'; i++) {
        canvas_put_colored(c, x + i, y, s[i], color_code);
    }
}

static void canvas_box(Canvas *c, int x0, int y0, int x1, int y1) {
    int x;
    int y;
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (x = x0 + 1; x < x1; x++) {
        canvas_put(c, x, y0, '-');
        canvas_put(c, x, y1, '-');
    }
    for (y = y0 + 1; y < y1; y++) {
        canvas_put(c, x0, y, '|');
        canvas_put(c, x1, y, '|');
    }
    canvas_put(c, x0, y0, '+');
    canvas_put(c, x1, y0, '+');
    canvas_put(c, x0, y1, '+');
    canvas_put(c, x1, y1, '+');
}

static const char *ansi_for_canvas_color(int code) {
    switch (code) {
        case -1:
            return NULL;
        case 100:
            return "\033[97m";
        case 101:
            return "\033[96m";
        case 102:
            return "\033[93m";
        case 103:
            return "\033[92m";
        case 104:
            return "\033[94m";
        case 105:
            return "\033[95m";
        case 106:
            return "\033[90m";
        default:
            return color_for_index(code);
    }
}

static void canvas_print(const Canvas *c) {
    int y;
    for (y = 0; y < c->h; y++) {
        int x;
        int active = -2;
        for (x = 0; x < c->w; x++) {
            int idx = y * c->w + x;
            int code = c->colors[idx];
            if (code != active) {
                if (active != -2) {
                    fputs(ANSI_RESET, stdout);
                }
                if (code >= -1) {
                    const char *ansi = ansi_for_canvas_color(code);
                    if (ansi != NULL) {
                        fputs(ansi, stdout);
                    }
                }
                active = code;
            }
            fputc(c->cells[idx], stdout);
        }
        if (active != -2) {
            fputs(ANSI_RESET, stdout);
        }
        fputc('\n', stdout);
    }
}

static void enqueue_arrivals(Process *processes, int count, int *next_arrival, int current_time, IntQueue *ready) {
    while (*next_arrival < count && processes[*next_arrival].arrival_time <= current_time) {
        queue_push(ready, *next_arrival);
        (*next_arrival)++;
    }
}

static char symbol_for_process(int index) {
    static const char *alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int n = (int)strlen(alphabet);
    if (index < 0 || index >= n) {
        return '?';
    }
    return alphabet[index];
}

static int index_for_symbol(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return 36 + (c - 'a');
    }
    return -1;
}

typedef struct {
    char *buf;
    int width;
    int pos;
    int filled;
} Gantt;

typedef struct {
    char *lines;
    int capacity;
    int width;
    int head;
    int count;
} EventLog;

static int eventlog_init(EventLog *log, int capacity, int width) {
    log->lines = NULL;
    log->capacity = 0;
    log->width = 0;
    log->head = 0;
    log->count = 0;

    if (capacity <= 0 || width <= 1) {
        return 1;
    }

    log->lines = (char *)malloc((size_t)capacity * (size_t)width);
    if (log->lines == NULL) {
        return 0;
    }
    memset(log->lines, 0, (size_t)capacity * (size_t)width);
    log->capacity = capacity;
    log->width = width;
    return 1;
}

static void eventlog_free(EventLog *log) {
    free(log->lines);
    log->lines = NULL;
    log->capacity = 0;
    log->width = 0;
    log->head = 0;
    log->count = 0;
}

static void eventlog_add(EventLog *log, const char *fmt, ...) {
    va_list args;
    int slot;
    char *dest;

    if (log->lines == NULL || log->capacity <= 0 || log->width <= 1) {
        return;
    }

    slot = (log->head + log->count) % log->capacity;
    if (log->count == log->capacity) {
        log->head = (log->head + 1) % log->capacity;
        slot = (log->head + log->count - 1) % log->capacity;
    } else {
        log->count++;
    }

    dest = log->lines + (size_t)slot * (size_t)log->width;
    memset(dest, 0, (size_t)log->width);
    va_start(args, fmt);
    vsnprintf(dest, (size_t)log->width, fmt, args);
    va_end(args);
}

static int gantt_init(Gantt *g, int width) {
    g->buf = (char *)malloc((size_t)width);
    if (g->buf == NULL) {
        return 0;
    }
    memset(g->buf, ' ', (size_t)width);
    g->width = width;
    g->pos = 0;
    g->filled = 0;
    return 1;
}

static void gantt_free(Gantt *g) {
    free(g->buf);
    g->buf = NULL;
    g->width = 0;
    g->pos = 0;
    g->filled = 0;
}

static void gantt_append(Gantt *g, char c) {
    if (g->width <= 0) {
        return;
    }
    g->buf[g->pos] = c;
    g->pos = (g->pos + 1) % g->width;
    if (g->filled < g->width) {
        g->filled++;
    }
}

static void print_gantt(const SimulationConfig *config, const Gantt *g) {
    int i;
    int start;
    int out_len;

    printf("GANTT: ");
    if (g->filled < g->width) {
        start = 0;
        out_len = g->filled;
        for (i = 0; i < g->width - g->filled; i++) {
            putchar(' ');
        }
    } else {
        start = g->pos;
        out_len = g->width;
    }

    for (i = 0; i < out_len; i++) {
        char c = g->buf[(start + i) % g->width];
        if (config->color && c != ' ' && c != '.' && c != '|') {
            int idx = -1;
            if (c >= '0' && c <= '9') {
                idx = c - '0';
            } else if (c >= 'A' && c <= 'Z') {
                idx = 10 + (c - 'A');
            } else if (c >= 'a' && c <= 'z') {
                idx = 36 + (c - 'a');
            }
            if (idx >= 0) {
                fputs(color_for_index(idx), stdout);
                putchar(c);
                fputs(ANSI_RESET, stdout);
            } else {
                putchar(c);
            }
        } else if (c == '.') {
            putchar('.');
        } else if (c == '|') {
            putchar('|');
        } else {
            putchar(c);
        }
    }

    printf("\n");
}

static void print_gantt_scale(const Gantt *g) {
    int i;
    printf("      ");
    for (i = 0; i < g->width; i++) {
        putchar((i % 10) == 0 ? '+' : ((i % 5) == 0 ? '|' : '.'));
    }
    printf("\n");
}

static void print_ready_queue(const Process *processes, const IntQueue *ready) {
    int i;
    int n = queue_size(ready);

    printf("[");
    for (i = 0; i < n; i++) {
        int idx;
        if (!queue_at(ready, i, &idx)) {
            continue;
        }
        printf("%s", processes[idx].pid);
        if (i + 1 < n) {
            printf(" ");
        }
    }
    printf("]");
}

static void build_ready_flags(const IntQueue *ready, int count, int *flags) {
    int i;
    int n = queue_size(ready);
    for (i = 0; i < count; i++) {
        flags[i] = 0;
    }
    for (i = 0; i < n; i++) {
        int idx;
        if (queue_at(ready, i, &idx) && idx >= 0 && idx < count) {
            flags[idx] = 1;
        }
    }
}

static void print_progress_bar(int width, int done, int total) {
    int filled;
    int i;

    if (total <= 0) {
        printf("[");
        for (i = 0; i < width; i++) {
            putchar(' ');
        }
        printf("]");
        return;
    }

    if (done < 0) {
        done = 0;
    }
    if (done > total) {
        done = total;
    }

    filled = (int)((double)done / (double)total * (double)width + 0.5);
    if (filled < 0) {
        filled = 0;
    }
    if (filled > width) {
        filled = width;
    }

    printf("[");
    for (i = 0; i < width; i++) {
        putchar(i < filled ? '#' : '.');
    }
    printf("]");
}

static void render_frame(const SimulationConfig *config,
                         int current_time,
                         const Process *running,
                         const char *running_label,
                         int quantum_left,
                         int cs_left,
                         const IntQueue *ready,
                         const Process *processes,
                         int count,
                         int completed,
                         const Gantt *gantt,
                         const EventLog *log,
                         int context_switches_so_far,
                         int busy_time_so_far) {
    int i;
    int *ready_flags;
    int shown_legend = 0;

    if (!config->animate) {
        return;
    }

    if (config->style == 1) {
        Canvas canvas;
        int w = config->screen_w > 0 ? config->screen_w : 100;
        int h = config->screen_h > 0 ? config->screen_h : 28;
        int cx = w / 2;
        int cy = 9;
        int r = 7;
        int ring_n = 48;
        int ring_i;
        int belt_y = h - 4;
        int belt_x0 = 2;
        int belt_x1 = w - 3;
        int belt_len = belt_x1 - belt_x0 + 1;
        int i;
        int show_log = 0;

        if (!canvas_init(&canvas, w, h)) {
            return;
        }
        canvas_clear(&canvas, ' ');

        /* Header */
        {
            char header[256];
            snprintf(header, sizeof(header), "t=%d  q=%d  cs=%d  done=%d/%d  ready=%d  ctxsw=%d  util=%.1f%%",
                     current_time,
                     config->quantum,
                     config->context_switch_cost,
                     completed,
                     count,
                     queue_size(ready),
                     context_switches_so_far,
                     current_time > 0 ? ((double)busy_time_so_far / (double)current_time) * 100.0 : 0.0);
            canvas_put_str_colored(&canvas, 2, 0, header, 100);
        }

        if (gantt != NULL) {
            int start;
            int out_len;
            int x = 2;
            canvas_put_str_colored(&canvas, x, 2, "TIME:", 101);
            x += 6;
            if (gantt->filled < gantt->width) {
                start = 0;
                out_len = gantt->filled;
            } else {
                start = gantt->pos;
                out_len = gantt->width;
            }
            for (i = 0; i < out_len && x + i < w - 2; i++) {
                char c = gantt->buf[(start + i) % gantt->width];
                if (c == '|') {
                    canvas_put_colored(&canvas, x + i, 2, c, 102);
                } else if (c == ' ') {
                    canvas_put(&canvas, x + i, 2, c);
                } else {
                    int idx = -1;
                    if (c >= '0' && c <= '9') {
                        idx = c - '0';
                    } else if (c >= 'A' && c <= 'Z') {
                        idx = 10 + (c - 'A');
                    } else if (c >= 'a' && c <= 'z') {
                        idx = 36 + (c - 'a');
                    }
                    if (idx >= 0) {
                        canvas_put_colored(&canvas, x + i, 2, c, idx);
                    } else {
                        canvas_put(&canvas, x + i, 2, c);
                    }
                }
            }
        }

        /* CPU donut (ring with moving highlight). */
        for (ring_i = 0; ring_i < ring_n; ring_i++) {
            double a = (2.0 * 3.14159265358979323846 * (double)ring_i) / (double)ring_n;
            int x = cx + (int)lround(cos(a) * (double)r);
            int y = cy + (int)lround(sin(a) * (double)r * 0.5);
            char ch = (ring_i == (current_time % ring_n)) ? '@' : 'o';
            canvas_put_colored(&canvas, x, y, ch, ch == '@' ? 102 : 101);
        }
        canvas_put_str_colored(&canvas, cx - 4, cy - 1, " CPU ", 100);
        if (running_label != NULL) {
            canvas_put_str_colored(&canvas, cx - 4, cy + 0, running_label, 103);
        } else if (running != NULL) {
            char runline[64];
            snprintf(runline, sizeof(runline), "%s", running->pid);
            canvas_put_str_colored(&canvas, cx - 4, cy + 0, runline, 104);
        } else {
            canvas_put_str_colored(&canvas, cx - 4, cy + 0, "idle", 106);
        }
        {
            char qline[64];
            if (cs_left > 0) {
                snprintf(qline, sizeof(qline), "csleft=%d", cs_left);
            } else {
                snprintf(qline, sizeof(qline), "qleft=%d", quantum_left);
            }
            canvas_put_str_colored(&canvas, cx - 6, cy + 2, qline, 102);
        }
        if (cs_left > 0) {
            /* Sparks during context switch. */
            canvas_put_colored(&canvas, cx - 2, cy - 4, '*', 102);
            canvas_put_colored(&canvas, cx + 3, cy + 4, '*', 102);
            canvas_put_colored(&canvas, cx + 6, cy - 1, '*', 102);
        }

        /* READY conveyor belt. */
        if (belt_len > 0) {
            for (i = 0; i < belt_len; i++) {
                canvas_put_colored(&canvas, belt_x0 + i, belt_y, '=', 103);
            }
            canvas_put_str_colored(&canvas, 2, belt_y - 2, "READY BELT:", 103);
            {
                int n = queue_size(ready);
                int k;
                for (k = 0; k < n; k++) {
                    int idx;
                    int pos;
                    char sym;
                    if (!queue_at(ready, k, &idx)) {
                        continue;
                    }
                    sym = symbol_for_process(idx);
                    pos = (belt_x0 + (k * 4) + (current_time % 4)) % belt_len;
                    canvas_put_colored(&canvas, belt_x0 + pos, belt_y - 1, '[', 103);
                    canvas_put_colored(&canvas, belt_x0 + pos + 1, belt_y - 1, sym, idx);
                    canvas_put_colored(&canvas, belt_x0 + pos + 2, belt_y - 1, ']', 103);
                }
            }
        }

        /* Small legend for first few processes. */
        {
            int max = count < 24 ? count : 24;
            int rows = 12;
            int col_width = 12;
            int x0 = 2;
            int y0 = 4;
            int legend_idx[24];
            int legend_count = 0;
            int used[64] = {0};
            canvas_put_str_colored(&canvas, x0, y0++, "LEGEND:", 105);

            if (running != NULL) {
                int idx = (int)(running - processes);
                if (idx >= 0 && idx < count && idx < 64) {
                    legend_idx[legend_count++] = idx;
                    used[idx] = 1;
                }
            }

            {
                int n = queue_size(ready);
                int k;
                for (k = 0; k < n && legend_count < max; k++) {
                    int idx;
                    if (!queue_at(ready, k, &idx)) {
                        continue;
                    }
                    if (idx >= 0 && idx < count && idx < 64 && !used[idx]) {
                        legend_idx[legend_count++] = idx;
                        used[idx] = 1;
                    }
                }
            }

            if (gantt != NULL) {
                int start;
                int out_len;
                if (gantt->filled < gantt->width) {
                    start = 0;
                    out_len = gantt->filled;
                } else {
                    start = gantt->pos;
                    out_len = gantt->width;
                }
                for (i = 0; i < out_len && legend_count < max; i++) {
                    char c = gantt->buf[(start + i) % gantt->width];
                    int idx = index_for_symbol(c);
                    if (idx >= 0 && idx < count && idx < 64 && !used[idx]) {
                        legend_idx[legend_count++] = idx;
                        used[idx] = 1;
                    }
                }
            }

            for (i = 0; i < count && legend_count < max; i++) {
                if (i < 64 && !used[i]) {
                    legend_idx[legend_count++] = i;
                    used[i] = 1;
                }
            }

            for (i = 0; i < legend_count; i++) {
                char line[64];
                int idx = legend_idx[i];
                char sym = symbol_for_process(idx);
                int col = i / rows;
                int row = i % rows;
                int x = x0 + (col * col_width);
                int y = y0 + row;
                if (y >= h - 6) {
                    break;
                }
                snprintf(line, sizeof(line), " %c = %s", sym, processes[idx].pid);
                canvas_put_str_colored(&canvas, x, y, line, 100);
                canvas_put_colored(&canvas, x + 1, y, sym, idx);
            }
        }

        /* Event log panel (right side). */
        if (log != NULL && log->lines != NULL && log->count > 0 && config->log_lines > 0) {
            show_log = log->count < config->log_lines ? log->count : config->log_lines;
            if (show_log > 0) {
                int panel_w = 34;
                int x0 = w - panel_w - 2;
                int y0 = 4;
                int y1 = y0 + show_log + 2;
                int start = (log->head + (log->count - show_log)) % log->capacity;
                canvas_box(&canvas, x0, y0, w - 2, y1);
                for (i = x0; i <= w - 2; i++) {
                    canvas_put_colored(&canvas, i, y0, canvas.cells[(size_t)y0 * (size_t)canvas.w + (size_t)i], 101);
                }
                canvas_put_str_colored(&canvas, x0 + 2, y0, "EVENT LOG", 100);
                for (i = 0; i < show_log; i++) {
                    const char *line = log->lines + (size_t)((start + i) % log->capacity) * (size_t)log->width;
                    canvas_put_str_colored(&canvas, x0 + 2, y0 + 1 + i, line, 100);
                }
            }
        }

        if (config->clear) {
            /* Faster than full clear; we redraw the whole frame. */
            printf("\033[H");
        }
        canvas_print(&canvas);
        canvas_free(&canvas);
        fflush(stdout);
        return;
    }

    if (config->clear) {
        clear_screen();
    } else {
        printf("\n");
    }
    printf("t=%d  q=%d  cs=%d  done=%d/%d  ready=%d  ctxsw=%d  util=%.1f%%\n",
           current_time,
           config->quantum,
           config->context_switch_cost,
           completed,
           count,
           queue_size(ready),
           context_switches_so_far,
           current_time > 0 ? ((double)busy_time_so_far / (double)current_time) * 100.0 : 0.0);
    if (gantt != NULL) {
        print_gantt(config, gantt);
        print_gantt_scale(gantt);
    }

    printf("RUN  : ");
    if (running_label != NULL) {
        printf("%s", running_label);
    } else if (running != NULL) {
        printf("%s (rem=%d)", running->pid, running->remaining_time);
    } else {
        printf("idle");
    }
    if (config->quantum > 0) {
        printf("  qleft=%d", quantum_left);
    }
    if (cs_left > 0) {
        printf("  csleft=%d", cs_left);
    }
    printf("\n");

    printf("READY: ");
    print_ready_queue(processes, ready);
    printf("\n");

    if (config->compact) {
        if (log != NULL && log->lines != NULL && log->count > 0 && config->log_lines > 0) {
            int show = log->count < config->log_lines ? log->count : config->log_lines;
            int start = (log->head + (log->count - show)) % log->capacity;
            printf("\n");
            for (i = 0; i < show; i++) {
                const char *line = log->lines + (size_t)((start + i) % log->capacity) * (size_t)log->width;
                printf("LOG  : %s\n", line);
            }
        }
        fflush(stdout);
        return;
    }

    ready_flags = (int *)malloc(sizeof(int) * (size_t)count);
    if (ready_flags == NULL) {
        fflush(stdout);
        return;
    }
    build_ready_flags(ready, count, ready_flags);

    printf("\n");
    for (i = 0; i < count; i++) {
        const Process *p = &processes[i];
        int arrived = p->arrival_time <= current_time;
        int done = p->remaining_time == 0 && p->completion_time >= 0;
        int is_running = (running != NULL) && (p == running);
        int is_ready = ready_flags[i] != 0;
        int used = p->burst_time - p->remaining_time;
        char sym = symbol_for_process(i);

        if (config->color) {
            fputs(color_for_index(i), stdout);
        }
        printf("%c %s ", sym, p->pid);
        if (config->color) {
            fputs(ANSI_RESET, stdout);
        }

        print_progress_bar(24, used, p->burst_time);
        printf(" rem=%d", p->remaining_time);

        if (!arrived) {
            printf(" NEW(arr=%d)\n", p->arrival_time);
        } else if (done) {
            printf(" DONE(t=%d)\n", p->completion_time);
        } else if (is_running) {
            printf(" RUN\n");
        } else if (is_ready) {
            printf(" READY\n");
        } else {
            printf(" WAIT\n");
        }

        if (!shown_legend && i + 1 == count) {
            shown_legend = 1;
        }
    }

    free(ready_flags);

    if (log != NULL && log->lines != NULL && log->count > 0 && config->log_lines > 0) {
        int show = log->count < config->log_lines ? log->count : config->log_lines;
        int start = (log->head + (log->count - show)) % log->capacity;
        printf("\n");
        for (i = 0; i < show; i++) {
            const char *line = log->lines + (size_t)((start + i) % log->capacity) * (size_t)log->width;
            printf("LOG  : %s\n", line);
        }
    }

    fflush(stdout);
}

int run_round_robin(const Process *input, int count, const SimulationConfig *config, SimulationResult *result) {
    SimulationConfig effective;
    const SimulationConfig *cfg;
    Process *processes;
    IntQueue ready;
    Gantt gantt;
    Gantt *gantt_ptr = NULL;
    EventLog log;
    EventLog *log_ptr = NULL;
    int shown_cursor = 0;
    int current_time = 0;
    int next_arrival = 0;
    int completed = 0;
    int busy_time = 0;
    int context_switches = 0;
    int current_index;
    int waiting;
    int turnaround;
    int response;
    double total_waiting = 0.0;
    double total_turnaround = 0.0;
    double total_response = 0.0;

    effective = *config;
    cfg = &effective;

    if (count <= 0 || cfg->quantum <= 0 || cfg->context_switch_cost < 0) {
        return 0;
    }

    processes = (Process *)malloc(sizeof(Process) * count);
    if (processes == NULL) {
        return 0;
    }
    memcpy(processes, input, sizeof(Process) * count);

    if (!queue_init(&ready, count)) {
        free(processes);
        return 0;
    }

    if (cfg->animate) {
        int width = cfg->gantt_width > 0 ? cfg->gantt_width : 64;
        if (!gantt_init(&gantt, width)) {
            queue_free(&ready);
            free(processes);
            return 0;
        }
        gantt_ptr = &gantt;

        if (!eventlog_init(&log, cfg->log_lines, 72)) {
            gantt_free(gantt_ptr);
            queue_free(&ready);
            free(processes);
            return 0;
        }
        log_ptr = &log;

        /* If stdout is not a TTY, ANSI clears often don't work; default to no-clear. */
#ifndef _WIN32
        if (isatty(fileno(stdout)) == 0) {
            effective.clear = 0;
        }
#endif
        if (cfg->clear) {
            cursor_hide();
            shown_cursor = 1;
        }
    }

    while (completed < count) {
        Process *current;
        int slice_len;
        int tick;
        int arrivals_before;
        int arrivals_after;

        arrivals_before = next_arrival;
        enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
        arrivals_after = next_arrival;
        if (log_ptr != NULL && arrivals_after > arrivals_before) {
            for (tick = arrivals_before; tick < arrivals_after; tick++) {
                eventlog_add(log_ptr, "t=%d arrive %s", current_time, processes[tick].pid);
            }
        }

        if (queue_is_empty(&ready)) {
            if (next_arrival < count) {
                if (cfg->animate) {
                    while (current_time < processes[next_arrival].arrival_time) {
                        gantt_append(gantt_ptr, ' ');
                        render_frame(cfg, current_time, NULL, NULL, cfg->quantum, 0, &ready, processes, count, completed, gantt_ptr, log_ptr, context_switches, busy_time);
                        sleep_ms(cfg->delay_ms);
                        current_time++;
                        arrivals_before = next_arrival;
                        enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
                        arrivals_after = next_arrival;
                        if (log_ptr != NULL && arrivals_after > arrivals_before) {
                            for (tick = arrivals_before; tick < arrivals_after; tick++) {
                                eventlog_add(log_ptr, "t=%d arrive %s", current_time, processes[tick].pid);
                            }
                        }
                    }
                } else {
                    current_time = processes[next_arrival].arrival_time;
                }
                arrivals_before = next_arrival;
                enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
                arrivals_after = next_arrival;
                if (log_ptr != NULL && arrivals_after > arrivals_before) {
                    for (tick = arrivals_before; tick < arrivals_after; tick++) {
                        eventlog_add(log_ptr, "t=%d arrive %s", current_time, processes[tick].pid);
                    }
                }
            } else {
                break;
            }
        }

        if (!queue_pop(&ready, &current_index)) {
            break;
        }

        current = &processes[current_index];
        if (current->has_started == 0) {
            current->has_started = 1;
            current->first_start_time = current_time;
        }

        if (cfg->trace) {
            print_trace_dispatch(current_time, current);
        }
        if (log_ptr != NULL) {
            eventlog_add(log_ptr, "t=%d dispatch %s", current_time, current->pid);
        }

        slice_len = current->remaining_time < cfg->quantum ? current->remaining_time : cfg->quantum;
        if (cfg->animate) {
            for (tick = 0; tick < slice_len; tick++) {
                gantt_append(gantt_ptr, symbol_for_process(current_index));
                render_frame(cfg, current_time, current, NULL, slice_len - tick, 0, &ready, processes, count, completed, gantt_ptr, log_ptr, context_switches, busy_time);
                sleep_ms(cfg->delay_ms);
                current->remaining_time--;
                current_time++;
                busy_time++;
                arrivals_before = next_arrival;
                enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
                arrivals_after = next_arrival;
                if (log_ptr != NULL && arrivals_after > arrivals_before) {
                    int a;
                    for (a = arrivals_before; a < arrivals_after; a++) {
                        eventlog_add(log_ptr, "t=%d arrive %s", current_time, processes[a].pid);
                    }
                }
            }
        } else {
            current->remaining_time -= slice_len;
            current_time += slice_len;
            busy_time += slice_len;
            enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
        }

        if (current->remaining_time == 0) {
            current->completion_time = current_time;
            completed++;

            if (cfg->trace) {
                print_trace_finish(current_time, current);
            }
            if (log_ptr != NULL) {
                eventlog_add(log_ptr, "t=%d finish %s", current_time, current->pid);
            }
        } else {
            queue_push(&ready, current_index);

            if (cfg->trace) {
                print_trace_requeue(current_time, current);
            }
            if (log_ptr != NULL) {
                eventlog_add(log_ptr, "t=%d expire %s", current_time, current->pid);
            }
        }

        if (completed < count && (!queue_is_empty(&ready) || next_arrival < count)) {
            if (cfg->context_switch_cost > 0) {
                context_switches++;
                if (cfg->animate) {
                    for (tick = 0; tick < cfg->context_switch_cost; tick++) {
                        gantt_append(gantt_ptr, '|');
                        render_frame(cfg, current_time, NULL, "CS", 0, cfg->context_switch_cost - tick, &ready, processes, count, completed, gantt_ptr, log_ptr, context_switches, busy_time);
                        sleep_ms(cfg->delay_ms);
                        current_time++;
                        arrivals_before = next_arrival;
                        enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
                        arrivals_after = next_arrival;
                        if (log_ptr != NULL && arrivals_after > arrivals_before) {
                            int a;
                            for (a = arrivals_before; a < arrivals_after; a++) {
                                eventlog_add(log_ptr, "t=%d arrive %s", current_time, processes[a].pid);
                            }
                        }
                    }
                } else {
                    current_time += cfg->context_switch_cost;
                    enqueue_arrivals(processes, count, &next_arrival, current_time, &ready);
                }
            } else if (!queue_is_empty(&ready) || next_arrival < count) {
                context_switches++;
            }
        }
    }

    for (current_index = 0; current_index < count; current_index++) {
        turnaround = processes[current_index].completion_time - processes[current_index].arrival_time;
        waiting = turnaround - processes[current_index].burst_time;
        response = processes[current_index].first_start_time - processes[current_index].arrival_time;
        total_turnaround += turnaround;
        total_waiting += waiting;
        total_response += response;
    }

    result->average_waiting_time = total_waiting / count;
    result->average_turnaround_time = total_turnaround / count;
    result->average_response_time = total_response / count;
    result->throughput = current_time > 0 ? (double)count / (double)current_time : 0.0;
    result->cpu_utilization = current_time > 0 ? ((double)busy_time / (double)current_time) * 100.0 : 0.0;
    result->context_switches = context_switches;
    result->total_time = current_time;

    queue_free(&ready);
    if (gantt_ptr != NULL) {
        gantt_free(gantt_ptr);
    }
    if (log_ptr != NULL) {
        eventlog_free(log_ptr);
    }
    if (shown_cursor) {
        cursor_show();
    }
    free(processes);
    return 1;
}

void print_simulation_result(const SimulationResult *result) {
    printf("+-------------------------+------------------------+\n");
    printf("| %-23s | %-22s |\n", "Metric", "Value");
    printf("+-------------------------+------------------------+\n");
    printf("| %-23s | %22.2f |\n", "Avg waiting time", result->average_waiting_time);
    printf("| %-23s | %22.2f |\n", "Avg turnaround time", result->average_turnaround_time);
    printf("| %-23s | %22.2f |\n", "Avg response time", result->average_response_time);
    printf("| %-23s | %22d |\n", "Context switches", result->context_switches);
    printf("| %-23s | %22d |\n", "Total time units", result->total_time);
    printf("| %-23s | %22.4f |\n", "Throughput", result->throughput);
    printf("| %-23s | %21.2f%% |\n", "CPU utilization", result->cpu_utilization);
    printf("+-------------------------+------------------------+\n");
}

int run_fcfs(const Process *input, int count, const SimulationConfig *config, SimulationResult *result) {
    Process *processes;
    int current_time = 0;
    int busy_time = 0;
    int context_switches = 0;
    int i;
    int waiting;
    int turnaround;
    int response;
    double total_waiting = 0.0;
    double total_turnaround = 0.0;
    double total_response = 0.0;

    if (count <= 0 || config->context_switch_cost < 0) {
        return 0;
    }

    processes = (Process *)malloc(sizeof(Process) * count);
    if (processes == NULL) {
        return 0;
    }
    memcpy(processes, input, sizeof(Process) * count);

    for (i = 0; i < count; i++) {
        Process *p = &processes[i];

        if (i > 0) {
            if (config->context_switch_cost > 0) {
                context_switches++;
                current_time += config->context_switch_cost;
            } else {
                context_switches++;
            }
        }

        if (current_time < p->arrival_time) {
            current_time = p->arrival_time;
        }

        p->has_started = 1;
        p->first_start_time = current_time;
        p->remaining_time = p->burst_time;

        current_time += p->remaining_time;
        busy_time += p->remaining_time;
        p->remaining_time = 0;
        p->completion_time = current_time;
    }

    for (i = 0; i < count; i++) {
        turnaround = processes[i].completion_time - processes[i].arrival_time;
        waiting = turnaround - processes[i].burst_time;
        response = processes[i].first_start_time - processes[i].arrival_time;
        total_turnaround += turnaround;
        total_waiting += waiting;
        total_response += response;
    }

    result->average_waiting_time = total_waiting / count;
    result->average_turnaround_time = total_turnaround / count;
    result->average_response_time = total_response / count;
    result->throughput = current_time > 0 ? (double)count / (double)current_time : 0.0;
    result->cpu_utilization = current_time > 0 ? ((double)busy_time / (double)current_time) * 100.0 : 0.0;
    result->context_switches = context_switches;
    result->total_time = current_time;

    free(processes);
    return 1;
}
