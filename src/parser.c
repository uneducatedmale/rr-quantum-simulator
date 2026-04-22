#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Normalizing the workload order here keeps the scheduler loop simpler later on. */
static int compare_by_arrival_then_pid(const void *left, const void *right) {
    const Process *a = (const Process *)left;
    const Process *b = (const Process *)right;

    if (a->arrival_time != b->arrival_time) {
        return a->arrival_time - b->arrival_time;
    }

    return strcmp(a->pid, b->pid);
}

/*
 * The parser only accepts the three fields the simulator actually uses. As each
 * process is loaded, runtime bookkeeping fields are initialized so the scheduler
 * can work on a clean copy without extra setup.
 */
int load_workload(const char *path, Process **processes_out, int *count_out) {
    FILE *file;
    Process *processes;
    int capacity = 8;
    int count = 0;
    char line[256];

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    processes = (Process *)malloc(sizeof(Process) * capacity);
    if (processes == NULL) {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        Process process;
        int fields;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        fields = sscanf(line, "%31s %d %d", process.pid, &process.arrival_time, &process.burst_time);
        if (fields != 3 || process.arrival_time < 0 || process.burst_time <= 0) {
            free(processes);
            fclose(file);
            return 0;
        }

        if (count == capacity) {
            Process *grown;
            capacity *= 2;
            grown = (Process *)realloc(processes, sizeof(Process) * capacity);
            if (grown == NULL) {
                free(processes);
                fclose(file);
                return 0;
            }
            processes = grown;
        }

        /* The simulation updates these fields in-place on a copied workload. */
        process.remaining_time = process.burst_time;
        process.first_start_time = -1;
        process.completion_time = -1;
        process.has_started = 0;
        processes[count++] = process;
    }

    fclose(file);

    if (count == 0) {
        free(processes);
        return 0;
    }

    qsort(processes, count, sizeof(Process), compare_by_arrival_then_pid);
    *processes_out = processes;
    *count_out = count;
    return 1;
}
