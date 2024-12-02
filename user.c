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

    srand(getpid()); // Seed random number generator with PID for randomness
    int pid = getpid();
    int runtime = 0;

    printf("USER PID: %d started\n", pid);

    while (runtime < 5000) { // Simulated runtime for the user process
        runtime += rand() % 100 + 1; // Increment runtime randomly

        // Generate a random page number to request
        int page_number = rand() % PAGE_COUNT;

        // Randomly decide whether to read or write
        int is_write = rand() % 2;
        Message msg;
        msg.mtype = 1; // Messages to OSS are of type 1
        msg.pid = pid;
        msg.page_number = page_number;
        msg.action = is_write ? REQUEST + 1 : REQUEST; // 1 = Read, 2 = Write

        // Send the request message to OSS
        msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);
        printf("USER PID: %d %s page %d\n", pid, is_write ? "writing to" : "reading from", page_number);

        // Simulate a random delay before the next request
        struct timespec ts = {0, (rand() % 100 + 1) * 1000000}; // Random delay 1-100 ms
        nanosleep(&ts, NULL);

        // Randomly decide whether to terminate (10% chance)
        if (rand() % 100 < 10) {
            msg.action = TERMINATE;
            msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);
            printf("USER PID: %d terminating\n", pid);
            break;
        }
    }

    printf("USER PID: %d finished execution\n", pid);
    return 0;
}
