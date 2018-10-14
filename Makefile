CC=gcc
DEPS=elk.h
OBJ=elk.o
CFLAGS=-I.

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

elk: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)


clean:
	rm *.o
	rm elk 
