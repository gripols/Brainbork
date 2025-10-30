CC       := gcc 
CFLAGS   := -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  :=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -ldl
endif

ARCH := $(shell uname -m)

ifeq ($(ARCH),x86_64)
	ARCH_DIR := x86_64
	ARCH_SRCS := src/jit/arch/x86_64/jit_x86_64.c
else ifeq ($(ARCH),aarch64)
	ARCH_DIR := aarch64
	ARCH_SRCS := src/jit/arch/aarch64/jit_aarch64.c
else ifeq ($(ARCH),arm64)
	ARCH_DIR := aarch64
	ARCH_SRCS := src/jit/arch/aarch64/jit_aarch64.c
else
	$(warning "JIT not supported for architecture: $(ARCH). JIT will be disabled.")
	ARCH_SRCS :=
endif

BASE_SRCS := \
	src/main.c \
	src/util.c \
	src/vm.c \
	src/compiler.c \
	src/jit/jit.c \
	src/jit/jit_common.c \
	src/jit/runtime/jit_runtime.c

SRCS := $(BASE_SRCS) $(ARCH_SRCS)

BUILD_DIR := build
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)
TARGET := brainbork

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $@"
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "Running Brainbork example: examples/mandelbrot.bf"
	./$(TARGET) examples/mandelbrot.bf

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean run
