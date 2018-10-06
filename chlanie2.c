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
#define MESSAGE_SIZE (int)1
#define ARBITER_SIZE (int)1

//tags
#define GROUP_INDEX (int)1
#define ANSWER (int)2
#define ARBITER_REQUEST (int)3
#define START_DRINKING (int)4
#define ARBITER_ANSWER (int)5

//answers
#define NO (int)0
#define YES (int)1

//states of group_indexes
#define NULL_UNIQUE (int)0
#define ONE_UNIQUE (int)1
#define TWO_OR_MORE_UNIQUES (int)2

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

int my_group_index_id;
int semaphore_my_group_index_id;

int am_i_in_group_id;
int semaphore_am_i_in_group_id;

int clock_id;
int semaphore_clock_id;

int start_drinking;
int semaphore_start_drinking_id;

int my_group_index, am_i_in_group;
int *lamport_clock;
int *my_group;

int am_i_master;

int timestamp;

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
	//	printf("clock = %d\n", *clock);
	memcpy(buf + size, lamport_clock, sizeof(int));
	up(semaphore_clock_id);
	int ret = MPI_Send(buf, size + 1, MPI_INT, destination, tag, MPI_COMM_WORLD);
	free(buf);
	return ret;
}

int recvInt(int *data, int size, int source, int tag, MPI_Status *status)
{
	int *buf;
	buf = malloc((size + 1) * sizeof(int));
	int ret = MPI_Recv(buf, size + 1, MPI_INT, source, tag, MPI_COMM_WORLD, status);

	down(semaphore_clock_id);
	*lamport_clock = max(*lamport_clock, buf[size]) + 1;
	printf("rank = %d clock = %d\n",rank, *lamport_clock);
	up(semaphore_clock_id);
	memcpy(data, buf, size * sizeof(int));
	free(buf);
	return ret;
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

void Send_To_All(int buf, int size, int my_rank, int tag)
{
	for (int i = 0; i < size; i++)
	{
		if (i != my_rank)
		{
			sendInt(&buf, MESSAGE_SIZE, i, tag);
			// printf("I sent to %d and my rank is %d\n", i, my_rank);
		}
	}
}

void Send_Start_Drinking()
{
	int my_rank = rank;

	for (int i = 0; i < size; i++)
	{
		if(my_group[i] == -1)
		{
			break;
		}

		sendInt(&my_group_index, MESSAGE_SIZE, my_group[i], START_DRINKING);
		printf("I sent to %d and my rank is %d\n", my_group[i], my_rank);
	}

}

void Show_Mates(int *my_mates)
{
	int my_rank = rank;

	for (int i = 0; i < size; i++)
	{
		if (my_mates[i] != -1)
		{
			printf("My rank is %d, i am with mate %d\n", my_rank, my_mates[i]);
		}
	}
}

int Check_State_Of_Group_Indexes(int *group_indexes)
{
	int is_unique = YES;
	int unique_count = 0;

	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			if (i != j && group_indexes[i] == group_indexes[j])
			{
				is_unique = NO;
				break;
			}
		}

		if (is_unique == YES)
		{
			unique_count++;
		}

		if (unique_count == 2)
		{
			return TWO_OR_MORE_UNIQUES;
		}

		is_unique = YES;
	}

	if (unique_count == 0)
	{
		return NULL_UNIQUE;
	}
	else //unique_count == 1
	{
		return ONE_UNIQUE;
	}
}

int *Get_Unique_Ranks(int *group_indexes)
{
	int *unique_ranks = malloc(size * sizeof(int));
	memset(unique_ranks, -1, size * sizeof(int));

	int is_unique = YES;

	int k = 0;

	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			if (i != j && group_indexes[i] == group_indexes[j])
			{
				is_unique = NO;
				break;
			}
		}

		if (is_unique == YES)
		{
			unique_ranks[k] = i;
			k++;
		}

		is_unique = YES;
	}

	return unique_ranks;
}

int Get_Minimum_Group_Index(int *group_indexes, int my_group_id)
{
	int min_group_id = size + 10;

	for (int i = 0; i < size; i++)
	{
		if (group_indexes[i] != my_group_id && group_indexes[i] < min_group_id)
		{
			min_group_id = group_indexes[i];
		}
	}

	return min_group_id;
}

int *Remove_Me(int *unique_ranks)
{
	int *my_mates = malloc(size * sizeof(int));
	memset(my_mates, -1, size * sizeof(int));

	int my_rank = rank;
	int j = 0;

	for (int i = 0; i < size; i++)
	{
		if (unique_ranks[i] != my_rank && unique_ranks[i] != -1)
		{
			my_mates[j] = unique_ranks[i];
			j++;
		}
	}

	//	free(my_mates);

	return my_mates;
}

int *Look_For_Mates(int *group_indexes, int my_group_id)
{
	int *my_mates = malloc(size * sizeof(int));
	memset(my_mates, -1, size * sizeof(int));

	int my_rank = rank;

	int j = -1;

	int *unique_ranks = Get_Unique_Ranks(group_indexes);

	int state_of_group_indexes = Check_State_Of_Group_Indexes(group_indexes);

	if (state_of_group_indexes == NULL_UNIQUE)
	{
		for (int mate_rank = 0; mate_rank < size; mate_rank++)
		{
			if (mate_rank != my_rank && group_indexes[mate_rank] == my_group_id)
			{
				j++;
				my_mates[j] = mate_rank;
			}
		}
	}
	else if (state_of_group_indexes == ONE_UNIQUE)
	{
		j = -1;

		if (unique_ranks[0] == my_rank)
		{
			int min_group_index = Get_Minimum_Group_Index(group_indexes, my_group_id);

			for (int mate_rank = 0; mate_rank < size; mate_rank++)
			{
				if (mate_rank != my_rank && group_indexes[mate_rank] == min_group_index)
				{
					j++;
					my_mates[j] = mate_rank;
				}
			}
		}
		else
		{
			for (int mate_rank = 0; mate_rank < size; mate_rank++)
			{
				if (mate_rank != my_rank && group_indexes[mate_rank] == my_group_id)
				{
					j++;
					my_mates[j] = mate_rank;
				}
			}

			if (Get_Minimum_Group_Index(group_indexes, group_indexes[unique_ranks[0]]) == my_group_id)
			{
				my_mates[j + 1] = unique_ranks[0];
			}
		}
	}
	else if (state_of_group_indexes == TWO_OR_MORE_UNIQUES)
	{
		for (int mate_rank = 0; mate_rank < size; mate_rank++)
		{
			if (mate_rank != my_rank && group_indexes[mate_rank] == my_group_id)
			{
				j++;
				my_mates[j] = mate_rank;
			}
		}
		if (my_mates[0] == -1)
		{
			my_mates = Remove_Me(unique_ranks);
		}
	}

	//free(unique_ranks);

	return my_mates;
}

int Check_If_Am_I_Master()
{
	int min = my_group[0];
	for (int i = 1; i < size; i++)
	{
		if (my_group[i] == -1)
		{
			break;
		}
		if (my_group[i] < min)
		{
			min = my_group[i];
		}
	}

	if (min > rank)
	{
		return YES;
	}
	return NO;
}

void *childThread()
{
	// printf("Start child! %d\n", rank);

	// printf("I send to all and wait for all and my rank is %d\n", rank);

	down(semaphore_my_group_index_id);
	int group_index = my_group_index;
	up(semaphore_my_group_index_id);

	Send_To_All(group_index, size, rank, GROUP_INDEX);

	down(semaphore_am_i_in_group_id);

	while (am_i_in_group != YES)
	{
		up(semaphore_am_i_in_group_id);
		sleep(0.8);
		down(semaphore_am_i_in_group_id);
	}

	up(semaphore_am_i_in_group_id);

	am_i_master = Check_If_Am_I_Master();

	down(semaphore_clock_id);
	timestamp = *lamport_clock;
	up(semaphore_clock_id);

	if (am_i_master == YES)
	{
		printf("Request arbiter\n");
		Send_To_All(timestamp, size, rank, ARBITER_REQUEST);
	}

	down(semaphore_start_drinking_id);

	while (start_drinking != YES)
	{
		up(semaphore_start_drinking_id);
		sleep(0.8);
		down(semaphore_start_drinking_id);
	}

	up(semaphore_start_drinking_id);

	printf("Start drinking and my group index is %d and my rank is%d\n", my_group_index, rank);

	sleep(5);

	printf("End of drinking and my group index is %d and my rank is %d\n", my_group_index, rank);

	return NULL;
}

int main(int argc, char **argv)
{
	int provided;
	int ret = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	assert(ret == 0 && provided == MPI_THREAD_MULTIPLE);

	range = 100;

	semaphore_my_group_index_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_my_group_index_id, 0, SETVAL, (int)1);

	semaphore_clock_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_clock_id, 0, SETVAL, (int)1);

	semaphore_am_i_in_group_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_am_i_in_group_id, 0, SETVAL, (int)1);

	semaphore_start_drinking_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_start_drinking_id, 0, SETVAL, (int)1);

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if(size < 2)
	{
		printf("Create more competitors\n");

		semctl(semaphore_am_i_in_group_id, 0, IPC_RMID);
		semctl(semaphore_my_group_index_id, 0, IPC_RMID);
		semctl(semaphore_clock_id, 0, IPC_RMID);
		semctl(semaphore_start_drinking_id, 0, IPC_RMID);

		MPI_Finalize();
		return 0;
	}
	else
	{
		srand(time(0) + rank);
		my_group_index = rand() % size;

		printf("My group index is %d and my rank is %d\n", my_group_index, rank);

		am_i_in_group = NO;
		lamport_clock = malloc(sizeof(int));
		*lamport_clock = 0;
		am_i_master = NO;
		start_drinking = NO;

		MPI_Barrier(MPI_COMM_WORLD);

		pthread_t thread;
		pthread_create(&thread, NULL, childThread, NULL);

		int message = -1;

		int answer_count = 1;
		int *all_group_indexes = malloc(size * sizeof(int));
		memset(all_group_indexes, -1, size * sizeof(int));

		down(semaphore_my_group_index_id);
		all_group_indexes[rank] = my_group_index;
		up(semaphore_my_group_index_id);

		int queryIndexLast = 0;
		int queryIndexFirst = 0;
		ArbiterRequest *requestsQuery = malloc(sizeof(*requestsQuery) * size);
		int arbiter_answer_count = 0;



		while (1)
		{
			recvInt(&message, MESSAGE_SIZE, MPI_ANY_SOURCE, MPI_ANY_TAG, &status);

			if (status.MPI_TAG == GROUP_INDEX)
			{
				all_group_indexes[status.MPI_SOURCE] = message;

				answer_count++;

				if (answer_count == size)
				{
					my_group = Look_For_Mates(all_group_indexes, all_group_indexes[rank]);

					Show_Mates(my_group);

					down(semaphore_am_i_in_group_id);
					am_i_in_group = YES;
					up(semaphore_am_i_in_group_id);

					//free(all_group_indexes);
				}
			}
			else if (status.MPI_TAG == ARBITER_REQUEST)
			{
				// down(semaphore_clock_id);
				// timestamp = *lamport_clock;
				// up(semaphore_clock_id);
				// printf("request\n");
				printf("timestamp=%d message=%d\n", timestamp, message);
				if (!am_i_master || timestamp > message)
				{
					printf("send arbiter answer to %d\n", status.MPI_SOURCE);
					sendInt(&timestamp, 1, status.MPI_SOURCE, ARBITER_ANSWER);
				}
				else if (timestamp == message && rank < status.MPI_SOURCE)
				{
					printf("send arbiter answer to %d\n", status.MPI_SOURCE);
					sendInt(&timestamp, 1, status.MPI_SOURCE, ARBITER_ANSWER);
				}
				else
				{
					ArbiterRequest request;
					request.timestamp = message;
					request.rank = status.MPI_SOURCE;
					Append_To_Query(request, requestsQuery, &queryIndexFirst, &queryIndexLast);
				}
			}
			else if (status.MPI_TAG == ARBITER_ANSWER)
			{
				arbiter_answer_count++;
				// printf("answer\n");
				if (am_i_master && arbiter_answer_count >= size - ARBITER_SIZE)
				{
					down(semaphore_start_drinking_id);
					start_drinking = YES;
					up(semaphore_start_drinking_id);

					Send_Start_Drinking();

					sleep(5);
					arbiter_answer_count = 0;
					am_i_master = NO;
					ArbiterRequest request;
					printf("first = %d, last = %d\n", queryIndexFirst, queryIndexLast);
					for (int i = queryIndexFirst; i < queryIndexLast; i++)
					{
						request = Pick_From_Query(requestsQuery, &queryIndexFirst, &queryIndexLast);
						sendInt(&rank, 1, request.rank, ARBITER_ANSWER);
					}
				}
			}
			else if(status.MPI_TAG == START_DRINKING)
			{
				down(semaphore_start_drinking_id);
				start_drinking = YES;
				up(semaphore_start_drinking_id);
			}
		}

		printf("I remove semaphores\n");
		pthread_join(thread, NULL);

		semctl(semaphore_am_i_in_group_id, 0, IPC_RMID);

		semctl(semaphore_my_group_index_id, 0, IPC_RMID);

		semctl(semaphore_clock_id, 0, IPC_RMID);

		semctl(semaphore_start_drinking_id, 0, IPC_RMID);
		//	free(my_group);
		printf("MPI finallize\n");
		MPI_Finalize();
		return 0;
	}
}
