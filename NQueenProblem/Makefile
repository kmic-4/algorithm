# N-Queens Island Model GA
# Targets: all (default), test, asan, asan-test, clean

CC      ?= cc
WARN    := -Wall -Wextra -Wpedantic -Wconversion -Wshadow
CFLAGS  ?= -std=c11 $(WARN) -O2 -g
CPPFLAGS += -Iinclude
LDLIBS  += -lm

SAN_FLAGS := -std=c11 $(WARN) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

LIB_SRC := $(filter-out src/main.c,$(wildcard src/*.c))
HEADERS := $(wildcard include/*.h)

BUILD   := build
BIN     := $(BUILD)/nqueens_ga
TEST    := $(BUILD)/test_nqueens
ASAN_BIN  := $(BUILD)/nqueens_ga_asan
ASAN_TEST := $(BUILD)/test_nqueens_asan

.PHONY: all test asan asan-test clean

all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BIN): $(LIB_SRC) src/main.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_SRC) src/main.c -o $@ $(LDLIBS)

$(TEST): $(LIB_SRC) tests/test_main.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_SRC) tests/test_main.c -o $@ $(LDLIBS)

$(ASAN_BIN): $(LIB_SRC) src/main.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(SAN_FLAGS) $(LIB_SRC) src/main.c -o $@ $(LDLIBS)

$(ASAN_TEST): $(LIB_SRC) tests/test_main.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(SAN_FLAGS) $(LIB_SRC) tests/test_main.c -o $@ $(LDLIBS)

test: $(TEST)
	./$(TEST)

asan: $(ASAN_BIN)

asan-test: $(ASAN_TEST)
	./$(ASAN_TEST)

clean:
	rm -rf $(BUILD)

