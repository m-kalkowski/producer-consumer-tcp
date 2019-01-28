all: producent konsumer

producent: producent.c
	gcc -Wall -pthread -lnsl producent.c -o producent

konsumer: konsument.c
	gcc -Wall -lnsl konsument.c -o konsument

clean:
	rm -f producent konsument
