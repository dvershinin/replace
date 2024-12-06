CC = gcc
# enable all warnings and use the C99 standard, inheriting external CFLAGS
CFLAGS = -Wall -std=c99 $(CFLAGS)
TARGET = replace

all: $(TARGET)

$(TARGET): replace.c
	$(CC) $(CFLAGS) -o $(TARGET) replace.c

clean:
	rm -f $(TARGET)
