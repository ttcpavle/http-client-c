client: client.c
	gcc -o client client.c

clean:
	rm -f client

.PHONY: clean

