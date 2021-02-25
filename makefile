programma: lotto_server.o lotto_client.o
	gcc -Wall lotto_server.o -o lotto_server 
	gcc -Wall lotto_client.o -o lotto_client

lotto_server.o: lotto_server.c
	gcc -g -c -std=c89 lotto_server.c

lotto_client.o: lotto_client.c
	gcc -g -c -std=c89 lotto_client.c

clean:
	rm *o programma
