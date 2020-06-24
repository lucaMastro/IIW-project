all:
	$(clean)
	gcc -o src/server/server src/server/server-side.c -pthread
	gcc -o src/client/client src/client/client-side.c

clean:
	rm src/server/server src/client/client
