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
#define ONE_INT (int)1
#define MESSAGE_SIZE (int)1
#define ARBITER_SIZE (int)1

//tags
#define GROUP_INDEX (int)1
#define ANSWER (int)2
#define TRIGGER (int)3
#define ARBITER_REQUEST (int)4
#define START_DRINKING (int)5

//answers
#define NO (int)0
#define YES (int)1
#define DOES_NOT_MATTER (int)2

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

int my_group_index_id;
int semaphore_my_group_index_id;

int am_i_in_group_id;
int semaphore_am_i_in_group_id;

int all_mates_id;
int semaphore_all_mates_id;

int clock_id;
int semaphore_clock_id;

int my_group_index, am_i_in_group;
int *lamport_clock;

int size, rank, range;

int *my_group;

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

void Send_To_All(int buf, int size, int my_rank, int tag)
{
	for (int i = 0; i < size; i++)
	{
		if (i != my_rank)
		{
			printf("size %d\n", size);
			//sendInt(buf, MESSAGE_SIZE, i, tag);
			MPI_Send(&buf, MESSAGE_SIZE, MPI_INT, i, tag, MPI_COMM_WORLD);
			printf("I sent to %d and my rank is %d\n", i, my_rank);
		}
	}
}
void Show_Mates(int *all_mates, int size, int my_rank)
{
	for (int i = 0; i < size; i++)
	{
		if (all_mates[i] != -1)
		{
			printf("My rank is %d, i am with mate %d\n", my_rank, all_mates[i]);
		}
	}
}

int Get_Mates_Count(int *all_mates, int size)
{
	int count = 0;
	for (int i = 0; i < size; i++)
	{
		if(all_mates[i] == -1)
		{
			return count;
		}
		count++;
	}
	return count;
}

int Check_If_I_Can_Decide(int *all_mates, int size, int my_rank)
{
	int min = all_mates[0];
	for (int i = 1; i < size; i++)
	{
		if (all_mates[i] == -1)
		{
			break;
		}
		if (all_mates[i] < min)
		{
			min = all_mates[i];
		}
	}

	if (min > my_rank)
	{
		return YES;
	}
	return NO;
}

int Get_My_Group_Index()
{
	down(semaphore_my_group_index_id);
	int group_index = my_group_index;
	up(semaphore_my_group_index_id);

	return group_index;
}

int Check_State_Of_Group_Indexes(int *group_indexes, int size)
{
	int is_unique = YES;
	int unique_count = 0;

	for(int i = 0; i < size; i++)
	{
		for(int j = 0; j < size; j++)
		{
			if(i != j && group_indexes[i] == group_indexes[j])
			{
				is_unique = NO;
				break;
			}
		}

		if(is_unique == YES)
		{
			unique_count++;
		}

		if(unique_count == 2)
		{
			return TWO_OR_MORE_UNIQUES;
		}

		is_unique = YES;
	}

	if(unique_count == 0)
	{
		return NULL_UNIQUE;
	}
	else //unique_count == 1
	{
		return ONE_UNIQUE;
	}
}

int *Get_Unique_Ranks(int *group_indexes, int size)
{
	int *unique_ranks = malloc(size * sizeof(int));
	memset(unique_ranks, -1, size * sizeof(int));

	int is_unique = YES;

	int k = 0;

	for(int i = 0; i < size; i++)
	{
		for(int j = 0; j < size; j++)
		{
			if(i != j && group_indexes[i] == group_indexes[j])
			{
				is_unique = NO;
				break;
			}
		}

		if(is_unique == YES)
		{
			unique_ranks[k] = i;
			k++;
		}

		is_unique = YES;
	}


	return unique_ranks;
}

int Get_Minimum_Group_Index(int *group_indexes, int size, int my_group_id)
{
	int min_group_id = size + 10;

	for(int i = 0; i < size; i++)
	{
		if( group_indexes[i] != my_group_id && group_indexes[i] < min_group_id)
		{
			min_group_id = group_indexes[i];
		}
	}

	return min_group_id;
}

int *Remove_Me(int *unique_ranks, int size, int my_rank)
{
	int *my_mates = malloc(size * sizeof(int));
	memset(my_mates, -1, size * sizeof(int));

	int j = 0;
	for(int i = 0; i < size; i++)
	{
		if( unique_ranks[i] != my_rank && unique_ranks[i] != -1)
		{
			my_mates[j] = unique_ranks[i];
			j++;
		}
	}

	return my_mates;       
}

int *Look_For_Mates(int *group_indexes, int size, int my_rank, int my_group_id)
{
	int *my_mates = malloc(size * sizeof(int));
	memset(my_mates, -1, size * sizeof(int));

	int j = -1;

	int *unique_ranks = Get_Unique_Ranks(group_indexes, size);

	int state_of_group_indexes = Check_State_Of_Group_Indexes(group_indexes, size);

	if(state_of_group_indexes == NULL_UNIQUE)
	{
		for(int mate_rank = 0; mate_rank < size; mate_rank++)
		{
			if(mate_rank != my_rank && group_indexes[mate_rank] == my_group_id)
			{
				j++;
				my_mates[j] = mate_rank;
			}
		}
	}
	else if(state_of_group_indexes == ONE_UNIQUE)
	{
		j = -1;

		if(unique_ranks[0] == my_rank)
		{
			int min_group_index = Get_Minimum_Group_Index(group_indexes, size, my_group_id);

			for(int mate_rank = 0; mate_rank < size; mate_rank++)
			{
				if(mate_rank != my_rank && group_indexes[mate_rank] == min_group_index)
				{
					j++;
					my_mates[j] = mate_rank;
				}
			}
		}
		else
		{
			for(int mate_rank = 0; mate_rank < size; mate_rank++)
			{
				if(mate_rank != my_rank && group_indexes[mate_rank] == my_group_id)
				{
					j++;
					my_mates[j] = mate_rank;
				}
			}

			if(Get_Minimum_Group_Index(group_indexes, size, group_indexes[unique_ranks[0]]) == my_group_id)
			{
				my_mates[j + 1] = unique_ranks[0];
			}
		}
	}
	else if(state_of_group_indexes == TWO_OR_MORE_UNIQUES)
	{
		for(int mate_rank = 0; mate_rank < size; mate_rank++)
		{
			if(mate_rank != my_rank && group_indexes[mate_rank] == my_group_id)
			{
				j++;
				my_mates[j] = mate_rank;
			}
		}
		if(my_mates[0] == -1)
		{
			my_mates = Remove_Me(unique_ranks, size, my_rank);
		}
	}

	return my_mates;
}

void *childThread()
{
	printf("Start child! %d\n", rank);

	printf("I send to all and wait for all and my rank is %d\n", rank);

	Send_To_All(my_group_index, size, rank, GROUP_INDEX);


	down(semaphore_am_i_in_group_id);

	while (am_i_in_group != YES)
	{
		up(semaphore_am_i_in_group_id);
		sleep(0.8);
		down(semaphore_am_i_in_group_id);
	}

	up(semaphore_am_i_in_group_id);

	MPI_Barrier(MPI_COMM_WORLD);

	printf("BARRIER my rank is %d\n", rank);
	/*int start_drinking = NO;

	  if(rank == 0)
	  {
	  while (start_drinking != YES)
	  {
	//              printf("I am here and my rank is %d\n", rank);
	start_drinking = rand() % 100;
	//              printf("START DRINKING = %d\n", start_drinking);
	if (start_drinking == YES)
	{
	printf("I DECIDED and my rank is %d\n", rank);
	int group_index = Get_My_Group_Index();
	Send_Trigger_To_Myself(rank, group_index);
	break;
	}

	sleep(0.8);
	}
	}


	if (start_drinking == YES)
	{
	printf("Start drinking\n");
	}
	else
	{
	printf("Waiting for decision and my rank is %d\n", rank);
	}
	*/ // Send_To_All(&rank, 1, rank, ARBITER_REQUEST);

	printf("waiting and my rank is %d\n", rank);
	while (1)
		;
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

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	srand(time(0));

	my_group_index = rand()%size;

	printf("My group index is %d and my rank is %d\n", my_group_index, rank);
	am_i_in_group = NO;
	//	*lamport_clock = 0;

	MPI_Barrier(MPI_COMM_WORLD);

	pthread_t thread;
	pthread_create(&thread, NULL, childThread, NULL);

	int message = -1;

	int answer_count = 1;
	int *all_group_indexes = malloc(size * sizeof(int));
	memset(all_group_indexes, -1, size * sizeof(int));;

	all_group_indexes[rank] = my_group_index;

	while (1)
	{
		//		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		//	MPI_Get_count(&status, MPI_INT, &message_size);

		//		recvInt(message, MESSAGE_SIZE + 1, MPI_ANY_SOURCE, MPI_ANY_TAG, &status);

		MPI_Recv(&message, MESSAGE_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		if(status.MPI_TAG == GROUP_INDEX)
		{
			all_group_indexes[status.MPI_SOURCE] = message;

			answer_count++;

			if(answer_count == size)
			{
				my_group = Look_For_Mates(all_group_indexes, size, rank, my_group_index);

				Show_Mates(my_group, size, rank);

				down(semaphore_am_i_in_group_id);
				am_i_in_group = YES;
				up(semaphore_am_i_in_group_id);
			}
		}
	}

	printf("I remove semaphores\n");
	pthread_join(thread, NULL);

	semctl(semaphore_am_i_in_group_id, 0, IPC_RMID);
	shmctl(am_i_in_group_id, IPC_RMID, NULL);

	semctl(semaphore_my_group_index_id, 0, IPC_RMID);
	shmctl(my_group_index_id, IPC_RMID, NULL);

	semctl(semaphore_clock_id, 0, IPC_RMID);
	shmctl(clock_id, IPC_RMID, NULL);

	printf("MPI finallize\n");
	MPI_Finalize();
	return 0;
}
