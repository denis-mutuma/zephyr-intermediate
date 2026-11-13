# L6 task 1

## Guarantee

Control work starts within 10 ms. The interval starts when the timer stores
`ready_ms` and emits `ctrl_ready`. It ends when the control thread returns
from `k_msgq_get` and emits `ctrl_done`. A sample is a miss when that
interval is greater than 10 ms.

## Workload

Both console logs use the same application settings: an event every 250 ms,
`k_busy_wait(45000)` in maintenance, and the 10 ms detector. CTF tracing was
enabled. The board is `native_sim` on Zephyr 4.4.0. `k_uptime` steps by 10 ms
because the tick rate is 100 Hz, so a 45 ms busy-wait shows up as a 40 ms
latency.

## Root cause

The timer readies both threads. Maintenance has priority 4 and control has
priority 7, so maintenance is the higher priority thread. The trace for
sequence 0 shows the order:

- `thread_sched_ready` for control, then for maintenance
- `thread_switched_in` names maintenance, not control
- `thread_busy_wait_enter` waits 45000 µs
- sys-tick interrupts return to maintenance
- only after `thread_busy_wait_exit` does `thread_switched_in` name control
- `ctrl_done` reports latency 40

Control cannot preempt the busy-wait, so every sample misses. The miss count
in `latency-original.txt` climbs from 1 to 20. `kernel thread list` shows
control at priority 7 and maintenance at priority 4, both `pending` between
events.

## Correction

Control is priority 4 and maintenance is priority 7. The scheduler runs
control when the timer interrupt returns, and control dequeues the event
before maintenance starts the busy-wait. The period and the busy-wait are
unchanged. `latency-corrected.txt` shows latency 0 ms and a miss count that
stays 0. `kernel-thread-list-corrected.txt` shows the new priorities.

The trace file is a local Babeltrace decode of the failing run. It is not an
instructor-provided capture.
