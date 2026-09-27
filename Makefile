CROSS_COMPILE ?= riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

HOST_CC = gcc

CFLAGS  = -Wall -Wextra -O2 -g -ffreestanding -fno-builtin -nostdlib -march=rv64gc -mabi=lp64
ASFLAGS = -march=rv64gc -mabi=lp64

SRC_DIR   = src
TEST_DIR  = test
BUILD_DIR = build

TRAP_OBJS = $(BUILD_DIR)/trap_entry.o $(BUILD_DIR)/trap_handler.o $(BUILD_DIR)/hsm.o

.PHONY: all clean test

all: $(BUILD_DIR) $(BUILD_DIR)/trap_handler.elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/trap_entry.o: $(SRC_DIR)/trap_entry.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trap_handler.o: $(SRC_DIR)/trap_handler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hsm.o: $(SRC_DIR)/hsm.c $(SRC_DIR)/hsm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trap_handler.elf: $(TRAP_OBJS)
	$(LD) -Ttext=0x80000000 -o $@ $(TRAP_OBJS)

# Host-native unit tests for the HSM matrix: compile hsm.c and
# test_hsm_matrix.c as SEPARATE translation units and link them,
# rather than #including hsm.c into the test file (which caused
# duplicate-symbol link errors).
$(BUILD_DIR)/hsm_host.o: $(SRC_DIR)/hsm.c $(SRC_DIR)/hsm.h | $(BUILD_DIR)
	$(HOST_CC) -Wall -Wextra -O0 -g -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/test_hsm_matrix.o: $(TEST_DIR)/test_hsm_matrix.c $(SRC_DIR)/hsm.h | $(BUILD_DIR)
	$(HOST_CC) -Wall -Wextra -O0 -g -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/hsm_matrix_test: $(BUILD_DIR)/hsm_host.o $(BUILD_DIR)/test_hsm_matrix.o
	$(HOST_CC) $^ -o $@

test: $(BUILD_DIR)/hsm_matrix_test
	./$(BUILD_DIR)/hsm_matrix_test

clean:
	rm -rf $(BUILD_DIR)
