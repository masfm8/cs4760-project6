# Project 6 - Memory Management Simulation

This project simulates a virtual memory management system
using paging and frame allocation. The simulation includes
multiple processes requesting read/write operations to 
virtual addresses, and the Operating System Simulator 
(oss.c) handles these requests through a frame table and 
page tables.

The project implements:

Page replacement policies.
Handling of page faults.
Process communication via shared memory and message queues.
## Repository
GitHub: https://github.com/masfm8/cs4760-project6


## Files
- `oss.c`:
- `user.c`:
- `Makefile`:
- `README.md`:
How to Compile the Code
make
This will generate the following executable files:

oss - The Operating System Simulator.
user - The user process that generates memory access requests.
How to Run the Simulation
After compiling the project, run the simulation using:
./oss 
Viewing the Output
Console Output
The program will display simulation events on the console, such as:
-User processes starting, reading, or writing pages.
-Page faults and replacements.
-Current memory layout, including the frame table and 
page tables.

Log File Output
-A detailed log of the simulation is stored in oss.log.
-To view the log file:
cat oss.log
Sample Output
OSS: Starting simulation
USER PID: 12345 started
USER PID: 12345 writing to page 19
USER PID: 12346 started
USER PID: 12346 reading from page 0
OSS: P0 requesting read of address 19 at time 100000
OSS: Address 19 loaded into frame 0 for P0 (read)

Current memory layout at time 100000 ns:
Frame Table:
Frame 0: P0, Page 19, Dirty: 1, LastUsed: 100000
Frame 1: Empty
Frame 2: Empty
Frame 3: Empty
Frame 4: Empty
Frame 5: Empty
Frame 6: Empty
Frame 7: Empty

Page Tables:
P0:  0 -1 -1 -1 -1 -1 -1 -1 -1 -1 ...
P1: -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ...
...


Log File (oss.log)
The oss.log file will contain detailed information, such as:

-Requests for pages.
-Page faults and replacements.
-Final memory layout.
Key Features
-Memory Allocation: Simulates the allocation of memory frames to processes.
-Paging: Each process has its own page table with PAGE_COUNT = 32 entries.
-Page Replacement Policy: The simulation uses the Least Recently Used (LRU) policy to handle page faults.
-Shared Memory and Message Queues: Inter-process communication between oss and user.
Cleanup:
make clean
Known Issues and Limitations
The simulation may produce a large log file due to the verbose output of page tables for all processes.
Limited to 18 processes (MAX_PROCESSES) and 8 frames (FRAME_COUNT) for simplicity.

