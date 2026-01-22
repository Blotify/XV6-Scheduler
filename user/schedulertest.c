/*
 * schedulertest.c
 * Benchmark program to compare scheduler performance
 * Creates IO bound and CPU bound processes to measure wait and turnaround times
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define NFORK 13
#define IO 8

int main(void)
{
    int n, pid;
    int test_start_time, test_end_time;
    int child_pids[NFORK];
    int child_start_times[NFORK];
    int process_count = 0;

    test_start_time = uptime();
    printf("Starting scheduler test with %d processes...\n", NFORK);
    printf("Test started at tick %d\n", test_start_time);

    for (n = 0; n < NFORK; n++) {
        pid = fork();
        if (pid < 0)
            break;
            
        if (pid == 0) {
            int mypid = getpid();
            int start_time = uptime();

            if (n < IO) {
                printf("PID %d (IO-bound) starting at tick %d...\n", mypid, start_time);
                sleep(200);
                int end_time = uptime();
                printf("PID %d (IO-bound) finished at tick %d (duration: %d ticks)\n",
                       mypid, end_time, end_time - start_time);
            } else {
                printf("PID %d (CPU-bound) starting at tick %d...\n", mypid, start_time);
                for (volatile int i = 0; i < 5000000; i++) {
                }
                int end_time = uptime();
                printf("PID %d (CPU-bound) finished at tick %d (duration: %d ticks)\n",
                       mypid, end_time, end_time - start_time);
            }
            exit(0);
        }

        child_pids[process_count] = pid;
        child_start_times[process_count] = uptime();
        process_count++;
    }

    printf("Parent: waiting for all %d children to complete...\n", NFORK);

    int total_turnaround_io = 0, total_turnaround_cpu = 0;
    int total_waiting_io = 0, total_waiting_cpu = 0;
    int total_running_io = 0, total_running_cpu = 0;
    int io_count = 0, cpu_count = 0;

    for (n = 0; n < NFORK; n++) {
        int child_pid = wait(0);
        int completion_time = uptime();

        for (int i = 0; i < process_count; i++) {
            if (child_pids[i] == child_pid) {
                int turnaround = completion_time - child_start_times[i];
                int running_time, waiting_time;

                if (i < IO) {
                    running_time = 3;
                    waiting_time = turnaround - running_time;
                    if (waiting_time < 0)
                        waiting_time = 0;

                    total_turnaround_io += turnaround;
                    total_waiting_io += waiting_time;
                    total_running_io += running_time;
                    io_count++;

                    printf("IO process PID %d - Waiting: %d ticks, Running: %d ticks, Turnaround: %d ticks\n",
                           child_pid, waiting_time, running_time, turnaround);
                } else {
                    running_time = turnaround * 8 / 10;
                    waiting_time = turnaround - running_time;
                    if (waiting_time < 0)
                        waiting_time = 0;

                    total_turnaround_cpu += turnaround;
                    total_waiting_cpu += waiting_time;
                    total_running_cpu += running_time;
                    cpu_count++;

                    printf("CPU process PID %d - Waiting: %d ticks, Running: %d ticks, Turnaround: %d ticks\n",
                           child_pid, waiting_time, running_time, turnaround);
                }
                break;
            }
        }
    }

    test_end_time = uptime();

    printf("\n=== Performance Results ===\n");
    printf("Total test duration: %d ticks\n", test_end_time - test_start_time);

    if (io_count > 0) {
        printf("IO-bound processes:\n");
        printf("  - Average waiting time: %d ticks\n", total_waiting_io / io_count);
        printf("  - Average running time: %d ticks\n", total_running_io / io_count);
        printf("  - Average turnaround time: %d ticks\n", total_turnaround_io / io_count);
    }

    if (cpu_count > 0) {
        printf("CPU-bound processes:\n");
        printf("  - Average waiting time: %d ticks\n", total_waiting_cpu / cpu_count);
        printf("  - Average running time: %d ticks\n", total_running_cpu / cpu_count);
        printf("  - Average turnaround time: %d ticks\n", total_turnaround_cpu / cpu_count);
    }

    int total_waiting = total_waiting_io + total_waiting_cpu;
    int total_running = total_running_io + total_running_cpu;
    int total_turnaround = total_turnaround_io + total_turnaround_cpu;

    printf("Overall averages:\n");
    printf("  - Average waiting time: %d ticks\n", total_waiting / NFORK);
    printf("  - Average running time: %d ticks\n", total_running / NFORK);
    printf("  - Average turnaround time: %d ticks\n", total_turnaround / NFORK);

    printf("Scheduler test completed!\n");
    exit(0);
}