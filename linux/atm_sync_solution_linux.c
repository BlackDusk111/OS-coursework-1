#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sched.h>

#define INITIAL_BALANCE 1000
#define WITHDRAW_AMOUNT 100
#define THREADS 4

typedef enum {
    USE_MUTEX = 1,
    USE_BINARY_SEM = 2
} SyncMode;

int balance;
int success_count;
int denied_count;
volatile int start_flag = 0;
SyncMode mode;

pthread_mutex_t bal_mutex;
sem_t bal_sem; // binary semaphore (init = 1)

typedef struct {
    int id;
} ThreadArg;

void lock_cs() {
    if (mode == USE_MUTEX) pthread_mutex_lock(&bal_mutex);
    else sem_wait(&bal_sem);
}

void unlock_cs() {
    if (mode == USE_MUTEX) pthread_mutex_unlock(&bal_mutex);
    else sem_post(&bal_sem);
}

void* withdraw_thread(void* arg) {
    ThreadArg* t = (ThreadArg*)arg;

    while (!start_flag) sched_yield(); // start together

    lock_cs();

    // ---- CRITICAL SECTION (protected) ----
    int before = balance;
    usleep(3000); // simulate processing delay
    if (before >= WITHDRAW_AMOUNT) {
        int after = before - WITHDRAW_AMOUNT;
        usleep(3000);
        balance = after;
        success_count++;
        printf("Thread %d: Withdrawal SUCCESS | %d -> %d\n", t->id, before, after);
    } else {
        denied_count++;
        printf("Thread %d: Withdrawal DENIED  | Balance=%d\n", t->id, before);
    }
    // --------------------------------------

    unlock_cs();
    return NULL;
}

void run_case(SyncMode selected_mode) {
    pthread_t th[THREADS];
    ThreadArg args[THREADS];

    balance = INITIAL_BALANCE;
    success_count = 0;
    denied_count = 0;
    start_flag = 0;
    mode = selected_mode;

    if (mode == USE_MUTEX) pthread_mutex_init(&bal_mutex, NULL);
    else sem_init(&bal_sem, 0, 1); // binary semaphore

    printf("\n=== %s SOLUTION ===\n", mode == USE_MUTEX ? "MUTEX" : "BINARY SEMAPHORE");

    for (int i = 0; i < THREADS; i++) {
        args[i].id = i + 1;
        pthread_create(&th[i], NULL, withdraw_thread, &args[i]);
    }

    usleep(10000);
    start_flag = 1;

    for (int i = 0; i < THREADS; i++) pthread_join(th[i], NULL);

    int expected_final = INITIAL_BALANCE - (THREADS * WITHDRAW_AMOUNT);

    printf("Final Balance           : NPR %d\n", balance);
    printf("Expected Final Balance  : NPR %d\n", expected_final);
    printf("Successful Withdrawals  : %d\n", success_count);
    printf("Denied Withdrawals      : %d\n", denied_count);
    printf("Critical Section Status : %s\n",
           (balance == expected_final) ? "PROTECTED ✅" : "VIOLATED ❌");

    if (mode == USE_MUTEX) pthread_mutex_destroy(&bal_mutex);
    else sem_destroy(&bal_sem);
}

int main() {
    printf("ATM Monitoring and Alert System - Process Synchronization SOLUTION (Linux)\n");
    printf("Scenario: 4 threads withdraw NPR 100 from NPR 1000\n");

    run_case(USE_MUTEX);
    run_case(USE_BINARY_SEM);

    return 0;
}