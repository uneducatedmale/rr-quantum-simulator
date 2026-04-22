# Interactive Round Robin Quantum Tuning Simulator

Prototype for a Principles of Operating Systems project.

## Features

- Round Robin scheduling with configurable quantum
- Workload input with arrival times and CPU bursts
- Optional context switch cost
- Metrics:
  - average waiting time
  - average turnaround time
  - average response time
  - throughput
  - CPU utilization
  - context switch count
- Trace mode for scheduling events
- Animated mode showing RUN/READY over time
- Sweep mode for comparing multiple quantum values

## Workload Format

Each non-comment line has:

```txt
PID ARRIVAL BURST
```

Example:

```txt
P1 0 5
P2 1 3
P3 2 8
P4 4 6
```

## Build

```bash
make
```

## Run

Single simulation:

```bash
./rrsim --input workloads/demo.txt --quantum 2 --cs 1
```

Bigger demo workloads:

```bash
./rrsim --input workloads/big-mix.txt --sweep 1:12 --cs 1
./rrsim --input workloads/interactive-bursty.txt --sweep 1:12 --cs 1
```

Baseline policy (FCFS):

```bash
./rrsim --input workloads/demo.txt --policy fcfs --cs 1
```

Trace mode:

```bash
./rrsim --input workloads/demo.txt --quantum 2 --cs 1 --trace
```

Animated mode:

```bash
./rrsim --input workloads/demo.txt --quantum 2 --cs 1 --animate --delay 80 --gantt 90
```

Tip: For the best visual effect (screen-clearing animation + color), run it in an actual terminal (WSL shell or Windows Terminal). If your host doesn’t handle ANSI clears well, use `--no-clear` or `--compact`.

Novel "theatre" animation (ASCII art CPU donut + READY belt + event log):

```bash
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1 --animate --style theatre --screen 110x30 --delay 40
```

Sweep: choose "best quantum" by a different metric and export CSV:

```bash
./rrsim --input workloads/big-mix.txt --sweep 1:12 --cs 1 --best-by response --best-mode min
./rrsim --input workloads/big-mix.txt --sweep 1:12 --cs 1 --best-by throughput --best-mode max --csv sweep.csv
```

Generate workloads (repeatable via seed):

```bash
./rrsim --gen mixed --n 30 --seed 1 --out workloads/generated-30.txt
./rrsim --input workloads/generated-30.txt --sweep 1:12 --cs 1
```

Demo script:

```bash
bash demo.sh
```

Sweep mode:

```bash
./rrsim --input workloads/demo.txt --sweep 1:8 --cs 1
```
