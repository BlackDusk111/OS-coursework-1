#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SHM_NAME   L"Local\\ATM_TRANSACTION_SHARED_MEMORY_V2"
#define MUTEX_NAME L"Local\\ATM_TRANSACTION_MUTEX_V2"
#define MAX_TX     10

typedef struct {
    int id;
    int amount;      // NPR
    int fraud_flag;  // 0=normal, 1=suspicious
} TxRecord;

typedef struct {
    int write_count;
    int total_monitored;
    int done;
    TxRecord tx[MAX_TX];
} SharedBlock;

static void die(const char *msg) {
    fprintf(stderr, "%s (GetLastError=%lu)\n", msg, GetLastError());
    ExitProcess(1);
}

static HANDLE create_or_open_mapping() {
    HANDLE hMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
        (DWORD)sizeof(SharedBlock), SHM_NAME
    );
    if (!hMap) die("CreateFileMapping");
    return hMap;
}

static HANDLE create_or_open_mutex() {
    HANDLE hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    if (!hMutex) die("CreateMutex");
    return hMutex;
}

static void run_writer() {
    HANDLE hMap = create_or_open_mapping();
    SharedBlock *blk = (SharedBlock *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock));
    if (!blk) die("MapViewOfFile(writer)");

    HANDLE hMutex = create_or_open_mutex();

    WaitForSingleObject(hMutex, INFINITE);
    memset(blk, 0, sizeof(SharedBlock));   // reset for fresh run
    ReleaseMutex(hMutex);

    printf("ATM_TRANSACTION_PROCESS (Windows)\n");
    printf("Writing transactions to shared memory...\n\n");

    srand((unsigned)time(NULL));

    for (int i = 0; i < MAX_TX; i++) {
        TxRecord t;
        t.id = i + 1;
        t.amount = 1000 + rand() % 4000;
        t.fraud_flag = (t.amount > 3500) ? 1 : 0;

        WaitForSingleObject(hMutex, INFINITE);
        blk->tx[blk->write_count] = t;
        blk->write_count++;
        ReleaseMutex(hMutex);

        printf("[Writer] TX %2d | Amount: NPR %4d | FraudFlag: %d\n",
               t.id, t.amount, t.fraud_flag);

        Sleep(50);
    }

    WaitForSingleObject(hMutex, INFINITE);
    blk->done = 1;
    int total = blk->write_count;
    ReleaseMutex(hMutex);

    printf("\n--- IPC REPORT (WRITER) ---\n");
    printf("Total Submissions: %d\n", total);
    printf("Status           : COMPLETE\n");

    UnmapViewOfFile(blk);
    CloseHandle(hMap);
    CloseHandle(hMutex);
}

static void run_monitor() {
    HANDLE hMap = create_or_open_mapping();
    SharedBlock *blk = (SharedBlock *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock));
    if (!blk) die("MapViewOfFile(monitor)");

    HANDLE hMutex = create_or_open_mutex();

    printf("ATM_MONITOR_DAEMON (Windows)\n");
    printf("Reading transactions from shared memory...\n\n");

    int next = 0;
    int alerts = 0;

    while (1) {
        int processed = 0, wc = 0, done = 0;
        TxRecord t = {0};

        WaitForSingleObject(hMutex, INFINITE);
        wc = blk->write_count;
        done = blk->done;

        if (next < wc) {
            t = blk->tx[next];
            next++;
            blk->total_monitored++;
            processed = 1;
        }
        ReleaseMutex(hMutex);

        if (processed) {
            printf("[Monitor] TX %2d | Amount: NPR %4d | FraudFlag: %d\n",
                   t.id, t.amount, t.fraud_flag);
            if (t.fraud_flag) {
                alerts++;
                printf("          -> ALERT: Suspicious transaction detected\n");
            }
        } else if (done && next >= wc) {
            break;
        } else {
            Sleep(30);
        }
    }

    WaitForSingleObject(hMutex, INFINITE);
    int total_sub = blk->write_count;
    int total_mon = blk->total_monitored;
    ReleaseMutex(hMutex);

    double integrity = (total_sub > 0)
        ? (100.0 * (double)total_mon / (double)total_sub)
        : 100.0;

    printf("\n--- IPC REPORT (MONITOR) ---\n");
    printf("Total Submissions : %d\n", total_sub);
    printf("Total Monitored   : %d\n", total_mon);
    printf("Alerts Raised     : %d\n", alerts);
    printf("Integrity Rate    : %.2f%%\n", integrity);
    printf("Sync Mechanism    : File Mapping + Named Mutex\n");

    UnmapViewOfFile(blk);
    CloseHandle(hMap);
    CloseHandle(hMutex);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage:\n");
        printf("  %s writer\n", argv[0]);
        printf("  %s monitor\n", argv[0]);
        return 1;
    }

    if (_stricmp(argv[1], "writer") == 0) run_writer();
    else if (_stricmp(argv[1], "monitor") == 0) run_monitor();
    else {
        printf("Invalid mode. Use writer | monitor\n");
        return 1;
    }

    return 0;
}