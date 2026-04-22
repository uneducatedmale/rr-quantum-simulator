#include "parser.h"
#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *input_path;
    int quantum;
    int context_switch_cost;
    int trace;
    int animate;
    int delay_ms;
    int gantt_width;
    int color;
    int clear;
    int compact;
    int log_lines;
    int style;
    int screen_w;
    int screen_h;
    int policy;
    int policy_set;
    int best_by;
    int best_mode;
    int best_by_set;
    int best_mode_set;
    const char *csv_path;
    int generate;
    int gen_kind;
    int gen_n;
    unsigned int gen_seed;
    const char *gen_out_path;
    int sweep_start;
    int sweep_end;
    int use_sweep;
} CliOptions;

static void print_usage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s --input <file> --quantum <n> [--cs <n>] [--trace]\n", program_name);
    printf("  %s --input <file> --quantum <n> [--cs <n>] [--policy rr|fcfs]\n", program_name);
    printf("  %s --input <file> --quantum <n> [--cs <n>] --trace\n", program_name);
    printf("  %s --input <file> --quantum <n> [--cs <n>] --animate [--style hud|theatre] [--screen <WxH>] [--delay <ms>] [--gantt <cols>] [--no-color] [--no-clear] [--compact] [--log <n>]\n", program_name);
    printf("  %s --input <file> --sweep <start:end> [--cs <n>] [--best-by waiting|turnaround|response|throughput|util|ctxsw|totaltime] [--best-mode min|max] [--csv <path>]\n", program_name);
    printf("  %s --gen cpu|interactive|mixed --n <count> [--seed <n>] [--out <path>]\n", program_name);
}

static int parse_sweep_range(const char *value, int *start_out, int *end_out) {
    return sscanf(value, "%d:%d", start_out, end_out) == 2 && *start_out > 0 && *end_out >= *start_out;
}

static int parse_screen(const char *value, int *w_out, int *h_out) {
    int w = 0;
    int h = 0;

    if (sscanf(value, "%dx%d", &w, &h) != 2) {
        return 0;
    }
    if (w < 60 || w > 240 || h < 18 || h > 80) {
        return 0;
    }
    *w_out = w;
    *h_out = h;
    return 1;
}

static int parse_style(const char *value, int *style_out) {
    if (strcmp(value, "hud") == 0) {
        *style_out = 0;
        return 1;
    }
    if (strcmp(value, "theatre") == 0) {
        *style_out = 1;
        return 1;
    }
    return 0;
}

static int parse_policy(const char *value, int *policy_out) {
    if (strcmp(value, "rr") == 0) {
        *policy_out = 0;
        return 1;
    }
    if (strcmp(value, "fcfs") == 0) {
        *policy_out = 1;
        return 1;
    }
    return 0;
}

static int parse_best_by(const char *value, int *best_by_out) {
    if (strcmp(value, "waiting") == 0) {
        *best_by_out = 0;
        return 1;
    }
    if (strcmp(value, "turnaround") == 0) {
        *best_by_out = 1;
        return 1;
    }
    if (strcmp(value, "response") == 0) {
        *best_by_out = 2;
        return 1;
    }
    if (strcmp(value, "throughput") == 0) {
        *best_by_out = 3;
        return 1;
    }
    if (strcmp(value, "util") == 0) {
        *best_by_out = 4;
        return 1;
    }
    if (strcmp(value, "ctxsw") == 0) {
        *best_by_out = 5;
        return 1;
    }
    if (strcmp(value, "totaltime") == 0) {
        *best_by_out = 6;
        return 1;
    }
    return 0;
}

static int parse_best_mode(const char *value, int *best_mode_out) {
    if (strcmp(value, "min") == 0) {
        *best_mode_out = 0;
        return 1;
    }
    if (strcmp(value, "max") == 0) {
        *best_mode_out = 1;
        return 1;
    }
    return 0;
}

static int parse_gen_kind(const char *value, int *gen_kind_out) {
    if (strcmp(value, "cpu") == 0) {
        *gen_kind_out = 0;
        return 1;
    }
    if (strcmp(value, "interactive") == 0) {
        *gen_kind_out = 1;
        return 1;
    }
    if (strcmp(value, "mixed") == 0) {
        *gen_kind_out = 2;
        return 1;
    }
    return 0;
}

static unsigned int lcg_next(unsigned int *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static int gen_range(unsigned int *state, int lo, int hi) {
    unsigned int x;
    int span;
    if (hi <= lo) {
        return lo;
    }
    x = lcg_next(state);
    span = hi - lo + 1;
    return lo + (int)(x % (unsigned int)span);
}

static int write_generated_workload(FILE *out, int kind, int n, unsigned int seed) {
    int i;
    unsigned int st = seed;

    if (n <= 0) {
        return 0;
    }

    fprintf(out, "# generated workload (pid arrival burst)\n");
    fprintf(out, "# kind=%s n=%d seed=%u\n",
            kind == 0 ? "cpu" : (kind == 1 ? "interactive" : "mixed"),
            n,
            seed);

    for (i = 0; i < n; i++) {
        int arrival;
        int burst;

        if (kind == 0) {
            /* CPU-bound: mostly long bursts, sparse arrivals. */
            arrival = i * gen_range(&st, 0, 2);
            burst = gen_range(&st, 18, 60);
        } else if (kind == 1) {
            /* Interactive: many short jobs, frequent arrivals. */
            arrival = i;
            burst = gen_range(&st, 1, 4);
        } else {
            /* Mixed: staggered arrivals, varied bursts. */
            arrival = i * gen_range(&st, 0, 2);
            burst = gen_range(&st, 1, 30);
        }

        fprintf(out, "P%d %d %d\n", i + 1, arrival, burst);
    }

    return 1;
}

static int parse_args(int argc, char **argv, CliOptions *options) {
    int i;

    options->input_path = NULL;
    options->quantum = 0;
    options->context_switch_cost = 0;
    options->trace = 0;
    options->animate = 0;
    options->delay_ms = 120;
    options->gantt_width = 64;
    options->color = 1;
    options->clear = 1;
    options->compact = 0;
    options->log_lines = 5;
    options->style = 0;
    options->screen_w = 100;
    options->screen_h = 28;
    options->policy = 0;
    options->policy_set = 0;
    options->best_by = 1;
    options->best_mode = -1;
    options->best_by_set = 0;
    options->best_mode_set = 0;
    options->csv_path = NULL;
    options->generate = 0;
    options->gen_kind = 2;
    options->gen_n = 0;
    options->gen_seed = 1u;
    options->gen_out_path = NULL;
    options->sweep_start = 0;
    options->sweep_end = 0;
    options->use_sweep = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            options->input_path = argv[++i];
        } else if (strcmp(argv[i], "--quantum") == 0 && i + 1 < argc) {
            options->quantum = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--cs") == 0 && i + 1 < argc) {
            options->context_switch_cost = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            if (!parse_policy(argv[++i], &options->policy)) {
                return 0;
            }
            options->policy_set = 1;
        } else if (strcmp(argv[i], "--trace") == 0) {
            options->trace = 1;
        } else if (strcmp(argv[i], "--animate") == 0) {
            options->animate = 1;
        } else if (strcmp(argv[i], "--style") == 0 && i + 1 < argc) {
            if (!parse_style(argv[++i], &options->style)) {
                return 0;
            }
            options->animate = 1;
        } else if (strcmp(argv[i], "--screen") == 0 && i + 1 < argc) {
            if (!parse_screen(argv[++i], &options->screen_w, &options->screen_h)) {
                return 0;
            }
            options->animate = 1;
        } else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            options->delay_ms = atoi(argv[++i]);
            options->animate = 1;
        } else if (strcmp(argv[i], "--gantt") == 0 && i + 1 < argc) {
            options->gantt_width = atoi(argv[++i]);
            options->animate = 1;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            options->color = 0;
        } else if (strcmp(argv[i], "--no-clear") == 0) {
            options->clear = 0;
            options->animate = 1;
        } else if (strcmp(argv[i], "--compact") == 0) {
            options->compact = 1;
            options->animate = 1;
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            options->log_lines = atoi(argv[++i]);
            options->animate = 1;
        } else if (strcmp(argv[i], "--sweep") == 0 && i + 1 < argc) {
            options->use_sweep = parse_sweep_range(argv[++i], &options->sweep_start, &options->sweep_end);
        } else if (strcmp(argv[i], "--best-by") == 0 && i + 1 < argc) {
            if (!parse_best_by(argv[++i], &options->best_by)) {
                return 0;
            }
            options->best_by_set = 1;
        } else if (strcmp(argv[i], "--best-mode") == 0 && i + 1 < argc) {
            if (!parse_best_mode(argv[++i], &options->best_mode)) {
                return 0;
            }
            options->best_mode_set = 1;
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            options->csv_path = argv[++i];
        } else if (strcmp(argv[i], "--gen") == 0 && i + 1 < argc) {
            if (!parse_gen_kind(argv[++i], &options->gen_kind)) {
                return 0;
            }
            options->generate = 1;
        } else if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            options->gen_n = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            options->gen_seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->gen_out_path = argv[++i];
        } else {
            return 0;
        }
    }

    if (options->context_switch_cost < 0 || options->delay_ms < 0) {
        return 0;
    }

    if (options->generate) {
        return options->gen_n > 0;
    }

    if (options->input_path == NULL) {
        return 0;
    }

    if (options->use_sweep) {
        if (options->policy != 0) {
            return 0;
        }
        return 1;
    }

    if (options->animate && options->trace) {
        return 0;
    }

    if (options->gantt_width < 10 || options->gantt_width > 240) {
        return 0;
    }

    if (options->log_lines < 0 || options->log_lines > 12) {
        return 0;
    }

    if (options->policy != 0) {
        if (options->animate || options->trace) {
            return 0;
        }
        return 1;
    }

    return options->quantum > 0;
}

static int run_single(const Process *processes, int count, const CliOptions *options) {
    SimulationConfig config;
    SimulationResult result;

    config.quantum = options->quantum;
    config.context_switch_cost = options->context_switch_cost;
    config.trace = options->trace;
    config.animate = options->animate;
    config.delay_ms = options->delay_ms;
    config.gantt_width = options->gantt_width;
    config.color = options->color;
    config.clear = options->clear;
    config.compact = options->compact;
    config.log_lines = options->log_lines;
    config.style = options->style;
    config.screen_w = options->screen_w;
    config.screen_h = options->screen_h;

    if (options->policy == 0) {
        if (!run_round_robin(processes, count, &config, &result)) {
            return 0;
        }
    } else {
        config.quantum = 0;
        config.trace = 0;
        config.animate = 0;
        if (!run_fcfs(processes, count, &config, &result)) {
            return 0;
        }
    }

    printf("Input file: %s\n", options->input_path);
    if (options->policy_set || options->policy != 0) {
        printf("Policy    : %s\n", options->policy == 0 ? "RR" : "FCFS");
    }
    if (options->policy == 0) {
        printf("Quantum   : %d\n", options->quantum);
    }
    printf("CS cost   : %d\n", options->context_switch_cost);
    print_simulation_result(&result);
    return 1;
}

static const char *best_by_name(int best_by) {
    switch (best_by) {
        case 0:
            return "average waiting time";
        case 1:
            return "average turnaround time";
        case 2:
            return "average response time";
        case 3:
            return "throughput";
        case 4:
            return "CPU utilization";
        case 5:
            return "context switches";
        case 6:
            return "total time";
        default:
            return "average turnaround time";
    }
}

static int default_best_mode_for(int best_by) {
    if (best_by == 3 || best_by == 4) {
        return 1; /* max */
    }
    return 0; /* min */
}

static double objective_value(const SimulationResult *r, int best_by) {
    switch (best_by) {
        case 0:
            return r->average_waiting_time;
        case 1:
            return r->average_turnaround_time;
        case 2:
            return r->average_response_time;
        case 3:
            return r->throughput;
        case 4:
            return r->cpu_utilization;
        case 5:
            return (double)r->context_switches;
        case 6:
            return (double)r->total_time;
        default:
            return r->average_turnaround_time;
    }
}

static int run_sweep(const Process *processes, int count, const CliOptions *options) {
    int quantum;
    int best_quantum = -1;
    double best_value = 0.0;
    int best_mode = options->best_mode >= 0 ? options->best_mode : default_best_mode_for(options->best_by);
    FILE *csv = NULL;
    int show_best = options->best_by_set || options->best_mode_set || options->csv_path != NULL;

    printf("Input file: %s\n", options->input_path);
    printf("Context switch cost: %d\n", options->context_switch_cost);
    if (show_best) {
        printf("Best-by: %s (%s)\n", best_by_name(options->best_by), best_mode == 0 ? "min" : "max");
    }
    printf("\n");
    printf("%-8s %-12s %-12s %-12s %-12s %-12s\n",
           "Quantum", "AvgWait", "AvgTurn", "AvgResp", "CtxSwitch", "TotalTime");

    if (options->csv_path != NULL) {
        csv = fopen(options->csv_path, "w");
        if (csv == NULL) {
            return 0;
        }
        fprintf(csv, "quantum,avg_wait,avg_turn,avg_resp,ctx_switches,total_time,throughput,cpu_util\n");
    }

    for (quantum = options->sweep_start; quantum <= options->sweep_end; quantum++) {
        SimulationConfig config;
        SimulationResult result;
        double value;

        config.quantum = quantum;
        config.context_switch_cost = options->context_switch_cost;
        config.trace = 0;
        config.animate = 0;
        config.delay_ms = 0;
        config.gantt_width = 0;
        config.color = 0;
        config.clear = 0;
        config.compact = 0;
        config.log_lines = 0;
        config.style = 0;
        config.screen_w = 0;
        config.screen_h = 0;

        if (!run_round_robin(processes, count, &config, &result)) {
            if (csv != NULL) {
                fclose(csv);
            }
            return 0;
        }

        printf("%-8d %-12.2f %-12.2f %-12.2f %-12d %-12d\n",
               quantum,
               result.average_waiting_time,
               result.average_turnaround_time,
               result.average_response_time,
               result.context_switches,
               result.total_time);

        if (csv != NULL) {
            fprintf(csv, "%d,%.4f,%.4f,%.4f,%d,%d,%.6f,%.4f\n",
                    quantum,
                    result.average_waiting_time,
                    result.average_turnaround_time,
                    result.average_response_time,
                    result.context_switches,
                    result.total_time,
                    result.throughput,
                    result.cpu_utilization);
        }

        value = objective_value(&result, options->best_by);
        if (best_quantum < 0) {
            best_value = value;
            best_quantum = quantum;
        } else if (best_mode == 0 && value < best_value) {
            best_value = value;
            best_quantum = quantum;
        } else if (best_mode == 1 && value > best_value) {
            best_value = value;
            best_quantum = quantum;
        }
    }

    if (csv != NULL) {
        fclose(csv);
    }

    if (show_best) {
        printf("\nBest quantum by %s (%s): %d\n", best_by_name(options->best_by), best_mode == 0 ? "min" : "max", best_quantum);
    } else {
        printf("\nBest quantum by average turnaround time: %d\n", best_quantum);
    }
    return 1;
}

int main(int argc, char **argv) {
    CliOptions options;
    Process *processes = NULL;
    int count = 0;
    int ok;
    FILE *gen_out;

    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }

    if (options.generate) {
        if (options.gen_out_path != NULL) {
            gen_out = fopen(options.gen_out_path, "w");
            if (gen_out == NULL) {
                fprintf(stderr, "Failed to open output path %s\n", options.gen_out_path);
                return 1;
            }
        } else {
            gen_out = stdout;
        }

        ok = write_generated_workload(gen_out, options.gen_kind, options.gen_n, options.gen_seed);
        if (gen_out != stdout) {
            fclose(gen_out);
        }

        if (!ok) {
            fprintf(stderr, "Failed to generate workload\n");
            return 1;
        }
        return 0;
    }

    if (!load_workload(options.input_path, &processes, &count)) {
        fprintf(stderr, "Failed to load workload from %s\n", options.input_path);
        return 1;
    }

    if (options.use_sweep) {
        ok = run_sweep(processes, count, &options);
    } else {
        ok = run_single(processes, count, &options);
    }

    free(processes);

    if (!ok) {
        fprintf(stderr, "Simulation failed\n");
        return 1;
    }

    return 0;
}
