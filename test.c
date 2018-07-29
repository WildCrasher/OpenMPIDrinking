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
		sleep(rank);
		
			for(int i = 0; i < 10; i++)
			{
				MPI_Send(tab, 3, MPI_INT, 1, 1, MPI_COMM_WORLD);
				printf("Wyslalem wiadomosc my rank is %d\n",rank);
				MPI_Recv(tab, 3, MPI_INT, 1, 1, MPI_COMM_WORLD, &status);
				printf("Otrzymalem odpowiedz %d, my rank %d\n", i, rank);
				sleep(0.5);
			}
		
		printf("koniec, my rank  %d\n", rank);
	}
	else
	{
		int message[3];
		while(1)
		{
			printf("czekam na wiadomosc, my rank is %d\n", rank);
			MPI_Recv(message, 3, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
		//	printf("Odebralem wiadomosc %d\n", message[0]);
		//	printf("Odebralem wiadomosc %d\n", message[1]);
		//	printf("Odebralem wiadomosc %d\n", message[2]);
			printf("Odpowiadam, my rank %d\n", rank);
			MPI_Send(message, 3, MPI_INT, 0, 1, MPI_COMM_WORLD);
		}
	}
	wait(0);
	MPI_Finalize();
	return 0;
}
