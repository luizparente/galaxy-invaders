CC := gcc
CSTD := -std=c23
WARN := -Wall -Wextra
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

CFLAGS := $(CSTD) $(WARN) -Iinclude $(SDL_CFLAGS) -O2
LDFLAGS := $(SDL_LIBS) -lm

BIN := galaxy_invaders
BUILD_DIR := build

SOURCES := $(shell find src -name '*.c')
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test run

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

# Unit tests exercise only the SDL-free usecases layer (pure game rules),
# so they build without linking SDL at all.
TEST_LOGIC_SOURCES := src/usecases/game_logic.c src/usecases/collision.c \
                       src/usecases/difficulty.c src/usecases/spawner.c
TEST_RUNNER := tests/test_runner

test: $(TEST_RUNNER)
	./$(TEST_RUNNER)

$(TEST_RUNNER): tests/test_main.c $(TEST_LOGIC_SOURCES)
	$(CC) $(CSTD) $(WARN) -Iinclude -O0 -g $^ -o $@ -lm

clean:
	rm -rf $(BUILD_DIR) $(BIN) $(TEST_RUNNER)
