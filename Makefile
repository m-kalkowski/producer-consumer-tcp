all: producent konsumer

producent: producent.c ringbuff.c
	gcc -Wall -c -pthread -lnsl producent.c -o producent.o
	gcc -Wall -c ringbuff.c -o ringbuff.o
	gcc -o producent -pthread -lnsl producent.o ringbuff.o
	rm ringbuff.o producent.o

konsumer: konsument.c
	gcc -Wall -lnsl konsument.c -o konsument

clean:
	rm -f producent konsument
