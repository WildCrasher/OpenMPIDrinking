MPI_COMPILE_FLAGS = $(shell mpicc --showme:compile)
MPI_LINK_FLAGS = $(shell mpicc --showme:link)

all: run #runwithdebugger

start:
	@sh ./zabij.sh
compile: start
	mpicc $(MPI_COMPILE_FLAGS) -Wall -g chlanie2.c $(MPI_LINK_FLAGS) -o chlanie2
run: compile
	mpirun -oversubscribe -np 5 ./chlanie2
clear:
	rm ./chlanie2
runwithdebugger: compile
	mpirun -oversubscribe -np 5 xterm -e gdb -ex run --args ./chlanie2
