# xv6 Scheduling Project

Extending xv6-riscv with alternative CPU dispatchers (FCFS and CFS) plus a new `getreadcount()` service that records aggregate bytes consumed by all read operations system-wide.

## About

This repository contains kernel-level changes to the xv6 teaching OS that introduce multiple process scheduling strategies. It compares how different policies influence turnaround, waiting time, and overall throughput.

> **Course**: Operating Systems and Networks (OSN) — Mini Project 1

## What's Included

### New Service: getreadcount()
Exposes a cumulative counter of every byte fetched via `read()` since boot. Useful for I/O accounting and monitoring.

### Selectable Schedulers

| Policy | Behavior | Ideal For |
|--------|----------|-----------|
| **Round Robin** | Fixed time quantum per process (default xv6) | Interactive / general-purpose |
| **FCFS** | Non-preemptive, ordered by arrival time | Batch jobs, sequential workloads |
| **CFS** | Weighted fair scheduling via virtual runtime | Multi-tenant servers, fairness-sensitive systems |

## Repository Layout

```
.
├── kernel/                    # Scheduler and syscall implementation
├── user/readcount.c           # Demo for getreadcount()
├── user/schedulertest.c       # Comparison harness (I/O and CPU processes)
├── xv6_modifications.patch    # Consolidated patch against stock xv6
├── report.md                  # Benchmark analysis and methodology
└── README.md                  # This document
```

## Build Instructions

### Requirements
- RISC-V cross-compilation toolchain (newlib)
- QEMU emulator targeting riscv64-softmmu
- Original xv6-riscv tree from MIT

### Patching
```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
git apply /path/to/xv6_modifications.patch
```

### Selecting a Scheduler at Build Time
```bash
make clean && make qemu CPUS=1                           # Round Robin
make clean && make qemu SCHEDULER=FCFS CPUS=1           # FCFS
make clean && make qemu SCHEDULER=CFS CPUS=1            # CFS
```

The preprocessor symbol `RR`, `FCFS`, or `CFS` is injected via `-D` in `CFLAGS` based on the `SCHEDULER` variable.

---

## Design Notes

### 1. getreadcount() — How It Works

A global `uint64 total_bytes_read` lives in `kernel/kalloc.c`. Every invocation of `sys_read()` in `kernel/sysfile.c` adds the return value (bytes actually transferred) to this accumulator. The `sys_getreadcount()` handler simply returns it.

```
Key files:
  kernel/kalloc.c   → counter declaration
  kernel/sysfile.c  → counter update inside sys_read
  kernel/sysproc.c  → sys_getreadcount() implementation
  kernel/syscall.{c,h} → wiring (number 22)
  user/user.h, usys.pl → user-space stub
```

**Sample run:**
```
$ readcount
Initial read count: 12543
Final read count: 12643
Difference: 100 bytes
SUCCESS
```

### 2. FCFS Scheduler

```
make SCHEDULER=FCFS
```

Each process receives a monotonically increasing `creation_time` inside `allocproc()`. The scheduler scans the process table and selects the **runnable** process bearing the smallest timestamp. Because no `yield()` interrupt occurs, the chosen process occupies the CPU until it voluntarily relinquishes control (exit, sleep, or I/O wait).

```
for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == RUNNABLE && p->creation_time < earliest) {
        earliest = p->creation_time;
        target  = p;
    }
}
```

**Trade-off:** Simple and starvation-free for CPU-bound jobs, but short processes behind long ones suffer (convoy effect).

### 3. CFS Scheduler

```
make SCHEDULER=CFS
```

Models the Linux CFS algorithm:

- **`nice`** (`-20`..`19`) maps to a **weight** via a precomputed lookup table (`nice_to_weight[40]`).
- Each process accumulates **virtual runtime** (`vruntime += (time_slice * 1024) / weight`).
- The scheduler always picks the runnable process with the **lowest vruntime**.
- Time slices are recalculated every scheduling round: `TARGET_LATENCY / nr_runnable`, floored at `MIN_TIME_SLICE`.

```
nice  -20 → weight 88761
nice    0 → weight 1024   (baseline)
nice  +19 → weight 15
```

**Debug output** (enabled when >2 processes are runnable):
```
[Scheduler Tick]
PID: 3 | vRuntime: 200
PID: 4 | vRuntime: 150
PID: 5 | vRuntime: 180
--> Scheduling PID 4 (lowest vRuntime)
```

---

## Performance Data

`$ schedulertest` spawns 13 children (8 I/O-bound, 5 CPU-bound) and collects timing metrics.

| Scheduler | Wall Clock | I/O Wait | CPU Wait | Avg Wait | Avg Turnaround |
|-----------|-----------|----------|----------|----------|----------------|
| **RR**    | 200 ticks | 197      | 1        | 121      | 123            |
| **FCFS**  | 201 ticks | 197      | 0        | 121      | 123            |
| **CFS**   | 203 ticks | 198      | 1        | 122      | 124            |

### Observations

- **Round Robin** finished fastest — its preemptive nature keeps the pipeline moving.
- **FCFS** eliminated CPU-bound waiting entirely (no context-switch overhead for compute-heavy processes).
- **CFS** incurred a 3-tick overhead from vruntime bookkeeping but delivers proportional fairness.
- I/O-bound processes showed near-identical behavior across all three because they spend most of their lifespan blocked on disk.

### When to Use Each

| Scenario | Recommended Scheduler |
|----------|----------------------|
| Desktop / interactive | Round Robin |
| Dedicated batch processing | FCFS |
| Shared server, multi-tenant | CFS |

---

## Interactive Debugging

Pressing `Ctrl+P` inside the xv6 console triggers `procdump()`. Output varies by scheduler:

```
# Round Robin
1  sleep   init
2  sleep   sh
3  runble  myprocess

# FCFS
1  sleep   init (creation time: 1)
2  sleep   sh   (creation time: 2)
3  runble  myprocess (creation time: 5)

# CFS
1  sleep   init nice=0 vruntime=1000
2  sleep   sh   nice=0 vruntime=1200
3  runble  myprocess nice=0 vruntime=800
```

---

## Files Changed

| Path | What Was Added |
|------|----------------|
| `kernel/proc.h` | `creation_time`, `nice`, `weight`, `vruntime`, `time_slice` in `struct proc` |
| `kernel/proc.c` | Weight table, FCFS/CFS scheduling logic, extended procdump |
| `kernel/param.h` | `TARGET_LATENCY`, `MIN_TIME_SLICE` |
| `kernel/kalloc.c` | `total_bytes_read` global |
| `kernel/sysfile.c` | Read-accounting in `sys_read()` |
| `kernel/sysproc.c` | `sys_getreadcount()`, `sys_sleep()` |
| `kernel/syscall.c` | Extern declarations and dispatch table entries |
| `kernel/syscall.h` | `SYS_getreadcount` (22), `SYS_sleep` (23) |
| `user/user.h` | `getreadcount()`, `sleep()` prototypes |
| `user/usys.pl` | Stub generator entries |
| `Makefile` | `SCHEDULER` selection variable, test programs |

---

## References

- [xv6 Book (RISC-V)](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf)
- [Linux CFS Design](https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html)
- [OSTEP — Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)
- [RISC-V Privileged Architecture Specification](https://riscv.org/technical/specifications/)

---

*Course project for OSN — modifications authored for educational purposes.*
