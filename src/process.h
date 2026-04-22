#ifndef PROCESS_H
#define PROCESS_H

#define PID_MAX_LEN 31

typedef struct {
    char pid[PID_MAX_LEN + 1];
    int arrival_time;
    int burst_time;
    int remaining_time;
    int first_start_time;
    int completion_time;
    int has_started;
} Process;

#endif
