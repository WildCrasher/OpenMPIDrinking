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
#define ONE_INT (int)1
#define SEMCOUNT (int)1
#define FALSE (int)0
#define TRUE (int)1
#define MESSAGE_SIZE (int)1
#define ARBITER_SIZE (int)1

//tags
#define WANT_TO_DRINK (int)1
#define ANSWER (int)2
#define ARBITER_REQUEST (int)3
#define START_DRINKING (int)4
#define ARBITER_ANSWER (int)5
#define I_HAVE_YOU_REQUEST (int)6
#define I_HAVE_YOU_ANSWER (int)7
#define END_DRINKING (int)8
#define IS_CORRECT (int)9

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

int end_of_gather;
int semaphore_end_of_gather_id;

int i_want_to_drink;
int am_i_in_group;
int *lamport_clock;
int *all_mates;

int master_rank;

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
//	printf("TAG %d and destination %d\n", tag, destination);
	int ret = MPI_Send(buf, size + 1, MPI_INT, destination, tag, MPI_COMM_WORLD);
	free(buf);
	//printf("tag = %d\n", tag);
	return ret;
}

int recvInt(int *data, int size, int source, int tag, MPI_Status *status)
{
	int *buf;
	buf = malloc((size + 1) * sizeof(int));
	int ret = MPI_Recv(buf, size + 1, MPI_INT, source, tag, MPI_COMM_WORLD, status);

	down(semaphore_clock_id);
	*lamport_clock = max(*lamport_clock, buf[size]) + 1;
	// printf("rank = %d clock = %d\n", rank, *lamport_clock);
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

void Send_To_All(int buf, int tag)
{
	int my_rank = rank;
	for (int i = 0; i < size; i++)
	{
		if (i != my_rank)
		{
			sendInt(&buf, MESSAGE_SIZE, i, tag);
			// printf("I sent to %d and my rank is %d\n", i, my_rank);
		}
	}
}

void Send_End_Drinking()
{
	int my_rank = rank;
	int message = YES;

	sendInt(&message, MESSAGE_SIZE, master_rank, END_DRINKING);
	printf("I sent to %d and my rank is %d\n", master_rank, my_rank);
}

void Send_To_Ranks(int buf, int tag, int ranks[])
{
	int my_rank = rank;
	for (int i = 0; i < size; i++)
	{
		if (ranks[i] != -1 && ranks[i] != my_rank)
		{
			sendInt(&buf, MESSAGE_SIZE, ranks[i], tag);
		}
	}
}

void Send_Start_Drinking()
{
	int my_rank = rank;

	for (int i = 0; i < size; i++)
	{
		if (all_mates[i] == -1)
		{
			break;
		}

		int message = YES;
		sendInt(&message, MESSAGE_SIZE, all_mates[i], START_DRINKING);
		printf("I sent to %d and my rank is %d\n", all_mates[i], my_rank);
	}
}

void Show_Mates(int *my_mates, char* parametr)
{
	int my_rank = rank;

	for (int i = 0; i < size; i++)
	{
		if (my_mates[i] != -1)
		{
			printf("My rank is %d, i am with mate %d %s\n", my_rank, my_mates[i], parametr);
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

int Check_If_I_Have_You(int myTab[], int mate_rank)
{
	for (int i = 0; i < size; i++)
	{
		if (myTab[i] == mate_rank)
			return YES;
	}
	return NO;
}

int Find_Master()
{
	int max = all_mates[0];
	for (int i = 1; i < size; i++)
	{
		if (all_mates[i] == -1)
		{
			continue;
		}
		if (all_mates[i] > max)
		{
			max = all_mates[i];
		}
	}

	if (max < rank)
	{
		return rank;
	}

	return max;
}

void Add_Mate_To_Group(int mate_rank, int *all_mates)
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

int *Sum_Arrays(int *first_array, int *second_array)
{
	int *result_array = malloc(size * sizeof(int));
	memset(result_array, -1, size * sizeof(int));

	for (int i = 0; i < size; i++)
	{
		Add_Mate_To_Group(first_array[i], result_array);
		Add_Mate_To_Group(second_array[i], result_array);
	}

	return result_array;
}

int Compare_Arrays(int *first_array, int *second_array)
{
	int found = NO;
	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			if (first_array[i] == second_array[j])
			{
				found = YES;
				break;
			}
		}

		if (found == NO)
		{
			return NO;
		}
		found = NO;
	}
	return YES;
}

int Get_Mates_Count(int *all_mates)
{
	int count = 0;
	for (int i = 0; i < size; i++)
	{
		if (all_mates[i] == -1)
		{
			return count;
		}
		count++;
	}
	return count;
}

void *childThread()
{
	sleep(rank);
	while (1)
	{
		i_want_to_drink = YES;
		Send_To_All(YES, WANT_TO_DRINK);

		down(semaphore_end_of_gather_id);

		while (end_of_gather != YES)
		{
			up(semaphore_end_of_gather_id);
			sleep(0.8);
			down(semaphore_end_of_gather_id);
		}

		end_of_gather = NO;
		up(semaphore_end_of_gather_id);

		down(semaphore_am_i_in_group_id);

		if (am_i_in_group == NO)
		{
			up(semaphore_am_i_in_group_id);
			memset(all_mates, -1, size * sizeof(int));
			continue;
		}

		up(semaphore_am_i_in_group_id);

		master_rank = Find_Master();

		down(semaphore_clock_id);
		timestamp = *lamport_clock;
		up(semaphore_clock_id);

		if (master_rank == rank)
		{
			printf("Request arbiter\n");
			Send_To_All(timestamp, ARBITER_REQUEST);
		}

		down(semaphore_start_drinking_id);

		while (start_drinking != YES)
		{
			up(semaphore_start_drinking_id);
			sleep(0.8);
			down(semaphore_start_drinking_id);
		}

		up(semaphore_start_drinking_id);

		printf("Start drinking and my rank is%d\n", rank);

		sleep(5);

		if (master_rank != rank)
		{
			Send_End_Drinking();
		}

		master_rank = -1;
		memset(all_mates, -1, size * sizeof(int));

		down(semaphore_am_i_in_group_id);
		am_i_in_group = NO;
		up(semaphore_am_i_in_group_id);

		printf("End of drinking and my rank is %d\n", rank);
	}
	return NULL;
}

void Show_All(int *tab, char *parametr)
{
	for (int i = 0; i < size; i++)
        {
		printf("ALLLL my rank is %d and this is tab %s: %d  ", rank, parametr, tab[i]);
	}
	printf("\n");
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

	semaphore_end_of_gather_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_end_of_gather_id, 0, SETVAL, (int)1);

	// semaphore_iteration_count_id = semget(IPC_PRIVATE, SEMCOUNT, 0666 | IPC_CREAT);
	// semctl(semaphore_iteration_count_id, 0, SETVAL, (int)1);

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	// iteration_count = 0;

	if (size < 2)
	{
		printf("Create more competitors\n");

		semctl(semaphore_am_i_in_group_id, 0, IPC_RMID);
		semctl(semaphore_my_group_index_id, 0, IPC_RMID);
		semctl(semaphore_clock_id, 0, IPC_RMID);
		semctl(semaphore_start_drinking_id, 0, IPC_RMID);
		semctl(semaphore_end_of_gather_id, 0, IPC_RMID);

		MPI_Finalize();
		return 0;
	}
	else
	{
		srand(time(0) + rank);

		printf("My rank is %d\n", rank);

		am_i_in_group = NO;
		lamport_clock = malloc(sizeof(int));
		*lamport_clock = 0;
		master_rank = -1;
		start_drinking = NO;
		end_of_gather = NO;

		pthread_t thread;
		pthread_create(&thread, NULL, childThread, NULL);

		int message = -1;

		int answer_count = 1;
		all_mates = malloc(size * sizeof(int));
		memset(all_mates, -1, size * sizeof(int));

		i_want_to_drink = NO;
		int queryIndexLast = 0;
		int queryIndexFirst = 0;
		ArbiterRequest *requestsQuery = malloc(sizeof(*requestsQuery) * size);
		int arbiter_answer_count = 0;

		int have_you_answer_count = 1;
		int *have_me_tab = malloc(size * sizeof(int));
		memset(have_me_tab, -1, size * sizeof(int));
		int have_me_count = 0;

		int *requests_buffer = malloc(size * sizeof(int));
		memset(requests_buffer, -1, size * sizeof(int));
		int buffer_count = 0;

		int is_correct_answer_count = 0;
		int is_correct = 0;

		int stage_3_complete = NO;

		while (1)
		{
			recvInt(&message, MESSAGE_SIZE, MPI_ANY_SOURCE, MPI_ANY_TAG, &status);

			if (status.MPI_TAG == WANT_TO_DRINK)
			{
				if (i_want_to_drink == YES)
				{
					message = YES;
					sendInt(&message, ONE_INT, status.MPI_SOURCE, ANSWER);
				}
				else
				{
					message = NO;
					sendInt(&message, ONE_INT, status.MPI_SOURCE, ANSWER);
				}
			}
			else if (status.MPI_TAG == ANSWER)
			{
				if(message == YES)
				{
					Add_Mate_To_Group(status.MPI_SOURCE, all_mates);
				}
				answer_count++;

				if (answer_count == size)
				{
					for (int i = 0; i < buffer_count; i++)
					{
						message = Check_If_I_Have_You(all_mates, requests_buffer[i]);
						sendInt(&message, 1, requests_buffer[i], I_HAVE_YOU_ANSWER);
					}
					buffer_count = 0;
					int buf = -1;
					Send_To_All(buf, I_HAVE_YOU_REQUEST);
					//answer_count musi byc czyszczone
					// answer_count = 1; Pamietac zeby wyczyscic przy nowej iteracji
					//free(all_group_indexes);
				}
			}
			else if (status.MPI_TAG == I_HAVE_YOU_REQUEST)
			{
				if(i_want_to_drink == YES)
				{
					if (answer_count == size)
					{
						message = Check_If_I_Have_You(all_mates, status.MPI_SOURCE);
						sendInt(&message, 1, status.MPI_SOURCE, I_HAVE_YOU_ANSWER);
					}
					else
					{
						requests_buffer[buffer_count] = status.MPI_SOURCE;
						buffer_count++;
					}
				}
				else
				{
					message = NO;
					sendInt(&message, 1, status.MPI_SOURCE, I_HAVE_YOU_ANSWER);
				}
			}
			else if (status.MPI_TAG == I_HAVE_YOU_ANSWER)
			{
				if (message == YES)
				{
					have_me_tab[have_me_count] = status.MPI_SOURCE;
					have_me_count++;
				}
				have_you_answer_count++;
				if (have_you_answer_count == size)
				{
					
				//	printf("COMPARE RESULT = %d and my rank is %d\n", Compare_Arrays(all_mates, have_me_tab), rank);
					//sprawdza czy ok
					int *my_ranks = Sum_Arrays(all_mates, have_me_tab);
					if (Compare_Arrays(all_mates, have_me_tab) == YES)
						message = YES;
					else
						message = NO;
					Send_To_Ranks(message, IS_CORRECT, my_ranks);
					have_you_answer_count = 1;

					stage_3_complete = YES;
					if (is_correct_answer_count == Get_Mates_Count(Sum_Arrays(all_mates, have_me_tab)))
					{
						if (is_correct == is_correct_answer_count && all_mates[0] != -1)
						{
							i_want_to_drink = NO;
							Show_Mates(all_mates, "");
							//zakonczone wybieranie
							printf("im in group\n");
							down(semaphore_am_i_in_group_id);
							am_i_in_group = YES;
							up(semaphore_am_i_in_group_id);
						}
					//	printf("ENDOFGATHER\n");

						down(semaphore_end_of_gather_id);
						end_of_gather = YES;
						up(semaphore_end_of_gather_id);

						start_drinking = NO;
						stage_3_complete = NO;
						answer_count = 1;
						is_correct = 0;
						is_correct_answer_count = 0;
						memset(have_me_tab, -1, size * sizeof(int));
						have_me_count = 0;
					}
				}
			}
			else if (status.MPI_TAG == IS_CORRECT)
			{
				if (message == YES)
				{
					is_correct++;
				}
				is_correct_answer_count++;
				if (stage_3_complete && is_correct_answer_count == Get_Mates_Count(Sum_Arrays(all_mates, have_me_tab)))
				{
					if (is_correct == is_correct_answer_count && all_mates[0] != -1)
					{
						i_want_to_drink = NO;
						Show_Mates(all_mates, "");
						//zakonczone wybieranie
						printf("im in group\n");
						down(semaphore_am_i_in_group_id);
						am_i_in_group = YES;
						up(semaphore_am_i_in_group_id);
					}
				//	printf("ENDOFGATHER\n");

					down(semaphore_end_of_gather_id);
					end_of_gather = YES;
					up(semaphore_end_of_gather_id);

					start_drinking = NO;
					stage_3_complete = NO;
					answer_count = 1;
					is_correct = 0;
					is_correct_answer_count = 0;
					memset(have_me_tab, -1, size * sizeof(int));
					have_me_count = 0;
				}
			}
			else if (status.MPI_TAG == ARBITER_REQUEST)
			{
				// down(semaphore_clock_id);
				// timestamp = *lamport_clock;
				// up(semaphore_clock_id);
				// printf("request\n");
				//printf("timestamp=%d message=%d\n", timestamp, message);
				if (master_rank != rank || timestamp > message)
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
				if (master_rank == rank && arbiter_answer_count >= size - ARBITER_SIZE)
				{
					down(semaphore_start_drinking_id);
					start_drinking = YES;
					up(semaphore_start_drinking_id);

					Send_Start_Drinking();
				}
			}
			else if (status.MPI_TAG == START_DRINKING)
			{
				down(semaphore_start_drinking_id);
				start_drinking = YES;
				up(semaphore_start_drinking_id);
			}
			else if (status.MPI_TAG == END_DRINKING)
			{
				arbiter_answer_count = 0;
				ArbiterRequest request;
			//	printf("first = %d, last = %d\n", queryIndexFirst, queryIndexLast);
				for (int i = queryIndexFirst; i < queryIndexLast; i++)
				{
					request = Pick_From_Query(requestsQuery, &queryIndexFirst, &queryIndexLast);
					sendInt(&rank, 1, request.rank, ARBITER_ANSWER);
				}
			}
		}

		printf("I remove semaphores\n");
		pthread_join(thread, NULL);

		semctl(semaphore_am_i_in_group_id, 0, IPC_RMID);

		semctl(semaphore_my_group_index_id, 0, IPC_RMID);

		semctl(semaphore_clock_id, 0, IPC_RMID);

		semctl(semaphore_start_drinking_id, 0, IPC_RMID);

		semctl(semaphore_end_of_gather_id, 0, IPC_RMID);
		printf("MPI finallize\n");
		MPI_Finalize();
		return 0;
	}
}
