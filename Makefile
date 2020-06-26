all: clean
	gcc -o server src/server/server-side.c -pthread
	gcc -o client src/client/client-side.c

compile: 
	gcc -o server src/server/server-side.c -pthread
	gcc -o client src/client/client-side.c
clean:
	rm server client
	#rm src/client/client-files/thisIsATestFile.txt
	#rm src/server/server-files/prova.png*

