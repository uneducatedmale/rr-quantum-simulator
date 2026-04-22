#!/usr/bin/env bash
set -euo pipefail

pause() {
  if [[ ! -t 0 ]]; then
    return
  fi
  echo
  read -r -p "Press Enter to continue..." _ || true
}

echo "== Build + tests =="
make clean
make
make test
pause

echo "== Sweep: interactive-ish workload (cs=0) =="
./rrsim --input workloads/interactive-bursty.txt --sweep 1:12 --cs 0
pause

echo "== Sweep: interactive-ish workload (cs=2) =="
./rrsim --input workloads/interactive-bursty.txt --sweep 1:12 --cs 2
pause

echo "== Sweep: best quantum by response time (cs=2) =="
./rrsim --input workloads/interactive-bursty.txt --sweep 1:12 --cs 2 --best-by response --best-mode min
pause

echo "== Sweep: best quantum by throughput (cs=2) =="
./rrsim --input workloads/interactive-bursty.txt --sweep 1:12 --cs 2 --best-by throughput --best-mode max
pause

echo "== Live animation (theatre) on a larger workload =="
echo "Tip: use Ctrl+C to stop the animation and continue the script."
set +e
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1 --animate --style theatre --screen 110x30 --delay 40
set -e
pause

echo "== Baseline comparison: FCFS vs RR (same workload) =="
./rrsim --input workloads/big-mix.txt --policy fcfs --cs 1
echo
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1
pause

echo "== Optional: generate a workload (repeatable) =="
./rrsim --gen mixed --n 30 --seed 1 --out workloads/generated-30.txt
echo "Wrote workloads/generated-30.txt"
./rrsim --input workloads/generated-30.txt --sweep 1:12 --cs 1 --best-by turnaround --best-mode min

echo
echo "Done."
