CC = gcc
RC = windres

ARCH ?= 64

ifeq ($(ARCH),32)
    ARCH_FLAGS = -m32
    RCFLAGS = -F pe-i386
    BUILD_DIR = build32
	TARGET = Wintamins32.exe
else
    ARCH_FLAGS = -m64
    RCFLAGS = -F pe-x86-64
    BUILD_DIR = build64
	TARGET = Wintamins64.exe
endif

# CFLAGS = -Wall -Wextra -g -Og -mwindows -flto -MMD -MP $(ARCH_FLAGS)
CFLAGS = -Wl,-s -static -O3 -mwindows -flto -MMD -MP $(ARCH_FLAGS)
LDLIBS = -lcomctl32 -ldwmapi

SRC_DIR = ..
BUILD_DIR = .

SRCS = $(wildcard $(SRC_DIR)/*.c)
HDRS = $(wildcard $(BUILD_DIR)*.d)
RSRC = $(SRC_DIR)/resources.rc
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
RC_OBJ = $(patsubst $(SRC_DIR)/%.rc, $(BUILD_DIR)/%.res.o, $(RSRC))

.PHONY: all compiledb

all: $(TARGET)

$(TARGET): $(OBJS) $(RC_OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(RC_OBJ) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.res.o: $(SRC_DIR)/%.rc
	$(RC) $(RCFLAGS) -i $< -o $@

compiledb:
	compiledb -o $(SRC_DIR)/compile_commands.json -n make
	
clean:
	rm -rf *.d *.o *.exe

-include $(HDRS)
