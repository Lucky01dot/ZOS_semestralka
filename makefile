CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
OBJ = main.o commands.o filesystem.o directory.o

all: filesystem

filesystem: $(OBJ)
	$(CC) $(CFLAGS) -o filesystem $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o filesystem
