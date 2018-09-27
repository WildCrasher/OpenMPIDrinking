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
#define SEMCOUNT 1
#define FALSE 0
#define TRUE 1
#define STRUCTURE_SIZE 3
#define ARBITER_SIZE 1

//tags
#define ARBITER_REQUEST 6
#define ARBITER_ANSWER 7

//answers
#define NO 0
#define YES 1

#define max(a, b) \
	({ __typeof__ (a) _a = (a); \
	 __typeof__ (b) _b = (b); \
	 _a > _b ? _a : _b; })

#define min(a, b) \
	({ __typeof__ (a) _a = (a); \
	 __typeof__ (b) _b = (b); \
	 _a < _b ? _a : _b; })

typedef struct ArbiterRequest
{
	int timestamp;
	int rank;
} ArbiterRequest;

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

void Append_To_Query(ArbiterRequest request, ArbiterRequest *query, int *queryFirstIndex, int *queryLastIndex)
{
	query[*queryLastIndex] = request;
	(*queryLastIndex)++;
}

ArbiterRequest Pick_From_Query(ArbiterRequest *query, int *queryFirstIndex, int *queryLastIndex)
{
	if (*queryFirstIndex < *queryLastIndex)
	{
		(*queryFirstIndex)++;
		return query[*queryFirstIndex - 1];
	}
	ArbiterRequest dummy;
	dummy.timestamp = 0;
	dummy.rank = -1;
	return dummy;
}

int sendInt(int *data, int size, int destination, int tag)
{
	int *buf = malloc((size + 1) * sizeof(int));
	memcpy(buf, data, size * sizeof(int));
	down(semaphore_clock_id);
	(*lamport_clock)++;
	// printf("clock = %d\n", *lamport_clock);
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
	// printf("clock = %d\n", *lamport_clock);
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
			sendInt(buf, 1, i, tag);
			// printf("I sent to %d and my rank is %d\n", i, my_rank);
		}
	}
}

void *childThread()
{
	// sleep(1);
	down(semaphore_clock_id);
	int message = *lamport_clock;
	up(semaphore_clock_id);
	Send_To_All(&message, size, rank, ARBITER_REQUEST);

	while (1)
		;
	return NULL;
}

int main(int argc, char **argv)
{
	int provided;
	int ret = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	assert(ret == 0 && provided == MPI_THREAD_MULTIPLE);

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	semaphore_clock_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_clock_id, 0, SETVAL, (int)1);

	lamport_clock = malloc(sizeof(int));
	*lamport_clock = 0;

	int queryIndexLast = 0;
	int queryIndexFirst = 0;
	ArbiterRequest *requestsQuery = malloc(sizeof(*requestsQuery) * size);

	srand(time(0));

	MPI_Barrier(MPI_COMM_WORLD); //pozostalosc po testach, mysle ze mozna to usunac, ale zobaczymy

	pthread_t thread;
	pthread_create(&thread, NULL, childThread, NULL);

	int arbiter_answer_count = 0;

	int message = 0;
	int message_size = 0;
	while (1)
	{
		// MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		// MPI_Get_count(&status, MPI_INT, &message_size);

		recvInt(&message, 1, MPI_ANY_SOURCE, MPI_ANY_TAG, &status);

		if (status.MPI_TAG == ARBITER_REQUEST)
		{
			// printf("request\n");
			int local_clock;
			down(semaphore_clock_id);
			local_clock = *lamport_clock;
			up(semaphore_clock_id);
			// printf("local_clock=%d message=%d\n",local_clock, message);
			if (local_clock > message)
			{
				printf("send answer to %d\n", status.MPI_SOURCE);
				sendInt(&local_clock, 1, status.MPI_SOURCE, ARBITER_ANSWER);
			}
			else
			{
				ArbiterRequest request;
				request.timestamp = message;
				request.rank = status.MPI_SOURCE;
				Append_To_Query(request, requestsQuery, &queryIndexFirst, &queryIndexLast);
			}
		}
		else if(status.MPI_TAG == ARBITER_ANSWER)
		{
			arbiter_answer_count++;
			// printf("answer\n");
			if(arbiter_answer_count >= size - ARBITER_SIZE)
			{
				printf("Start to drink\n");
				sleep(5);
				arbiter_answer_count = 0;
				ArbiterRequest request;
				// printf("first = %d, last = %d\n",queryIndexFirst, queryIndexLast);
				for(int i=queryIndexFirst; i<queryIndexLast; i++)
				{
					request = Pick_From_Query(requestsQuery, &queryIndexFirst, &queryIndexLast);
					sendInt(&rank, 1, request.rank, ARBITER_ANSWER);
				}
			}
		}

		// printf("I received tag %d from %d and my rank is %d\n", status.MPI_TAG, status.MPI_SOURCE, rank);
	}

	semctl(semaphore_clock_id, 0, IPC_RMID);
	printf("MPI finallize\n");
	MPI_Finalize();
	return 0;
}
