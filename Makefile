CC = gcc
PKGS = gtk+-3.0 x11 xext json-glib-1.0
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags $(PKGS))
LDFLAGS = $(shell pkg-config --libs $(PKGS))

SRC = src/main.c src/config.c src/overlay_window.c src/tray.c
OBJ = $(SRC:.c=.o)
BIN = bin/crosshair-overlay

TEST_SRC = tests/test_config.c src/config.c
TEST_BIN = bin/test_config

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)
	./$(TEST_BIN)

clean:
	rm -f $(OBJ) $(TEST_BIN)
	rm -rf bin
