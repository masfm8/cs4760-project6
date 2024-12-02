#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#define PAGE_COUNT 32   // Pages per process
#define REQUEST 1       // Action type for read/write requests
#define TERMINATE 3     // Action type for termination

typedef struct {
    long mtype;
    int pid;
    int page_number;
    int action; // 1 = Read, 2 = Write, 3 = Terminate
} Message;

int main() {
    int msgid;
    key_t key = ftok("oss.c", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);

    srand(getpid()); // Seed random number generator with PID for uniqueness
    int pid = getpid();
    int runtime = 0;

    printf("USER PID: %d started\n", pid);

    while (runtime < 5000) { // User runs for a fixed amount of time
        runtime += rand() % 100 + 1; // Simulate elapsed runtime

        // Generate a random page number to access
        int page_number = rand() % PAGE_COUNT;

        // Randomly decide whether to read or write
        int is_write = rand() % 2;
        Message msg;
        msg.mtype = 1; // Messages to OSS are of type 1
        msg.pid = pid;
        msg.page_number = page_number;
        msg.action = is_write ? REQUEST + 1 : REQUEST; // 1 = Read, 2 = Write

        // Send the memory request to OSS
        msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);
        printf("USER PID: %d %s page %d\n", pid, is_write ? "writing to" : "reading from", page_number);

        // Simulate a random delay before the next request
        struct timespec ts = {0, (rand() % 100 + 1) * 1000000}; // Random sleep between 1-100 ms
        nanosleep(&ts, NULL);

        // Randomly decide whether to terminate
        if (rand() % 100 < 10) { // 10% chance to terminate
            msg.action = TERMINATE;
            msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);
            printf("USER PID: %d terminating\n", pid);
            break;
        }
    }

    return 0;
}
