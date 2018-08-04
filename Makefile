all: run #runwithdebugger

start:
	@sh ./zabij.sh
compile: start
	mpicc -Wall -o chlanie2 chlanie2.c
run: compile
	mpirun --oversubscribe -np 10 chlanie2
#runwithdebugger: compile
#	mpirun --oversubscribe -np 3 xterm -hold -e gdb -ex run --args chlanie2
