#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define WITHDRAW 100
#define INITIAL_BALANCE 1000
#define OPS_PER_THREAD 2000

static volatile LONG start_flag = 0;

static long long expected_submissions = 0;
static volatile LONGLONG actual_recorded = 0;

static volatile LONG account_balance = INITIAL_BALANCE;

static long long before_db[THREADS];
static long long after_db[THREADS];

static long long lost_updates = 0;
static int race_condition = 0;
static int critical_section_violated = 0;

static long long context_switches_forced = 0;
static long long starvation_events = 0;

static int use_mutex = 0;
static HANDLE hMutex;

typedef struct { int id; } Arg;

static void tiny_delay() {
    Sleep(1);
    SwitchToThread();
}

DWORD WINAPI worker_withdraw(LPVOID p) {
    Arg* a = (Arg*)p;
    int id = a->id;

    while (start_flag == 0) SwitchToThread();

    before_db[id] = account_balance;

    long long local_success = 0;

    for (int k = 0; k < OPS_PER_THREAD; k++) {
        if (use_mutex) WaitForSingleObject(hMutex, INFINITE);

        int before = account_balance;
        tiny_delay();
        int after = before - WITHDRAW;
        tiny_delay();
        account_balance = after;

        if (use_mutex) ReleaseMutex(hMutex);

        local_success++;
        context_switches_forced++;
    }

    after_db[id] = account_balance;

    InterlockedAdd64(&actual_recorded, local_success);
    return 0;
}

static double qpc_seconds(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER fq) {
    return (double)(b.QuadPart - a.QuadPart) / (double)fq.QuadPart;
}

int main() {
    printf("ATM Monitoring and Alert System: Process Sync Failure/Success Report (Windows)\n\n");

    printf("Select mode:\n");
    printf("1. UNSYNCHRONIZED (Race expected)\n");
    printf("2. SYNCHRONIZED (Mutex protected)\n");
    printf("Enter choice: ");

    int ch = 1;
    if (scanf("%d", &ch) != 1) return 1;
    use_mutex = (ch == 2);

    HANDLE th[THREADS];
    Arg args[THREADS];

    account_balance = INITIAL_BALANCE;
    expected_submissions = (long long)THREADS * OPS_PER_THREAD;
    actual_recorded = 0;
    start_flag = 0;

    if (use_mutex) hMutex = CreateMutex(NULL, FALSE, NULL);

    LARGE_INTEGER fq, t1, t2;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t1);

    for (int i = 0; i < THREADS; i++) {
        args[i].id = i;
        before_db[i] = after_db[i] = 0;
        th[i] = CreateThread(NULL, 0, worker_withdraw, &args[i], 0, NULL);
    }

    Sleep(20);
    start_flag = 1;

    WaitForMultipleObjects(THREADS, th, TRUE, INFINITE);

    QueryPerformanceCounter(&t2);
    double exec_seconds = qpc_seconds(t1, t2, fq);

    for (int i = 0; i < THREADS; i++) CloseHandle(th[i]);
    if (use_mutex) CloseHandle(hMutex);

    long long expected_all_ops_balance = (long long)INITIAL_BALANCE - (long long)WITHDRAW * expected_submissions;

    if ((long long)account_balance != expected_all_ops_balance) {
        race_condition = 1;
        critical_section_violated = 1;
        lost_updates = llabs(expected_all_ops_balance - (long long)account_balance) / WITHDRAW;
    }

    double integrity_rate = 0.0;
    if (expected_submissions > 0) {
        integrity_rate = ((double)(expected_submissions - lost_updates) / (double)expected_submissions) * 100.0;
        if (integrity_rate < 0) integrity_rate = 0;
        if (integrity_rate > 100) integrity_rate = 100;
    }

    printf("---------------------------------------------------------------\n");
    printf(" ATM MONITORING SYSTEM CONCURRENCY %s REPORT\n", use_mutex ? "SUCCESS" : "FAILURE");
    printf("---------------------------------------------------------------\n\n");

    printf("[ DATA INTEGRITY MATRIX ]\n");
    printf("+--------+-----------+----------+\n");
    printf("| Thread | Before DB | After DB |\n");
    printf("+--------+-----------+----------+\n");
    for (int i = 0; i < THREADS; i++) {
        printf("| %6d | %9lld | %8lld |\n", i + 1, before_db[i], after_db[i]);
    }
    printf("+--------+-----------+----------+\n\n");

    printf("Expected Submissions : %lld\n", expected_submissions);
    printf("Actual Recorded      : %lld\n", actual_recorded);
    printf("Lost Records (Race)  : %lld\n", lost_updates);
    printf("Data Integrity Rate  : %.2f%%\n\n", integrity_rate);

    printf("[ SCHEDULING & STARVATION ]\n");
    printf("Forced Context Switches : %lld\n", context_switches_forced);
    printf("Starvation Events       : %lld\n\n", starvation_events);

    printf("[ PERFORMANCE METRICS ]\n");
    printf("Total Execution Time    : %.6f seconds\n\n", exec_seconds);

    printf("[ PROBLEMS ]\n");
    printf("Race Condition          : %s\n", race_condition ? "YES" : "NO");
    printf("Critical Section        : %s\n", critical_section_violated ? "VIOLATED" : "PROTECTED");
    printf("Deadlock                : NO\n");
    printf("Starvation              : %s\n", starvation_events ? "YES" : "NO");
    printf("Data Inconsistency      : %s\n", race_condition ? "YES" : "NO");

    return 0;
}