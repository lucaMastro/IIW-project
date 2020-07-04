default: compile-adaptive
static: compile

compile: 
	gcc -o server src/server/server-side.c -pthread -lrt
	gcc -o client src/client/client-side.c -pthread -lrt

compile-adaptive:
	gcc -o server src/server/server-side.c -pthread -lrt -DADAPT_TO
	gcc -o client src/client/client-side.c -pthread -lrt -DADAPT_TO
	
get-time: 
	gcc -o server src/server/server-side.c -pthread -lrt -DGET_TIME
	gcc -o client src/client/client-side.c -pthread -lrt -DGET_TIME

get-time-adaptive:
	gcc -o server src/server/server-side.c -pthread -lrt -DADAPT_TO -DGET_TIME
	gcc -o client src/client/client-side.c -pthread -lrt -DADAPT_TO -DGET_TIME
