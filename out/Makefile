CC = clang
RC = windres

# CFLAGS = -Wall -Wextra -g -O2 -mwindows -flto -MMD -MP
CFLAGS = -O2 -mwindows -flto -MMD -MP
LDLIBS = -lcomctl32 -ldwmapi

TARGET = Wintamins.exe

SRC_DIR = ..
BUILD_DIR = .

SRCS = $(wildcard $(SRC_DIR)/*.c)
HDRS = $(wildcard *.d)
RSRC = $(SRC_DIR)/resources.rc
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
RC_OBJ = $(patsubst $(SRC_DIR)/%.rc, $(BUILD_DIR)/%.res.o, $(RSRC))

.PHONY: all kill compiledb

all: $(TARGET)

$(TARGET): $(OBJS) $(RC_OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(RC_OBJ) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.res.o: $(SRC_DIR)/%.rc
	$(RC) -i $< -o $@

compiledb:
	compiledb -o $(SRC_DIR)/compile_commands.json -n make

-include $(HDRS)
