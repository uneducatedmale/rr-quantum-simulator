# Round Robin Quantum Simulator

A small C project for exploring how Round Robin time-slice choice changes scheduler behavior under different workloads.

## What It Does

- simulates Round Robin scheduling with a configurable quantum
- supports arrival times and CPU burst lengths from workload files
- applies optional context-switch cost
- reports waiting, turnaround, response, throughput, utilization, and context-switch metrics
- includes trace, sweep, and terminal animation modes
- includes an FCFS baseline for comparison

## Build

```bash
make
```

## Test

```bash
make test
```

## Workload Format

Each non-comment line is:

```text
PID ARRIVAL BURST
```

Example:

```text
P1 0 5
P2 1 3
P3 2 8
```

All timing uses the same simulator time unit across arrivals, bursts, quantum size, context-switch cost, and reported metrics.

## Common Runs

Single RR run:

```bash
./rrsim --input workloads/demo.txt --quantum 2 --cs 1
```

Trace mode:

```bash
./rrsim --input workloads/demo.txt --quantum 2 --cs 1 --trace
```

Sweep a range of quantum values:

```bash
./rrsim --input workloads/interactive-bursty.txt --sweep 1:10 --cs 2
```

Choose the best quantum by a specific metric:

```bash
./rrsim --input workloads/interactive-bursty.txt --sweep 1:10 --cs 2 --best-by response --best-mode min
./rrsim --input workloads/interactive-bursty.txt --sweep 1:10 --cs 2 --best-by throughput --best-mode max
```

FCFS baseline:

```bash
./rrsim --input workloads/big-mix.txt --policy fcfs --cs 1
```

Terminal animation:

```bash
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1 --animate --style theatre --screen 110x30 --delay 40
```

Guided demo sequence:

```bash
bash demo.sh
```

## Project Layout

- `src/main.c` - CLI, mode selection, sweep mode, workload generation
- `src/parser.c` - workload parsing and normalization
- `src/queue.c` - READY queue implementation
- `src/scheduler.c` - RR/FCFS execution, metrics, trace, animation
- `workloads/` - example inputs for testing and demos
- `tests/` - regression tests and expected outputs

