# Variables
CC = gcc
CFLAGS = -Wall -g -std=c11 -D_POSIX_C_SOURCE=200809L
TARGETS = oss user

# Build all targets
all: $(TARGETS)

oss: oss.c
	$(CC) $(CFLAGS) -o oss oss.c

user: user.c
	$(CC) $(CFLAGS) -o user user.c

# Run the program
run: clean all
	./oss

# Clean up executables, shared memory, message queues, and lingering processes
clean:
	@echo "Removing executables..."
	rm -f $(TARGETS)
	@echo "Removing shared memory segments owned by this user..."
	ipcs -m 2>/dev/null | grep SER | awk '{print $$2}' | xargs -r ipcrm -m || true
	@echo "Removing message queues owned by this user..."
	ipcs -q 2>/dev/null | grep SER | awk '{print $$2}' | xargs -r ipcrm -q || true
	@echo "Killing lingering processes named 'user' owned by this user..."
	ps -u $$USER 2>/dev/null | grep 'user' | awk '{print $$1}' | xargs -r kill -9 || true
	@echo "Cleanup complete."

# Phony targets
.PHONY: all clean run
