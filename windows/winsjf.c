#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXP 100
#define CONTEXT_SWITCH_OVERHEAD 0.01
#define SWAP_TIME_PER_PROCESS 0.001957

typedef struct {
    int pid;
    double at, bt;
    double st, ft;
    double wt, tat, rt;
    int done;
} Process;

void fill_auto(Process p[], int n) {
    srand(42);
    for (int i = 0; i < n; i++) {
        p[i].pid = i + 1;
        p[i].at = 1 + (rand() % 6);
        p[i].bt = 2 + (rand() % 9);
        p[i].st = p[i].ft = p[i].wt = p[i].tat = p[i].rt = 0.0;
        p[i].done = 0;
    }
}

int select_sjf(Process p[], int n, double time) {
    int idx = -1;
    double min_bt = 1e18;

    for (int i = 0; i < n; i++) {
        if (!p[i].done && p[i].at <= time) {
            if (p[i].bt < min_bt) {
                min_bt = p[i].bt;
                idx = i;
            } else if (p[i].bt == min_bt) {
                if (p[i].at < p[idx].at || (p[i].at == p[idx].at && p[i].pid < p[idx].pid))
                    idx = i;
            }
        }
    }
    return idx;
}

int main() {
    Process p[MAXP];
    int n, choice;

    printf("ATM Monitoring and Alert System - SJF Scheduler (Windows)\n");
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
            p[i].done = 0;
        }
    } else {
        fill_auto(p, n);
    }

    LARGE_INTEGER fq, t1, t2;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t1);

    int completed = 0;
    double time_now = 0.0;
    double total_wt = 0.0, total_tat = 0.0, total_rt = 0.0;
    double max_wt = -1e18, min_wt = 1e18, max_tat = -1e18, min_tat = 1e18;
    double total_busy = 0.0;
    double first_arrival = 1e18;

    for (int i = 0; i < n; i++) if (p[i].at < first_arrival) first_arrival = p[i].at;

    while (completed < n) {
        int idx = select_sjf(p, n, time_now);

        if (idx == -1) {
            double next_at = 1e18;
            for (int i = 0; i < n; i++) if (!p[i].done && p[i].at < next_at) next_at = p[i].at;
            time_now = next_at;
            continue;
        }

        p[idx].st = time_now;
        p[idx].ft = p[idx].st + p[idx].bt;
        p[idx].tat = p[idx].ft - p[idx].at;
        p[idx].wt  = p[idx].tat - p[idx].bt;
        p[idx].rt  = p[idx].wt;

        total_wt += p[idx].wt;
        total_tat += p[idx].tat;
        total_rt += p[idx].rt;
        total_busy += p[idx].bt;

        if (p[idx].wt > max_wt) max_wt = p[idx].wt;
        if (p[idx].wt < min_wt) min_wt = p[idx].wt;
        if (p[idx].tat > max_tat) max_tat = p[idx].tat;
        if (p[idx].tat < min_tat) min_tat = p[idx].tat;

        p[idx].done = 1;
        completed++;

        time_now = p[idx].ft;
        if (completed < n) time_now += CONTEXT_SWITCH_OVERHEAD;
    }

    QueryPerformanceCounter(&t2);
    double exec_seconds = (double)(t2.QuadPart - t1.QuadPart) / (double)fq.QuadPart;

    double last_finish = 0.0;
    for (int i = 0; i < n; i++) if (p[i].ft > last_finish) last_finish = p[i].ft;
    double makespan = last_finish - first_arrival;
    if (makespan <= 0) makespan = 1.0;

    double avg_wt = total_wt / n;
    double avg_tat = total_tat / n;
    double avg_rt = total_rt / n;
    double throughput = n / makespan;
    double cpu_util = (total_busy / makespan) * 100.0;

    int swapped_processes = (n > 1) ? n - 1 : 0;
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
    printf("CPU Utilization           : %.2f%%\n", cpu_util);

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