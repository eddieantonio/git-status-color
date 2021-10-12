BIN = git-status-color

CFLAGS = -std=c99 -Wall -pedantic

all: $(BIN)

test: $(BIN)
	./$(BIN) && git rev-parse --abbrev-ref HEAD 

