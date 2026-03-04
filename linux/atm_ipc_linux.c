#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#define SHM_NAME "/atm_transaction_shared_memory"
#define SEM_NAME "/atm_transaction_semaphore"
#define MAX_TX   10

typedef struct {
    int id;
    int amount;      // NPR
    int fraud_flag;  // 0 = normal, 1 = suspicious
} TxRecord;

typedef struct {
    int write_count;
    int total_monitored;
    int done;
    TxRecord tx[MAX_TX];
} SharedBlock;

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void run_writer() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) die("shm_open");

    if (ftruncate(fd, sizeof(SharedBlock)) < 0) die("ftruncate");

    SharedBlock *blk = mmap(NULL, sizeof(SharedBlock),
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (blk == MAP_FAILED) die("mmap");

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) die("sem_open");

    sem_wait(sem);
    memset(blk, 0, sizeof(SharedBlock));
    sem_post(sem);

    printf("ATM_TRANSACTION_PROCESS (Linux)\n");
    printf("Writing transactions to shared memory...\n\n");

    srand((unsigned)time(NULL));

    for (int i = 0; i < MAX_TX; i++) {
        TxRecord t;
        t.id = i + 1;
        t.amount = 1000 + rand() % 4000;
        t.fraud_flag = (t.amount > 3500) ? 1 : 0;

        sem_wait(sem);
        blk->tx[blk->write_count] = t;
        blk->write_count++;
        sem_post(sem);

        printf("[Writer] TX %2d | Amount: NPR %4d | FraudFlag: %d\n",
               t.id, t.amount, t.fraud_flag);

        usleep(50000);
    }

    sem_wait(sem);
    blk->done = 1;
    int total = blk->write_count;
    sem_post(sem);

    printf("\n--- IPC REPORT (WRITER) ---\n");
    printf("Total Submissions: %d\n", total);
    printf("Status           : COMPLETE\n");

    munmap(blk, sizeof(SharedBlock));
    close(fd);
    sem_close(sem);
}

static void run_monitor() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) die("shm_open");

    if (ftruncate(fd, sizeof(SharedBlock)) < 0) die("ftruncate");

    SharedBlock *blk = mmap(NULL, sizeof(SharedBlock),
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (blk == MAP_FAILED) die("mmap");

    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) die("sem_open");

    printf("ATM_MONITOR_DAEMON (Linux)\n");
    printf("Reading transactions from shared memory...\n\n");

    int next = 0;
    int alerts = 0;

    while (1) {
        int processed = 0, wc = 0, done = 0;
        TxRecord t = {0};

        sem_wait(sem);
        wc = blk->write_count;
        done = blk->done;

        if (next < wc) {
            t = blk->tx[next];
            next++;
            blk->total_monitored++;
            processed = 1;
        }
        sem_post(sem);

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
            usleep(30000);
        }
    }

    sem_wait(sem);
    int total_sub = blk->write_count;
    int total_mon = blk->total_monitored;
    sem_post(sem);

    double integrity = (total_sub > 0)
        ? (100.0 * (double)total_mon / (double)total_sub)
        : 100.0;

    printf("\n--- IPC REPORT (MONITOR) ---\n");
    printf("Total Submissions : %d\n", total_sub);
    printf("Total Monitored   : %d\n", total_mon);
    printf("Alerts Raised     : %d\n", alerts);
    printf("Integrity Rate    : %.2f%%\n", integrity);
    printf("Sync Mechanism    : POSIX Shared Memory + POSIX Semaphore\n");

    munmap(blk, sizeof(SharedBlock));
    close(fd);
    sem_close(sem);
}

static void run_cleanup() {
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
    printf("IPC resources cleaned.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage:\n");
        printf("  %s writer\n", argv[0]);
        printf("  %s monitor\n", argv[0]);
        printf("  %s cleanup\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "writer") == 0) run_writer();
    else if (strcmp(argv[1], "monitor") == 0) run_monitor();
    else if (strcmp(argv[1], "cleanup") == 0) run_cleanup();
    else {
        printf("Invalid mode. Use writer | monitor | cleanup\n");
        return 1;
    }

    return 0;
}