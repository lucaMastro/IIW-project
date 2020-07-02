all: compile
adapt: compile-adaptive

compile: 
	gcc -o server src/server/server-side.c -pthread -lrt
	gcc -o client src/client/client-side.c -pthread -lrt

compile-adaptive:
	gcc -o server src/server/server-side.c -pthread -lrt -DADAPT_TO
	gcc -o client src/client/client-side.c -pthread -lrt -DADAPT_TO
	
clean:
	rm server client
	#rm src/client/client-files/thisIsATestFile.txt
	#rm src/server/server-files/prova.png*

