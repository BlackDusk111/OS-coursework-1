#include <windows.h>
#include <stdio.h>

#define INITIAL_BALANCE 1000
#define WITHDRAW_AMOUNT 100
#define THREADS 4

typedef enum {
    USE_MUTEX = 1,
    USE_BINARY_SEM = 2
} SyncMode;

volatile LONG balance;
volatile LONG success_count;
volatile LONG denied_count;
volatile LONG start_flag = 0;
SyncMode mode;

HANDLE bal_mutex;
HANDLE bal_sem; // binary semaphore (init=1, max=1)

typedef struct {
    int id;
} ThreadArg;

void lock_cs() {
    if (mode == USE_MUTEX) WaitForSingleObject(bal_mutex, INFINITE);
    else WaitForSingleObject(bal_sem, INFINITE);
}

void unlock_cs() {
    if (mode == USE_MUTEX) ReleaseMutex(bal_mutex);
    else ReleaseSemaphore(bal_sem, 1, NULL);
}

DWORD WINAPI withdraw_thread(LPVOID param) {
    ThreadArg* t = (ThreadArg*)param;

    while (start_flag == 0) SwitchToThread(); // start together

    lock_cs();

    // ---- CRITICAL SECTION (protected) ----
    LONG before = balance;
    Sleep(3); // simulate processing delay
    if (before >= WITHDRAW_AMOUNT) {
        LONG after = before - WITHDRAW_AMOUNT;
        Sleep(3);
        balance = after;
        success_count++;
        printf("Thread %d: Withdrawal SUCCESS | %ld -> %ld\n", t->id, before, after);
    } else {
        denied_count++;
        printf("Thread %d: Withdrawal DENIED  | Balance=%ld\n", t->id, before);
    }
    // --------------------------------------

    unlock_cs();
    return 0;
}

void run_case(SyncMode selected_mode) {
    HANDLE th[THREADS];
    ThreadArg args[THREADS];

    balance = INITIAL_BALANCE;
    success_count = 0;
    denied_count = 0;
    start_flag = 0;
    mode = selected_mode;

    if (mode == USE_MUTEX) bal_mutex = CreateMutex(NULL, FALSE, NULL);
    else bal_sem = CreateSemaphore(NULL, 1, 1, NULL); // binary semaphore

    printf("\n=== %s SOLUTION ===\n", mode == USE_MUTEX ? "MUTEX" : "BINARY SEMAPHORE");

    for (int i = 0; i < THREADS; i++) {
        args[i].id = i + 1;
        th[i] = CreateThread(NULL, 0, withdraw_thread, &args[i], 0, NULL);
    }

    Sleep(10);
    InterlockedExchange(&start_flag, 1);

    WaitForMultipleObjects(THREADS, th, TRUE, INFINITE);

    int expected_final = INITIAL_BALANCE - (THREADS * WITHDRAW_AMOUNT);

    printf("Final Balance           : NPR %ld\n", balance);
    printf("Expected Final Balance  : NPR %d\n", expected_final);
    printf("Successful Withdrawals  : %ld\n", success_count);
    printf("Denied Withdrawals      : %ld\n", denied_count);
    printf("Critical Section Status : %s\n",
           (balance == expected_final) ? "PROTECTED ✅" : "VIOLATED ❌");

    for (int i = 0; i < THREADS; i++) CloseHandle(th[i]);
    if (mode == USE_MUTEX) CloseHandle(bal_mutex);
    else CloseHandle(bal_sem);
}

int main() {
    printf("ATM Monitoring and Alert System - Process Synchronization SOLUTION (Windows)\n");
    printf("Scenario: 4 threads withdraw NPR 100 from NPR 1000\n");

    run_case(USE_MUTEX);
    run_case(USE_BINARY_SEM);

    return 0;
}