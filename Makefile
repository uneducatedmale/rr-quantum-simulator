CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11
LDLIBS = -lm
TARGET = rrsim
SRC = src/main.c src/parser.c src/queue.c src/scheduler.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

test: $(TARGET)
	bash tests/run.sh

clean:
	rm -f $(TARGET)
