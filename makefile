CC = gcc
CFLAGS = -g -Wall
TARGET = jms_console jms_coord pool
HEADERS = list.h 

.PHONY: clean all

all: pool jms_console jms_coord

pool.o: pool.c $(HEADERS)
	$(CC) $(CFLAGS) -c pool.c -o pool.o

pool: pool.o
	$(CC) $(CFLAGS)  pool.o -o pool

list.o: list.c $(HEADERS)
	$(CC) $(CFLAGS) -c list.c -o list.o

new_coord.o: new_coord.c $(HEADERS)
	$(CC) $(CFLAGS) -c new_coord.c -o new_coord.o

jms_coord.o: jms_coord.c $(HEADERS)
	$(CC) $(CFLAGS) -c jms_coord.c -o jms_coord.o

jms_coord: jms_coord.o new_coord.o list.o
	$(CC) $(CFLAGS)  jms_coord.o new_coord.o list.o -o jms_coord

console_functions.o: console_functions.c 
	$(CC) $(CFLAGS) -c console_functions.c -o console_functions.o

jms_console.o: jms_console.c 
	$(CC) $(CFLAGS) -c jms_console.c -o jms_console.o

jms_console: jms_console.o console_functions.o
	$(CC) $(CFLAGS)  jms_console.o console_functions.o -o jms_console



clean:
	rm -rf *.o $(TARGET)


