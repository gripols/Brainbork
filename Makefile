CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = buslang
SRC = buslang.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

