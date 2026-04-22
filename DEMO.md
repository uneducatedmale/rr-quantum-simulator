# Demo Runbook (WSL)

From `.../OSproject`, run:

```bash
bash demo.sh
```

What it demonstrates:

- Sweeps across quantum values on an interactive-ish workload with `cs=0` vs `cs>0`
- How “best quantum” changes depending on which metric you optimize (`--best-by`)
- A live terminal animation for intuition about RR behavior
- A baseline comparison using `--policy fcfs`
- A repeatable generated workload (`--gen ... --seed ...`)

Useful direct commands:

```bash
make clean && make && make test
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1 --animate --style theatre --screen 110x30 --delay 40
./rrsim --input workloads/big-mix.txt --sweep 1:12 --cs 1 --best-by turnaround --best-mode min --csv sweep.csv
./rrsim --input workloads/interactive-bursty.txt --sweep 1:12 --cs 2 --best-by response --best-mode min
./rrsim --input workloads/big-mix.txt --policy fcfs --cs 1
```

