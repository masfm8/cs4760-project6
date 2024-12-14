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
#define FRAME_COUNT 8   // Adjusted for easier tracking
#define PAGE_COUNT 32
#define TIME_INCREMENT 100000
#define SIMULATION_TIME_LIMIT 20000000

typedef struct {
    int occupied;
    int dirty;
    int process_id;
    int page_number;
    unsigned long last_used_time;
} FrameTableEntry;

typedef struct {
    long mtype;
    int pid;
    int page_number;
    int action; // 1 = Read, 2 = Write, 3 = Terminate
} Message;

FrameTableEntry frameTable[FRAME_COUNT];
int pageTable[MAX_PROCESSES][PAGE_COUNT];
pid_t pids[MAX_PROCESSES];
int clock_ns = 0;
int activeProcesses = 0;
FILE *logfile;  // Log file pointer

void initializeFrameTable();
void initializePageTable();
void printMemoryState();
void handlePageRequest(int process_id, int page_number, int action);
void cleanup();
int selectVictimFrame();

void signalHandler(int sig) {
    cleanup();
    exit(0);
}

int main() {
    int msgid;
    key_t key = ftok("oss.c", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);

    signal(SIGINT, signalHandler);

    logfile = fopen("oss.log", "w");
    if (logfile == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    initializeFrameTable();
    initializePageTable();

    printf("OSS: Starting simulation\n");
    fprintf(logfile, "OSS: Starting simulation\n");

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

        Message msg;
        if (msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) != -1) {
            if (msg.action == 1 || msg.action == 2) {
                printf("OSS: P%d requesting %s of address %d at time %d\n", 
                        msg.pid % MAX_PROCESSES, 
                        msg.action == 1 ? "read" : "write", 
                        msg.page_number, clock_ns);
                fprintf(logfile, "OSS: P%d requesting %s of address %d at time %d\n", 
                        msg.pid % MAX_PROCESSES, 
                        msg.action == 1 ? "read" : "write", 
                        msg.page_number, clock_ns);
                handlePageRequest(msg.pid % MAX_PROCESSES, msg.page_number, msg.action);
            } else if (msg.action == 3) {
                printf("OSS: Process P%d terminating\n", msg.pid % MAX_PROCESSES);
                fprintf(logfile, "OSS: Process P%d terminating\n", msg.pid % MAX_PROCESSES);
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
        struct timespec ts = {0, 1000000};
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
            pageTable[i][j] = -1;
        }
    }
}

void handlePageRequest(int process_id, int page_number, int action) {
    int frame = -1;

    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frameTable[i].occupied && frameTable[i].process_id == process_id &&
            frameTable[i].page_number == page_number) {
            frame = i;
            frameTable[i].last_used_time = clock_ns;
            printf("OSS: Address %d found in frame %d, P%d (%s)\n", 
                    page_number, i, process_id, action == 1 ? "read" : "write");
            fprintf(logfile, "OSS: Address %d found in frame %d, P%d (%s)\n", 
                    page_number, i, process_id, action == 1 ? "read" : "write");
            if (action == 2) frameTable[i].dirty = 1;
            return;
        }
    }

    for (int i = 0; i < FRAME_COUNT; i++) {
        if (!frameTable[i].occupied) {
            frame = i;
            break;
        }
    }

    if (frame == -1) frame = selectVictimFrame();

    if (frameTable[frame].dirty) {
        printf("OSS: Writing back dirty frame %d\n", frame);
        fprintf(logfile, "OSS: Writing back dirty frame %d\n", frame);
    }

    pageTable[frameTable[frame].process_id][frameTable[frame].page_number] = -1;

    frameTable[frame].occupied = 1;
    frameTable[frame].dirty = (action == 2);
    frameTable[frame].process_id = process_id;
    frameTable[frame].page_number = page_number;
    frameTable[frame].last_used_time = clock_ns;

    pageTable[process_id][page_number] = frame;
    printf("OSS: Address %d loaded into frame %d for P%d (%s)\n", page_number, frame, process_id, action == 1 ? "read" : "write");
    fprintf(logfile, "OSS: Address %d loaded into frame %d for P%d (%s)\n", page_number, frame, process_id, action == 1 ? "read" : "write");
}

int selectVictimFrame() {
    unsigned long oldest_time = clock_ns;
    int victim = 0;
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frameTable[i].last_used_time < oldest_time) {
            oldest_time = frameTable[i].last_used_time;
            victim = i;
        }
    }
    return victim;
}

void printMemoryState() {
    fprintf(logfile, "\nCurrent memory layout at time %d ns:\n", clock_ns);
    fprintf(logfile, "Frame Table:\n");
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frameTable[i].occupied) {
            fprintf(logfile, "Frame %d: P%d, Page %d, Dirty: %d, LastUsed: %lu\n", 
                   i, frameTable[i].process_id, frameTable[i].page_number, frameTable[i].dirty, frameTable[i].last_used_time);
        } else {
            fprintf(logfile, "Frame %d: Empty\n", i);
        }
    }
    fprintf(logfile, "\nPage Tables:\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        fprintf(logfile, "P%d: ", i);
        for (int j = 0; j < PAGE_COUNT; j++) {
            fprintf(logfile, "%2d ", pageTable[i][j]);
        }
        fprintf(logfile, "\n");
    }
}

void cleanup() {
    printf("OSS: Cleaning up resources...\n");
    fprintf(logfile, "OSS: Cleaning up resources...\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pids[i] > 0) kill(pids[i], SIGKILL);
    }

    fclose(logfile);

    key_t key = ftok("oss.c", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    msgctl(msgid, IPC_RMID, NULL);
}
