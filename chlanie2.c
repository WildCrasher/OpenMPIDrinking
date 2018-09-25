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
#define STRUCTURE_SIZE (int)3
#define ARBITER_SIZE (int)1

//tags
#define WANT_TO_DRINK (int)1
#define ANSWER (int)2
#define TRIGGER (int)3
#define START_DRINKING (int)4
#define GATHER_RANKS (int)5
#define ARRAY (int)6
#define ARBITER_REQUEST (int)7

//answers
#define YES (int)1
#define NO (int)0
#define NO_DRINK (int)2
#define NOT_EQUAL_INDEX (int)3
#define WE_BEGIN_DRINK (int)4

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

int i_want_to_drink_id;
int semaphore_drink_id;

int am_i_in_group_id;
int semaphore_am_i_in_group_id;

int all_mates_id;
int semaphore_all_mates_id;

int clock_id;
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
	int *clock = (int *)shmat(clock_id, NULL, 0);
	(*clock)++;
	printf("clock = %d\n", *clock);
	memcpy(buf + size, clock, sizeof(int));
	shmdt(clock);
	up(semaphore_clock_id);
	return MPI_Send(buf, size + 1, MPI_INT, destination, tag, MPI_COMM_WORLD);
}

int recvInt(int *data, int size, int source, int tag, MPI_Status *status)
{
	int ret = MPI_Recv(data, size + 1, MPI_INT, source, tag, MPI_COMM_WORLD, status);
	down(semaphore_clock_id);
	int *clock = (int *)shmat(clock_id, NULL, 0);
	*clock = max(*clock, data[size]) + 1;
	printf("clock = %d\n", *clock);
	shmdt(clock);
	up(semaphore_clock_id);
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

void Send_Trigger_To_Myself(int my_rank)
{
	int group_index_answer_rank[STRUCTURE_SIZE];
	group_index_answer_rank[0] = -1;
	group_index_answer_rank[1] = YES;
	group_index_answer_rank[2] = my_rank;

	// MPI_Send(group_index_answer_rank, STRUCTURE_SIZE, MPI_INT, my_rank, TRIGGER, MPI_COMM_WORLD);
	sendInt(group_index_answer_rank, STRUCTURE_SIZE, my_rank, TRIGGER);
	printf("Trigger sent, my rank is %d\n", my_rank);
}

void Send_To_All_Start_Drinking(int *all_mates, int size, int my_rank)
{
	int group_index_answer_rank[STRUCTURE_SIZE];
	group_index_answer_rank[0] = -1;
	group_index_answer_rank[1] = YES;
	group_index_answer_rank[2] = my_rank;

	for (int i = 0; i < size; i++)
	{
		if (all_mates[i] != -1)
		{
			MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, my_rank, START_DRINKING, MPI_COMM_WORLD);
		}
	}
}

void Send_To_All_My_Ranks(int *all_mates, int size, int my_rank)
{
	int group_index_answer_rank[STRUCTURE_SIZE];
	group_index_answer_rank[0] = -1;
	group_index_answer_rank[1] = YES;
	group_index_answer_rank[2] = my_rank;

	for (int i = 0; i < size; i++)
	{
		int other_rank = all_mates[i];
		if (all_mates[i] != -1)
		{
			sendInt(group_index_answer_rank, THREE_INT, my_rank, GATHER_RANKS);
			sendInt(all_mates, size, other_rank, tag);
		}

		// printf("I sent to %d and my rank is %d\n", i, my_rank);
	}
}

int *Leave_Only_Common_Ranks(int *original_mates, int *received_mates, int size)
{

	return original_mates;
}

void Add_Mate_To_Group(int mate_rank, int *all_mates, int size)
{
	for (int i = 0; i < size; i++)
	{
		if (all_mates[i] == -1)
		{
			all_mates[i] = mate_rank;
			break;
		}
		else if (all_mates[i] == mate_rank)
		{
			break;
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

// int Check_If_I_Can_Drink(struct package *packages, int size)
// {
// 	int group_index = -1;
// 	for (int i = 0; i < size - 1; i++)
// 	{
// 		if (packages[i].answer == YES)
// 		{
// 			group_index = packages[i].group_index;
// 			break;
// 		}
// 	}

// 	if (group_index == -1)
// 	{
// 		return -1;
// 	}

// 	for (int i = 0; i < size - 1; i++)
// 	{
// 		if (packages[i].group_index == group_index && packages[i].answer == NO)
// 		{
// 			return -2;
// 		}
// 	}

// 	return group_index;
// }

// void Get_All_Ranks_From_Group(struct package *packages, int size, int group_index, int *all_ranks)
// {
// 	int index = 0;
// 	for (int i = 0; i < size; i++)
// 	{
// 		if (packages[i].group_index == group_index)
// 		{
// 			all_ranks[index] = packages[i].rank;
// 			index++;
// 		}
// 	}
// }

int Get_My_Group_Index()
{
	int *my_group_index;

	down(semaphore_my_group_index_id);
	my_group_index = (int *)shmat(my_group_index_id, NULL, 0);
	int group_index = *my_group_index;
	shmdt(my_group_index);
	up(semaphore_my_group_index_id);

	return group_index;
}

void *childThread()
{
	int *i_want_to_drink;
	int *am_i_in_group;
	int *all_mates;

	printf("start losu losu\n");
	down(semaphore_drink_id);
	i_want_to_drink = (int *)shmat(i_want_to_drink_id, NULL, 0);
	// perror("i want to drink error");

	printf("%d\n", *i_want_to_drink);

	while (*i_want_to_drink != YES)
	{
		*i_want_to_drink = rand() % range;
		shmdt(i_want_to_drink);
		up(semaphore_drink_id);
		sleep(0.8);
		down(semaphore_drink_id);
		i_want_to_drink = (int *)shmat(i_want_to_drink_id, NULL, 0);
		// printf("%d\n", *i_want_to_drink);
		// perror("i want to drink error2");
	}

	shmdt(i_want_to_drink);
	up(semaphore_drink_id);

	printf("Chce pic! %d\n", rank);

	printf("I send to all and wait for all and my rank is %d\n", rank);

	int group_index = Get_My_Group_Index();

	int group_index_answer_rank[STRUCTURE_SIZE];
	group_index_answer_rank[0] = group_index;
	group_index_answer_rank[1] = YES;
	group_index_answer_rank[2] = rank;
	Send_To_All(group_index_answer_rank, size, rank, WANT_TO_DRINK);

	down(semaphore_am_i_in_group_id);
	am_i_in_group = (int *)shmat(am_i_in_group_id, NULL, 0);

	while (*am_i_in_group != YES)
	{
		shmdt(am_i_in_group);
		up(semaphore_am_i_in_group_id);
		sleep(0.8);
		down(semaphore_am_i_in_group_id);
		am_i_in_group = (int *)shmat(am_i_in_group_id, NULL, 0);
	}

	shmdt(am_i_in_group);
	up(semaphore_am_i_in_group_id);

	// perror("am_i_in_group_error\n");
	down(semaphore_all_mates_id);
	all_mates = (int *)shmat(all_mates_id, NULL, 0);
	int i_can_decide = Check_If_I_Can_Decide(all_mates, size, rank);
	printf("can_decide = %d and my rank is %d\n", i_can_decide, rank);
	int start_drinking = NO;
	while (i_can_decide == YES && start_drinking != YES)
	{
		printf("I am here and my rank is %d\n", rank);
		start_drinking = rand() % 100;
		printf("START DRINKING = %d\n", start_drinking);
		if (start_drinking == YES)
		{
			printf("I DECIDED and my rank is %d\n", rank);
			Send_Trigger_To_Myself(rank);
			break;
		}

		shmdt(all_mates);
		up(semaphore_all_mates_id);
		sleep(0.8);

		down(semaphore_all_mates_id);
		all_mates = (int *)shmat(all_mates_id, NULL, 0);
		i_can_decide = Check_If_I_Can_Decide(all_mates, size, rank);
	}

	shmdt(all_mates);
	up(semaphore_all_mates_id);

	if (start_drinking == YES)
	{
		printf("Start drinking\n");
	}
	else
	{
		printf("Waiting for decision and my rank is %d\n", rank);
	}
	// Send_To_All(&rank, 1, rank, ARBITER_REQUEST);
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

	int *my_group_index;
	my_group_index_id = shmget(IPC_PRIVATE, sizeof(int), 0666 | IPC_CREAT);
	my_group_index = (int *)shmat(my_group_index_id, NULL, 0);
	*my_group_index = 1;
	shmdt(my_group_index);

	clock_id = shmget(IPC_PRIVATE, sizeof(int), 0666 | IPC_CREAT);

	semaphore_clock_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_clock_id, 0, SETVAL, (int)1);

	int *clock;

	clock = (int *)shmat(clock_id, NULL, 0);
	*clock = 0;
	shmdt(clock);

	i_want_to_drink_id = shmget(IPC_PRIVATE, sizeof(int), 0777 | IPC_CREAT);
	// perror("i_want_to_drink_id error");
	// printf("%d\n", i_want_to_drink_id);

	semaphore_drink_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_drink_id, 0, SETVAL, (int)1);

	int *i_want_to_drink;
	i_want_to_drink_id = shmget(IPC_PRIVATE, sizeof(int), 0777 | IPC_CREAT);
	i_want_to_drink = (int *)shmat(i_want_to_drink_id, NULL, 0);
	*i_want_to_drink = NO;
	shmdt(i_want_to_drink);

	semaphore_am_i_in_group_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_am_i_in_group_id, 0, SETVAL, (int)1);

	int *am_i_in_group;
	am_i_in_group_id = shmget(IPC_PRIVATE, sizeof(int), 0777 | IPC_CREAT);
	am_i_in_group = (int *)shmat(am_i_in_group_id, NULL, 0);
	*am_i_in_group = NO;
	shmdt(am_i_in_group);

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	semaphore_all_mates_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_all_mates_id, 0, SETVAL, (int)1);

	int *all_mates;
	all_mates_id = shmget(IPC_PRIVATE, size * sizeof(int), 0777 | IPC_CREAT);
	perror("shmget");
	all_mates = (int *)shmat(all_mates_id, NULL, 0);
	memset(all_mates, -1, sizeof(int) * size);
	shmdt(all_mates);

	srand(time(0));

	MPI_Barrier(MPI_COMM_WORLD); //pozostalosc po testach, mysle ze mozna to usunac, ale zobaczymy

	pthread_t thread;
	pthread_create(&thread, NULL, childThread, NULL);
	int group_index_answer_rank[STRUCTURE_SIZE], answer_count = 0, answer_count_gather = 0;
	int invalid = FALSE;
	int start_drinking = NO;
	int *all_mates_temp;
	all_mates_temp = malloc(size * sizeof(int));
	memset(all_mates_temp, -1, sizeof(int) * size);

	while (1)
	{
		recvInt(group_index_answer_rank, STRUCTURE_SIZE, MPI_ANY_SOURCE, MPI_ANY_TAG, &status);

		printf("I received tag %d from %d and my rank is %d\n", status.MPI_TAG, status.MPI_SOURCE, rank);
		if (status.MPI_TAG == WANT_TO_DRINK)
		{
			down(semaphore_drink_id);
			i_want_to_drink = (int *)shmat(i_want_to_drink_id, NULL, 0);

			down(semaphore_my_group_index_id);
			my_group_index = (int *)shmat(my_group_index_id, NULL, 0);

			if (*i_want_to_drink == YES)
			{
				shmdt(i_want_to_drink);
				up(semaphore_drink_id);

				group_index_answer_rank[0] = *my_group_index;
				group_index_answer_rank[2] = rank;

				if (start_drinking == NO)
				{
					if (group_index_answer_rank[0] == *my_group_index)
					{
						//	down(semaphore_am_i_in_group_id);
						//	am_i_in_group = (int *)shmat(am_i_in_group_id, NULL, 0);
						/*				down(semaphore_all_mates_id);
							all_mates = (int *)shmat(all_mates_id, NULL, 0);
							Add_Mate_To_Group(status.MPI_SOURCE, all_mates, size);
							Show_Mates(all_mates, size, rank);
							shmdt(all_mates);
							up(semaphore_all_mates_id);*/
						//	shmdt(am_i_in_group);
						//	up(semaphore_am_i_in_group_id);

						group_index_answer_rank[1] = YES;
						printf("I answer YES to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
						// MPI_Send(group_index_answer_rank, STRUCTURE_SIZE, MPI_INT, status.MPI_SOURCE, ANSWER, MPI_COMM_WORLD);
						sendInt(group_index_answer_rank, STRUCTURE_SIZE, status.MPI_SOURCE, ANSWER);
					}
					else
					{
						group_index_answer_rank[1] = NOT_EQUAL_INDEX;
						printf("I answer NO2 to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
						sendInt(group_index_answer_rank, STRUCTURE_SIZE, status.MPI_SOURCE, ANSWER);
					}
				}
				else
				{
					group_index_answer_rank[1] = WE_BEGIN_DRINK;
					printf("I answer NO3 to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
					sendInt(group_index_answer_rank, STRUCTURE_SIZE, status.MPI_SOURCE, ANSWER);
				}
			}
			else
			{
				shmdt(i_want_to_drink);
				up(semaphore_drink_id);

				if (*my_group_index < group_index_answer_rank[0])
				{
					*my_group_index = group_index_answer_rank[0];
				}
				group_index_answer_rank[0] = *my_group_index;
				group_index_answer_rank[1] = NO_DRINK;
				group_index_answer_rank[2] = rank;
				printf("I answer NO to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
				sendInt(group_index_answer_rank, STRUCTURE_SIZE, status.MPI_SOURCE, ANSWER);
			}
			shmdt(my_group_index);
			up(semaphore_my_group_index_id);
		}
		else if (status.MPI_TAG == ANSWER)
		{
			down(semaphore_my_group_index_id);
			my_group_index = (int *)shmat(my_group_index_id, NULL, 0);

			down(semaphore_all_mates_id);
			all_mates = (int *)shmat(all_mates_id, NULL, 0);
			answer_count++;

			if (group_index_answer_rank[1] == YES && invalid == FALSE)
			{
				Add_Mate_To_Group(status.MPI_SOURCE, all_mates, size);
			}
			else if (group_index_answer_rank[1] != YES && invalid == FALSE)
			{
				if (group_index_answer_rank[1] == NOT_EQUAL_INDEX)
				{
					if (*my_group_index < group_index_answer_rank[0])
						*my_group_index = group_index_answer_rank[0];
					invalid = TRUE;
					memset(all_mates, -1, sizeof(int) * size);
				}
				else if (group_index_answer_rank[1] == WE_BEGIN_DRINK)
				{
					*my_group_index = group_index_answer_rank[0] + 1;
					invalid = TRUE;
					memset(all_mates, -1, sizeof(int) * size);
				}
			}

			if (answer_count == size - 1)
			{
				if (invalid == FALSE)
				{
					printf("All answers came:\n");
					Show_Mates(all_mates, size, rank);

					down(semaphore_am_i_in_group_id);
					am_i_in_group = (int *)shmat(am_i_in_group_id, NULL, 0);
					*am_i_in_group = YES;
					shmdt(am_i_in_group);
					up(semaphore_am_i_in_group_id);
				}
				answer_count = 0;
				invalid = FALSE;
			}

			shmdt(my_group_index);
			up(semaphore_my_group_index_id);
			shmdt(all_mates);
			up(semaphore_all_mates_id);
		}
		else if (status.MPI_TAG == TRIGGER)
		{
			start_drinking = YES;
			down(semaphore_all_mates_id);
			all_mates = (int *)shmat(all_mates_id, NULL, 0);

			Send_To_All_Start_Drinking(all_mates, size, rank);
			Send_To_All_My_Ranks(all_mates, size, rank);

			shmdt(all_mates);
			up(semaphore_all_mates_id);
		}
		else if (status.MPI_TAG == START_DRINKING)
		{
			start_drinking = YES;

			down(semaphore_all_mates_id);
			all_mates = (int *)shmat(all_mates_id, NULL, 0);

			Send_To_All_My_Ranks(all_mates, size, rank);

			shmdt(all_mates);
			up(semaphore_all_mates_id);
		}
		else if (status.MPI_TAG == GATHER_RANKS)
		{
			start_drinking = YES;
			answer_count_gather++;

			MPI_Recv(all_mates_temp, size, MPI_INT, MPI_ANY_SOURCE, ARRAY, MPI_COMM_WORLD, &status);

			down(semaphore_all_mates_id);
			all_mates = (int *)shmat(all_mates_id, NULL, 0);

			all_mates = Leave_Only_Common_Ranks(all_mates, all_mates_temp, size);

			shmdt(all_mates);
			up(semaphore_all_mates_id);
		}
	}
	// printf("WAIT\n");
	// wait(0);
	pthread_join(thread, NULL);
	semctl(semaphore_drink_id, 0, IPC_RMID);
	shmctl(i_want_to_drink_id, IPC_RMID, NULL);

	semctl(semaphore_am_i_in_group_id, 0, IPC_RMID);
	shmctl(am_i_in_group_id, IPC_RMID, NULL);

	semctl(semaphore_my_group_index_id, 0, IPC_RMID);
	shmctl(my_group_index_id, IPC_RMID, NULL);

	semctl(semaphore_all_mates_id, 0, IPC_RMID);
	shmctl(all_mates_id, IPC_RMID, NULL);

	printf("MPI finallize\n");
	MPI_Finalize();
	return 0;
}
