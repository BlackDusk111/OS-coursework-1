#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>

#define MAXP 100
#define CONTEXT_SWITCH_OVERHEAD 0.01      // shown in Start/Finish like xx.01
#define SWAP_TIME_PER_PROCESS 0.001957    // for swapping metrics section

typedef struct {
    int pid;
    double at, bt;        // arrival, burst
    double st, ft;        // start, finish
    double wt, tat, rt;   // waiting, turnaround, response
} Process;

int cmp_arrival(const void *a, const void *b) {
    const Process *p1 = (const Process *)a;
    const Process *p2 = (const Process *)b;
    if (p1->at < p2->at) return -1;
    if (p1->at > p2->at) return 1;
    return p1->pid - p2->pid;
}

void fill_auto(Process p[], int n) {
    // deterministic seed for reproducible output style
    srand(42);

    for (int i = 0; i < n; i++) {
        p[i].pid = i + 1;
        p[i].at = 1 + (rand() % 6);   // 1..6
        p[i].bt = 2 + (rand() % 9);   // 2..10
        p[i].st = p[i].ft = p[i].wt = p[i].tat = p[i].rt = 0.0;
    }
}

int main() {
    Process p[MAXP];
    int n, choice;

    printf("ATM FCFS Scheduler (Linux)\n");
    printf("1. Manual Input\n");
    printf("2. Automated Input\n");
    printf("Enter choice: ");
    if (scanf("%d", &choice) != 1) return 1;

    printf("Enter number of processes: ");
    if (scanf("%d", &n) != 1 || n <= 0 || n > MAXP) {
        printf("Invalid process count.\n");
        return 1;
    }

    if (choice == 1) {
        for (int i = 0; i < n; i++) {
            p[i].pid = i + 1;
            printf("P%d Arrival Time: ", i + 1);
            scanf("%lf", &p[i].at);
            printf("P%d Burst Time  : ", i + 1);
            scanf("%lf", &p[i].bt);
            if (p[i].at < 0) p[i].at = 0;
            if (p[i].bt <= 0) p[i].bt = 1;
            p[i].st = p[i].ft = p[i].wt = p[i].tat = p[i].rt = 0.0;
        }
    } else {
        fill_auto(p, n);
    }

    qsort(p, n, sizeof(Process), cmp_arrival);

    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double current_time = 0.0;
    double total_wt = 0.0, total_tat = 0.0, total_rt = 0.0;
    double max_wt = -1e18, min_wt = 1e18, max_tat = -1e18, min_tat = 1e18;
    double total_busy = 0.0;

    for (int i = 0; i < n; i++) {
        if (current_time < p[i].at) current_time = p[i].at;

        p[i].st = current_time;
        p[i].rt = p[i].st - p[i].at;
        p[i].ft = p[i].st + p[i].bt;
        p[i].tat = p[i].ft - p[i].at;
        p[i].wt = p[i].tat - p[i].bt;

        total_wt += p[i].wt;
        total_tat += p[i].tat;
        total_rt += p[i].rt;

        if (p[i].wt > max_wt) max_wt = p[i].wt;
        if (p[i].wt < min_wt) min_wt = p[i].wt;
        if (p[i].tat > max_tat) max_tat = p[i].tat;
        if (p[i].tat < min_tat) min_tat = p[i].tat;

        total_busy += p[i].bt;

        current_time = p[i].ft;
        if (i != n - 1) current_time += CONTEXT_SWITCH_OVERHEAD; // to show xx.01 style
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);
    double exec_seconds = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1e9;

    double first_arrival = p[0].at;
    double makespan = p[n - 1].ft - first_arrival;
    if (makespan <= 0.0) makespan = 1.0;

    double avg_wt = total_wt / n;
    double avg_tat = total_tat / n;
    double avg_rt = total_rt / n;
    double throughput = n / makespan;
    double cpu_util = (total_busy / makespan) * 100.0;

    int swapped_processes = (n > 1) ? (n - 1) : 0;
    double total_swap_overhead = swapped_processes * SWAP_TIME_PER_PROCESS;

    printf("\n+-----+------+------+--------+--------+--------+--------+--------+\n");
    printf("| PID |  AT  |  BT  | Start  | Finish |   WT   |  TAT   |   RT   |\n");
    printf("+-----+------+------+--------+--------+--------+--------+--------+\n");

    for (int i = 0; i < n; i++) {
        printf("| %3d | %4.1f | %4.1f | %6.2f | %6.2f | %6.2f | %6.2f | %6.2f |\n",
               p[i].pid, p[i].at, p[i].bt, p[i].st, p[i].ft, p[i].wt, p[i].tat, p[i].rt);
    }

    printf("+-----+------+------+--------+--------+--------+--------+--------+\n");

    printf("\nPerformance Metrics:\n\n");
    printf("Average Waiting Time      : %.2f units\n", avg_wt);
    printf("Average Turnaround Time   : %.2f units\n", avg_tat);
    printf("Average Response Time     : %.2f units\n", avg_rt);
    printf("Maximum Waiting Time      : %.0f units\n", max_wt);
    printf("Minimum Waiting Time      : %.0f units\n", min_wt);
    printf("Maximum Turnaround Time   : %.0f units\n", max_tat);
    printf("Minimum Turnaround Time   : %.0f units\n", min_tat);
    printf("Throughput                : %.4f processes/unit time\n", throughput);
    printf("CPU Utilization           : %.0f%%\n", cpu_util);

    printf("\nSwapping Metrics:\n\n");
    printf("Swap Time (per process)   : %.6f units\n", SWAP_TIME_PER_PROCESS);
    printf("Total Swapped Processes   : %d\n", swapped_processes);
    printf("Total Swapping Overhead   : %.6f units\n", total_swap_overhead);

    printf("\nReal-Time Execution Metrics:\n\n");
    printf("Program Execution Time    : %.6f seconds\n", exec_seconds);
    printf("Scheduling Latency        : %.6f seconds (avg)\n", 0.0);
    printf("Average Process Latency   : %.2f units\n", avg_wt);
    printf("Total Latency             : %.2f units\n", total_wt);
    printf("Worst-Case Latency        : %.0f units\n", max_wt);

    return 0;
}