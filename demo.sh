#!/usr/bin/env bash
set -euo pipefail

#
# Demo flow:
#   1. prove the build/test path still works
#   2. show how q changes the numbers
#   3. show that "best" depends on the metric
#   4. use animation to make the scheduler behavior intuitive
#   5. finish with an FCFS baseline
#

reset_terminal() {
  printf '\033[0m\033[?25h\033[3J\033[2J\033[H'
  stty sane 2>/dev/null || true
}

pause() {
  # Only prompt when the script is running in a real terminal.
  if [[ ! -t 0 ]]; then
    return
  fi
  printf '\033[0m\033[?25h\n'
  read -r -p "Press Enter to continue..." _ || true
  reset_terminal
}

reset_terminal
echo "== Build + tests =="
make clean
make
make test
pause

reset_terminal
echo "== Main sweep: interactive workload with context-switch cost =="
echo
echo "Summary:"
echo "Same workload, same switch cost, different quantum values."
echo
echo "Small q means a lot of preemption, so the context-switch count shoots up."
echo "Because cs=2 is charged every time, that overhead drives total time up fast."
echo "As q gets larger, the switch count falls and the wait/turnaround numbers improve."
echo
./rrsim --input workloads/interactive-bursty.txt --sweep 1:10 --cs 2
pause

reset_terminal
echo "== Best by response time =="
echo
echo "Summary:"
echo "Now the simulator picks the best quantum using response time only."
echo
echo "Response time is how long a process waits before it gets CPU for the first time."
echo "That usually favors a smaller quantum, because the scheduler rotates through READY"
echo "more often and lets new arrivals start sooner."
echo
./rrsim --input workloads/interactive-bursty.txt --sweep 1:10 --cs 2 --best-by response --best-mode min
pause

reset_terminal
echo "== Best by throughput =="
echo
echo "Summary:"
echo "Same data again, but this time 'best' means highest throughput."
echo
echo "Throughput rewards finishing the overall workload efficiently."
echo "That usually favors a larger quantum, because less time is wasted on switching."
echo "This is the key point of the project: the best q changes with the metric."
echo
./rrsim --input workloads/interactive-bursty.txt --sweep 1:10 --cs 2 --best-by throughput --best-mode max
pause

reset_terminal
echo "== Live animation (theatre mode) =="
echo
echo "Summary:"
echo "This is the visual version of the same scheduling logic."
echo
echo "The CPU view shows what is running now."
echo "The belt is the READY queue."
echo "The TIME strip is the recent execution history."
echo "The event log shows arrivals, quantum expirations, and finishes."
echo
echo "Tip: use Ctrl+C to stop the animation and continue the script."
set +e
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1 --animate --style theatre --screen 110x30 --delay 40
set -e
pause

reset_terminal
echo "== Baseline comparison: FCFS vs RR =="
echo
echo "Summary:"
echo "This compares the same workload under FCFS and Round Robin."
echo
echo "FCFS usually wins on fewer switches and can look better on completion-oriented metrics."
echo "RR usually helps fairness and responsiveness because it slices CPU time across jobs."
echo "The point is not that one policy always wins; it depends on what the system cares about."
echo
./rrsim --input workloads/big-mix.txt --policy fcfs --cs 1
echo
./rrsim --input workloads/big-mix.txt --quantum 3 --cs 1
pause

echo
echo "Done."
