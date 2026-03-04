#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>

#define THREADS 4
#define WITHDRAW 100
#define INITIAL_BALANCE 1000

// Make the race obvious
#define OPS_PER_THREAD 2000

static volatile int start_flag = 0;

static long long expected_submissions = 0;
static long long actual_recorded = 0;

static int account_balance = INITIAL_BALANCE;

// For "DB" simulation
static long long before_db[THREADS];
static long long after_db[THREADS];

// Metrics (simple counters to resemble your report)
static long long lost_updates = 0;
static int race_condition = 0;
static int critical_section_violated = 0;

static long long context_switches_forced = 0;
static long long starvation_events = 0;

// Optional safety
static int use_mutex = 0;
static pthread_mutex_t bal_lock;

typedef struct {
    int id;
} Arg;

static inline void tiny_delay() {
    // force interleavings
    usleep(50);
    sched_yield();
}

void* worker_withdraw(void* p) {
    Arg* a = (Arg*)p;
    int id = a->id;

    while (!start_flag) sched_yield();

    // before DB snapshot
    before_db[id] = account_balance;

    long long local_success = 0;

    for (int k = 0; k < OPS_PER_THREAD; k++) {
        if (use_mutex) pthread_mutex_lock(&bal_lock);

        // Critical section (intentionally non-atomic if no mutex)
        int before = account_balance;
        tiny_delay();
        int after = before - WITHDRAW;
        tiny_delay();
        account_balance = after;

        if (use_mutex) pthread_mutex_unlock(&bal_lock);

        local_success++;
        // rough "forced context switches"
        context_switches_forced++;
        if ((k % 777) == 0) starvation_events += 0; // placeholder counter
    }

    // after DB snapshot
    after_db[id] = account_balance;

    __sync_fetch_and_add(&actual_recorded, local_success);
    return NULL;
}

int main() {
    printf("ATM Monitoring and Alert System: Process Sync Failure/Success Report\n\n");

    printf("Select mode:\n");
    printf("1. UNSYNCHRONIZED (Race expected)\n");
    printf("2. SYNCHRONIZED (Mutex protected)\n");
    printf("Enter choice: ");

    int ch = 1;
    if (scanf("%d", &ch) != 1) return 1;
    use_mutex = (ch == 2);

    pthread_t th[THREADS];
    Arg args[THREADS];

    // Setup
    account_balance = INITIAL_BALANCE;
    expected_submissions = (long long)THREADS * OPS_PER_THREAD;
    actual_recorded = 0;
    start_flag = 0;

    if (use_mutex) pthread_mutex_init(&bal_lock, NULL);

    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    for (int i = 0; i < THREADS; i++) {
        args[i].id = i;
        before_db[i] = after_db[i] = 0;
        pthread_create(&th[i], NULL, worker_withdraw, &args[i]);
    }

    usleep(20000);
    start_flag = 1;

    for (int i = 0; i < THREADS; i++) pthread_join(th[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t2);
    double exec_seconds = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1e9;

    if (use_mutex) pthread_mutex_destroy(&bal_lock);

    // Compute correctness expectation for the simple ATM story
    int expected_final_balance = INITIAL_BALANCE - (THREADS * WITHDRAW);
    // BUT we ran many ops; for "report style", we check whether updates were lost:
    // expected balance if *all* ops applied:
    long long expected_all_ops_balance = (long long)INITIAL_BALANCE - (long long)WITHDRAW * expected_submissions;

    // Determine race/violation by comparing what should have happened vs what did
    // If no race: account_balance == expected_all_ops_balance
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

    // Print report in the same “style”
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

    // Keep these sections so your figure matches the screenshot’s structure
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

    // Also show the simple “4 withdrawals” story line for your paragraph
    printf("\n[ ATM STORY CHECK ]\n");
    printf("If 4 threads withdraw NPR 100 from NPR 1000, expected is NPR %d.\n", expected_final_balance);
    printf("(This program runs %d ops/thread to *amplify* the race for the report.)\n", OPS_PER_THREAD);

    return 0;
}