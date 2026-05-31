CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
TARGET = minifs
OBJS = main.o commands.o mfs.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c commands.h
	$(CC) $(CFLAGS) -c main.c

commands.o: commands.c commands.h mfs.h
	$(CC) $(CFLAGS) -c commands.c

mfs.o: mfs.c mfs.h
	$(CC) $(CFLAGS) -c mfs.c

clean:
	rm -f $(TARGET) $(OBJS)
