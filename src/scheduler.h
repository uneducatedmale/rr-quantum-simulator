#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

typedef struct {
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
} SimulationConfig;

typedef struct {
    double average_waiting_time;
    double average_turnaround_time;
    double average_response_time;
    double throughput;
    double cpu_utilization;
    int context_switches;
    int total_time;
} SimulationResult;

int run_round_robin(const Process *input, int count, const SimulationConfig *config, SimulationResult *result);
int run_fcfs(const Process *input, int count, const SimulationConfig *config, SimulationResult *result);
void print_simulation_result(const SimulationResult *result);

#endif
