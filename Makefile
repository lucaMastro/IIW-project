all: clean compile

compile: 
	gcc -o server src/server/server-side.c -pthread -lrt
	gcc -o client src/client/client-side.c -pthread -lrt
clean:
	rm server client
	#rm src/client/client-files/thisIsATestFile.txt
	#rm src/server/server-files/prova.png*

