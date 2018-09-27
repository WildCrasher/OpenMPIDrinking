MPI_COMPILE_FLAGS = $(shell mpicc --showme:compile)
MPI_LINK_FLAGS = $(shell mpicc --showme:link)

all: run #runwithdebugger

start:
	@sh ./zabij.sh
compile: start
	mpicc $(MPI_COMPILE_FLAGS) -Wall -g chlanie3.c $(MPI_LINK_FLAGS) -o chlanie3
run: compile
	mpirun -np 3 ./chlanie3
clear:
	rm ./chlanie3
runwithdebugger: compile
	mpirun -np 4 xterm -e gdb -ex run --args ./chlanie3
