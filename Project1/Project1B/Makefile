all: client server
	
client: client.c
	@gcc -pthread -std=gnu99 -Wall -g -o  client client.c -lmcrypt
	
server: server.c
	@gcc -pthread  -std=gnu99 -Wall -g -o  server server.c -lmcrypt

clean:
	@rm -rf server client
