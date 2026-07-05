CC = $(shell which cc 2>/dev/null)

# CC = clang
RC = windres

ARCH ?= 64

SRC_DIR = .
BUILD_DIR = out

ifeq ($(ARCH),32)
    ARCH_FLAGS = -m32
    RCFLAGS = -F pe-i386
	TARGET = $(BUILD_DIR)/Wintamins32.exe
else
    ARCH_FLAGS = -m64
    RCFLAGS = -F pe-x86-64
	TARGET = $(BUILD_DIR)/Wintamins64.exe
endif

# CFLAGS = -g -Og -flto -MMD -MP $(ARCH_FLAGS)
CFLAGS = -static -O3 -flto -MMD -MP $(ARCH_FLAGS)

# LFLAGS = $(CFLAGS) -Wall -Wextra  -mwindows
LFLAGS = $(CFLAGS) -Wl,-s -mwindows
LDLIBS = -lcomctl32 -ldwmapi

SRCS = $(wildcard $(SRC_DIR)/*.c)
HDRS = $(wildcard $(BUILD_DIR)/*.d)
RSRC = $(SRC_DIR)/resources.rc
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
RC_OBJ = $(patsubst $(SRC_DIR)/%.rc, $(BUILD_DIR)/%.res.o, $(RSRC))

.PHONY: all compiledb

all: $(BUILD_DIR) $(TARGET)

$(TARGET): $(OBJS) $(RC_OBJ)
	$(CC) $(LFLAGS) -o "$@" $(OBJS) $(RC_OBJ) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o "$@"

$(BUILD_DIR)/%.res.o: $(SRC_DIR)/%.rc
	$(RC) $(RCFLAGS) -i $< -o "$@"

$(BUILD_DIR):
	mkdir -p "$(BUILD_DIR)"

compiledb:
	compiledb -o "$(SRC_DIR)/compile_commands.json" -n make
	
clean:
	rm -rf "$(BUILD_DIR)"

-include $(HDRS)
