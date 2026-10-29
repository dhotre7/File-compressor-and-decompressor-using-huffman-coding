CC = gcc
CFLAGS = -Wall -Wextra

all: main compressor decompressor

main: main.c compressor.c decompressor.c compressor.h decompressor.h
	$(CC) $(CFLAGS) -o main main.c compressor.c decompressor.c

compressor: compressor.c compressor.h
	$(CC) $(CFLAGS) -DSTANDALONE -o compressor compressor.c

decompressor: decompressor.c decompressor.h
	$(CC) $(CFLAGS) -DSTANDALONE -o decompressor decompressor.c

clean:
	rm -f main compressor decompressor *.o
