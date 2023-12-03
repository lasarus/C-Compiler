# Variables
CC = gcc
CONFIG_PATH = config.h
CFLAGS = -Wall -Wextra -Werror -pedantic -I$(SRC_DIR) -I. -MMD -DCONFIG_PATH="\"$(CONFIG_PATH)\""
CFLAGS_SELF = -Wall -Wextra -Werror -I$(SRC_DIR) -I. -MD -DCONFIG_PATH="\"$(CONFIG_PATH)\""
SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj
ASM_DIR = asm
BIN_DIR = bin

# Compiler source files and object files
SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/1/%.o)
DEPS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/1/%.d)

# Second generation object files
OBJS2 = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/2/%.o)
DEPS += $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/2/%.d)

# Third generation object files
OBJS3 = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/3/%.o)
DEPS += $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/3/%.d)

# Second generation asm files.
ASM_DEBUG = $(SRCS:$(SRC_DIR)/%.c=$(ASM_DIR)/asm_debug/%.s)
DEPS += $(SRCS:$(SRC_DIR)/%.c=$(ASM_DIR)/asm_debug/%.d)

# Test source files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
SHOULD_FAIL_TEST_SRCS = $(wildcard $(TEST_DIR)/should_fail/*.c)

TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(OBJ_DIR)/1/%.o)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BIN_DIR)/tests/1/%)

TEST_OBJS2 = $(TEST_SRCS:$(TEST_DIR)/%.c=$(OBJ_DIR)/2/%.o)
TEST_BINS2 = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BIN_DIR)/tests/2/%)

TEST_ASM = $(TEST_SRCS:$(TEST_DIR)/%.c=$(ASM_DIR)/%.s)
TEST_BINS_ASM = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BIN_DIR)/tests/asm/%)

TEST_OBJS_WINE = $(TEST_SRCS:$(TEST_DIR)/%.c=$(OBJ_DIR)/wine/%.o)
TEST_BINS_WINE = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BIN_DIR)/tests/wine/%.exe)

# Compiler binary
COMPILER = $(BIN_DIR)/cc
COMPILER2 = $(BIN_DIR)/cc2
COMPILER3 = $(BIN_DIR)/cc3
COMPILER_ASM_DBG = $(BIN_DIR)/cc_asm_dbg

# Default target
all: $(COMPILER)

# Compile the compiler
$(COMPILER): config.h $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile source files into object files
$(OBJ_DIR)/1/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

config.h:
	cp config.def.h $@

# Self compilation targets.
self-compile: $(COMPILER3)

$(COMPILER2): config.h $(OBJS2)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) -no-pie

$(OBJ_DIR)/2/%.o: $(SRC_DIR)/%.c $(COMPILER)
	@mkdir -p $(dir $@)
	$(COMPILER) $(CFLAGS_SELF) -c $< -o $@

$(COMPILER3): config.h $(OBJS3)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS3) -no-pie

$(OBJ_DIR)/3/%.o: $(SRC_DIR)/%.c $(COMPILER2)
	@mkdir -p $(dir $@)
	$(COMPILER2) $(CFLAGS_SELF) -c $< -o $@

$(COMPILER_ASM_DBG): config.h $(ASM_DEBUG)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -g -o $@ $(ASM_DEBUG) -no-pie

$(ASM_DIR)/asm_debug/%.s: $(SRC_DIR)/%.c $(COMPILER)
	@mkdir -p $(dir $@)
	$(COMPILER) $(CFLAGS_SELF) -S $< -o $@

# Compile and run tests.
check: run-tests run-tests2 run-tests-asm run-should-fail-tests compare-generations

compare-generations: $(COMPILER2) $(COMPILER3)
	@diff $(COMPILER2) $(COMPILER3) ; \
	if [ $$? -ne 0 ]; then \
		echo "Second and third generation not equal." ; \
		exit 1 ; \
	else \
		echo "Second and third generation equal." ; \
	fi ; \

run-tests: $(TEST_BINS)
	@for test in $^ ; do \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed." ; \
			exit 1 ; \
		else \
			echo "Test $$test passed." ; \
		fi ; \
	done

run-tests2: $(TEST_BINS2)
	@for test in $^ ; do \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed (second generation)." ; \
			exit 1 ; \
		else \
			echo "Test $$test passed (second generation)." ; \
		fi ; \
	done

run-tests-asm: $(TEST_BINS_ASM)
	@for test in $^ ; do \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed (asm)." ; \
			exit 1 ; \
		else \
			echo "Test $$test passed (asm)." ; \
		fi ; \
	done

run-should-fail-tests: $(SHOULD_FAIL_TEST_SRCS) $(COMPILER)
	@for test in $(SHOULD_FAIL_TEST_SRCS) ; do \
		$(COMPILER) -S $$test -o tmp.s >/dev/null; \
		if [ $$? -eq 1 ]; then \
			echo "Test $$test passed." ; \
		else \
			echo "Test $$test failed." ; \
			exit 1 ; \
		fi ; \
	done

# Rules for first generation objects.
$(OBJ_DIR)/1/%.o: $(TEST_DIR)/%.c $(COMPILER)
	@mkdir -p $(dir $@)
	@$(COMPILER) $< -c -o $@

$(BIN_DIR)/tests/1/%: $(OBJ_DIR)/1/%.o
	@mkdir -p $(dir $@)
	@gcc $< -o $@ -no-pie

# Rules for first generation assembly.
$(ASM_DIR)/%.s: $(TEST_DIR)/%.c $(COMPILER)
	@mkdir -p $(dir $@)
	$(COMPILER) $< -S -o $@

$(BIN_DIR)/tests/asm/%: $(ASM_DIR)/%.s
	@mkdir -p $(dir $@)
	@gcc $< -o $@ -no-pie

# Rules for second generation objects.
$(OBJ_DIR)/2/%.o: $(TEST_DIR)/%.c $(COMPILER2)
	@mkdir -p $(dir $@)
	@$(COMPILER2) $< -c -o $@

$(BIN_DIR)/tests/2/%: $(OBJ_DIR)/2/%.o
	@mkdir -p $(dir $@)
	@gcc $< -o $@ -no-pie

#
check-wine: $(TEST_BINS_WINE)
	@for test in $^ ; do \
		wine $$test ; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed (asm)." ; \
			exit 1 ; \
		else \
			echo "Test $$test passed (asm)." ; \
		fi ; \
	done

# Rules for generating wine-compatible binaries.
$(OBJ_DIR)/wine/%.o: $(TEST_DIR)/%.c $(COMPILER)
	@mkdir -p $(dir $@)
	@$(COMPILER) $< -c -o $@

$(BIN_DIR)/tests/wine/%.exe: $(OBJ_DIR)/wine/%.o
	@mkdir -p $(dir $@)
	@x86_64-w64-mingw32-gcc $< -o $@ -no-pie

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(ASM_DIR)

# Measure the time it takes for the compiler to compile itself.
benchmark: $(COMPILER2) $(SRCS)
	time for test in $(SRCS) ; do \
		$(COMPILER2) $(CFLAGS_SELF) -c $$test -o $(OBJ_DIR)/tmp.o ; \
	done

.PHONY: all check self-compile run-tests run-tests2 compare-generations clean benchmark check-wine run-should-fail-tests

-include $(DEPS)
