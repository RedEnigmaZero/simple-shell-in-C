CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99
TARGET = sshell
SRC = sshell.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean