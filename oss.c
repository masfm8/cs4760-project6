#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define MAX_PROCESSES 18
#define FRAME_COUNT 256  // Total frames in the system
#define PAGE_COUNT 32    // Pages per process
#define TIME_INCREMENT 100000
#define DEADLOCK_CHECK_INTERVAL 1000000
#define SIMULATION_TIME_LIMIT 20000000 // 20 seconds

typedef struct {
    int occupied;          // Whether the frame is occupied
    int dirty;             // Whether the frame has been modified
    int process_id;        // Process using the frame
    int page_number;       // Page number in the process
    unsigned long last_used_time; // LRU timestamp
} FrameTableEntry;

typedef struct {
    long mtype;
    int pid;
    int page_number;
    int action; // 1 = Read, 2 = Write, 3 = Terminate
} Message;

FrameTableEntry frameTable[FRAME_COUNT];
int pageTable[MAX_PROCESSES][PAGE_COUNT];
static pid_t pids[MAX_PROCESSES];
static int clock_ns = 0;
static int activeProcesses = 0;

// Function Prototypes
void initializeFrameTable();
void initializePageTable();
void printMemoryState();
void handlePageRequest(int process_id, int page_number, int action);
void cleanup();

// Signal handler for cleanup on CTRL+C
void signalHandler(int sig) {
    cleanup();
    exit(0);
}

int main() {
    int msgid;
    key_t key = ftok("oss.c", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);

    signal(SIGINT, signalHandler);

    initializeFrameTable();
    initializePageTable();

    printf("OSS: Starting simulation\n");

    // Spawn child processes
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if ((pids[i] = fork()) == 0) {
            execl("./user", "./user", NULL);
            perror("Error launching user process");
            exit(1);
        }
        activeProcesses++;
    }

    while (clock_ns < SIMULATION_TIME_LIMIT && activeProcesses > 0) {
        clock_ns += TIME_INCREMENT;

        // Process messages from user processes
        Message msg;
        if (msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) != -1) {
            if (msg.action == 1 || msg.action == 2) { // Read or Write
                handlePageRequest(msg.pid % MAX_PROCESSES, msg.page_number, msg.action);
            } else if (msg.action == 3) { // Terminate
                printf("OSS: Process P%d terminating and releasing all frames\n", msg.pid % MAX_PROCESSES);
                for (int i = 0; i < FRAME_COUNT; i++) {
                    if (frameTable[i].occupied && frameTable[i].process_id == msg.pid % MAX_PROCESSES) {
                        frameTable[i].occupied = 0;
                        frameTable[i].dirty = 0;
                        frameTable[i].process_id = -1;
                        frameTable[i].page_number = -1;
                    }
                }
                activeProcesses--;
            }
        }

        printMemoryState();

        struct timespec ts = {0, 1000000}; // Sleep for 1 ms
        nanosleep(&ts, NULL);
    }

    cleanup();
    return 0;
}

void initializeFrameTable() {
    for (int i = 0; i < FRAME_COUNT; i++) {
        frameTable[i].occupied = 0;
        frameTable[i].dirty = 0;
        frameTable[i].process_id = -1;
        frameTable[i].page_number = -1;
        frameTable[i].last_used_time = 0;
    }
}

void initializePageTable() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        for (int j = 0; j < PAGE_COUNT; j++) {
            pageTable[i][j] = -1; // -1 indicates the page is not in any frame
        }
    }
}

void handlePageRequest(int process_id, int page_number, int action) {
    int frame = -1;

    // Check if the page is already in a frame
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frameTable[i].occupied && frameTable[i].process_id == process_id &&
            frameTable[i].page_number == page_number) {
            frame = i;
            frameTable[i].last_used_time = clock_ns; // Update LRU timestamp
            printf("OSS: Address %d already in frame %d, granting %s access to P%d\n",
                   page_number, i, action == 1 ? "read" : "write", process_id);
            if (action == 2) { // Write
                frameTable[i].dirty = 1;
            }
            return;
        }
    }

    // Page fault: Find an empty frame or replace LRU
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (!frameTable[i].occupied) {
            frame = i;
            break;
        }
    }

    if (frame == -1) {
        // No free frame, find the LRU frame
        unsigned long oldest_time = clock_ns;
        for (int i = 0; i < FRAME_COUNT; i++) {
            if (frameTable[i].last_used_time < oldest_time) {
                oldest_time = frameTable[i].last_used_time;
                frame = i;
            }
        }
        printf("OSS: Replacing frame %d (P%d, page %d)\n", frame,
               frameTable[frame].process_id, frameTable[frame].page_number);

        // Write back if dirty
        if (frameTable[frame].dirty) {
            printf("OSS: Dirty bit set for frame %d, writing back to memory\n", frame);
        }

        // Remove the page from the old process's page table
        pageTable[frameTable[frame].process_id][frameTable[frame].page_number] = -1;
    }

    // Load the new page into the frame
    frameTable[frame].occupied = 1;
    frameTable[frame].dirty = (action == 2); // Set dirty if it's a write
    frameTable[frame].process_id = process_id;
    frameTable[frame].page_number = page_number;
    frameTable[frame].last_used_time = clock_ns;

    // Update the page table
    pageTable[process_id][page_number] = frame;

    printf("OSS: Address %d loaded into frame %d for P%d (%s)\n", page_number, frame, process_id,
           action == 1 ? "read" : "write");
}

void printMemoryState() {
    printf("Current memory layout at time %d ns:\n", clock_ns);
    printf("Frame Table:\n");
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frameTable[i].occupied) {
            printf("Frame %d: P%d, Page %d, Dirty: %d, LastUsed: %lu\n", i,
                   frameTable[i].process_id, frameTable[i].page_number,
                   frameTable[i].dirty, frameTable[i].last_used_time);
        } else {
            printf("Frame %d: Empty\n", i);
        }
    }

    printf("\nPage Tables:\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        printf("P%d: ", i);
        for (int j = 0; j < PAGE_COUNT; j++) {
            printf("%d ", pageTable[i][j]);
        }
        printf("\n");
    }
}

void cleanup() {
    printf("OSS: Cleaning up resources...\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0) {
            kill(pids[i], SIGKILL);
            waitpid(pids[i], NULL, 0);
        }
    }

    key_t key = ftok("oss.c", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    msgctl(msgid, IPC_RMID, NULL);
    exit(0);
}
