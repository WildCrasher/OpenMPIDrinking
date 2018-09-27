#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <assert.h>

//normal consts
#define SEMCOUNT (int)1
#define FALSE (int)0
#define TRUE (int)1
#define STRUCTURE_SIZE (int)3
#define ARBITER_SIZE (int)1

//tags
#define ARBITER_REQUEST (int)1

//answers
#define NO (int)0
#define YES (int)1

#define max(a, b) \
	({ __typeof__ (a) _a = (a); \
	 __typeof__ (b) _b = (b); \
	 _a > _b ? _a : _b; })

#define min(a, b) \
	({ __typeof__ (a) _a = (a); \
	 __typeof__ (b) _b = (b); \
	 _a < _b ? _a : _b; })

int *lamport_clock;
int semaphore_clock_id;

int size, rank, range;

void up(int semid)
{
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = 1;
	buf.sem_flg = 0;
	//	printf("up %d\n",semop(semid, &buf, 1));
	semop(semid, &buf, 1);
}

void down(int semid)
{
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = 0;
	semop(semid, &buf, 1);
	//	printf("down %d\n", semop(semid, &buf, 1));
}

int sendInt(int *data, int size, int destination, int tag)
{
	int *buf = malloc((size + 1) * sizeof(int));
	memcpy(buf, data, size * sizeof(int));
	down(semaphore_clock_id);
	(*lamport_clock)++;
	//	printf("clock = %d\n", *lamport_clock);
	memcpy(buf + size, lamport_clock, sizeof(int));
	up(semaphore_clock_id);
	return MPI_Send(buf, size + 1, MPI_INT, destination, tag, MPI_COMM_WORLD);
}

int recvInt(int *data, int size, int source, int tag, MPI_Status *status)
{
	int *buf;
	buf = malloc((size + 1) * sizeof(int));
	int ret = MPI_Recv(buf, size + 1, MPI_INT, source, tag, MPI_COMM_WORLD, status);

	down(semaphore_clock_id);
	*lamport_clock = max(*lamport_clock, buf[size]) + 1;
	printf("clock = %d\n", *lamport_clock);
	up(semaphore_clock_id);
	memcpy(data, buf, size * sizeof(int));
	free(buf);
	return ret;
}

void Send_To_All(int *buf, int size, int my_rank, int tag)
{
	for (int i = 0; i < size; i++)
	{
		if (i != my_rank)
		{
			sendInt(buf, STRUCTURE_SIZE, i, tag);
			printf("I sent to %d and my rank is %d\n", i, my_rank);
		}
	}
}

void *childThread()
{
	return NULL;
}

int main(int argc, char **argv)
{

	int provided;
	int ret = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	assert(ret == 0 && provided == MPI_THREAD_MULTIPLE);

	semaphore_clock_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_clock_id, 0, SETVAL, (int)1);

	lamport_clock = malloc(sizeof(int));
	*lamport_clock = 0;

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	srand(time(0));

	MPI_Barrier(MPI_COMM_WORLD); //pozostalosc po testach, mysle ze mozna to usunac, ale zobaczymy

	pthread_t thread;
	pthread_create(&thread, NULL, childThread, NULL);


	int message = 0;
	int message_size = 0;
	while (1)
	{
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_INT, &message_size);

		recvInt(&message, message_size, MPI_ANY_SOURCE, MPI_ANY_TAG, &status);

		if(status.MPI_TAG == )

		printf("I received tag %d from %d and my rank is %d\n", status.MPI_TAG, status.MPI_SOURCE, rank);

	}

	semctl(semaphore_clock_id, 0, IPC_RMID);
	printf("MPI finallize\n");
	MPI_Finalize();
	return 0;
}
