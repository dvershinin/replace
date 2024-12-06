CC = gcc
CFLAGS = -std=c99 -Wall
TARGET = replace

all: $(TARGET)

$(TARGET): replace.c
	$(CC) $(CFLAGS) -o $(TARGET) replace.c

clean:
	rm -f $(TARGET)
