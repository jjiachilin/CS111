# NAME: Joseph Lin
# EMAIL: jj.lin42@gmail.com
# ID: 505111868

default: client server

client:
	gcc -Wall -Wextra -o lab1b-client -lz lab1b-client.c

server:
	gcc -Wall -Wextra -o lab1b-server -lz lab1b-server.c

clean:
	rm -f lab1b-505111868.tar.gz lab1b-client lab1b-server

dist:
	tar -cvzf lab1b-505111868.tar.gz lab1b-server.c lab1b-client.c README Makefile