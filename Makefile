client: client.c
	gcc -o client client.c -Wall -Wextra -pedantic -g -lssl -lcrypto

clean:
	rm -f client

.PHONY: clean

