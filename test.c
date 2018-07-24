#include <mpi.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char** argv )
{
	MPI_Init(&argc, &argv);
	int size, rank;

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	printf("start \n");

	if(fork() != 0)
	{
		int tab[3];
		tab[0] = 1;
		tab[1] = 2;
		tab[2] = 3;
		if(rank == 0)
		{
			MPI_Send(tab, 3, MPI_INT, 1, 1, MPI_COMM_WORLD);
			printf("Wyslalem wiadomosc my rank is %d\n",rank);
			sleep(0.5);
		}
		while(1){sleep(10);}
	}
	else
	{
		sleep(3);
		int message[3];
		while(1)
		{
			printf("czekam na wiadomosc, my rank is %d", rank);
			MPI_Recv(&message, 3, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
			printf("Odebralem wiadomosc %d\n", message[0]);
			printf("Odebralem wiadomosc %d\n", message[1]);
			printf("Odebralem wiadomosc %d\n", message[2]);
		}
	}
	wait(0);
	MPI_Finalize();
	return 0;
}
