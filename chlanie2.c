#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
//#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SEMCOUNT 1
#define FALSE 0
#define TRUE 1
#define ONE_INT 1
#define THREE_INT 3
#define WANT_TO_DRINK 1
#define ANSWER 2
#define YES 1
#define NO 0

struct sembuf buf;

void Send_To_All( int group_index, int size, int my_rank)
{
	int group_index_answer_rank[3];
	group_index_answer_rank[0] = group_index;
	group_index_answer_rank[1] = YES;
	group_index_answer_rank[2] = my_rank;

	for( int i = 0; i < size; i++)
	{
		if(i != my_rank)
		{
			MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, i, WANT_TO_DRINK, MPI_COMM_WORLD);
			printf("I sent to %d and my rank is %d\n", i, my_rank);
		}
	}
}

void Add_Mate_To_Group( int mate_rank, int* all_mates, int size)
{
	for( int i = 0; i < size; i++)
	{
		if( all_mates[i] == -1)
		{
			all_mates[i] = mate_rank;
			break;
		}
		else if( all_mates[i] == mate_rank)
		{
			break;
		}
	}
}

/*
int Check_If_I_Can_Drink(struct package* packages, int size)
{
	int group_index = -1;
	for(int i = 0; i < size - 1; i++)
	{
		if( packages[i].answer == YES)
		{
			group_index = packages[i].group_index;
			break;
		}
	}

	if( group_index == -1 )
	{
		return -1;
	}

	for(int i = 0; i < size - 1; i++)
	{
		if( packages[i].group_index == group_index && packages[i].answer == NO)
		{
			return -2;
		}
	}

	return group_index;
}

void Get_All_Ranks_From_Group( struct package* packages, int size, int group_index, int* all_ranks )
{
	int index = 0;
	for(int i = 0; i < size; i++)
	{
		if( packages[i].group_index == group_index )
		{
			all_ranks[index] = packages[i].rank;
			index++;
		}
	}
}
*/

void up(int semid)
{
	buf.sem_num = 0;
	buf.sem_op = 1;
	buf.sem_flg = 0;
	semop(semid, &buf, 1);
}

void down(int semid)
{
	buf.sem_num = 0;
        buf.sem_op = -1;
        buf.sem_flg = 0;
        semop(semid, &buf, 1);
}

int main(int argc, char **argv)
{
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

	int size, rank, range = 100, my_group_index = 1;

	int i_want_to_drink_id = shmget(rank + 1, sizeof(int), IPC_CREAT | 0666 ); //was IPC_PRIVATE

	int semaphore_drink_id = semget(rank + 1, SEMCOUNT, 0666 | IPC_CREAT);
	semctl(semaphore_drink_id, 0, SETVAL, (int)1);

	int *i_want_to_drink;
	i_want_to_drink = (int*) shmat(i_want_to_drink_id, NULL, 0);
	*i_want_to_drink = -1;
	shmdt(i_want_to_drink);
	
	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	srand(time(0));

	if(fork() != 0) //root
	{
		printf("start losu losu\n");
		down(semaphore_drink_id);
		i_want_to_drink = (int*) shmat(i_want_to_drink_id, NULL, 0);
		
		while(*i_want_to_drink != 1)
		{
			*i_want_to_drink = rand()%range;
			shmdt(i_want_to_drink);
			up(semaphore_drink_id);
			sleep(0.8);
			down(semaphore_drink_id);
			i_want_to_drink = (int*) shmat(i_want_to_drink_id, NULL, 0);
		}

		shmdt(i_want_to_drink);
		up(semaphore_drink_id);

		printf("Chce pic! %d\n", rank);
		
		printf("I send to all and wait for all and my rank is %d\n", rank);
		Send_To_All(my_group_index, size, rank); //group_index can be changed, so it is temporary solution

		sleep(1000);
		//wait for end
	}
	else //child
	{
		int group_index_answer_rank[3], answer_count = 0;
		int* i_want_to_drink2;
		int* all_mates_in_group = malloc(sizeof(int)* size);
        	memset(all_mates_in_group, -1, sizeof(int)* size);
		int invalid = FALSE;
		
		while(1)
		{
			printf("i am waiting, my rank is %d\n", rank);
			MPI_Recv(group_index_answer_rank, THREE_INT, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			printf("I received tag %d from %d and my rank is %d\n", status.MPI_TAG, status.MPI_SOURCE, rank);
			if( status.MPI_TAG == WANT_TO_DRINK)
			{
				down(semaphore_drink_id);
				i_want_to_drink2 = (int*) shmat(i_want_to_drink_id, NULL, 0);
				if(*i_want_to_drink2 == 1)
				{
					shmdt(i_want_to_drink2);
					up(semaphore_drink_id);
					group_index_answer_rank[0] = my_group_index;
					group_index_answer_rank[2] = rank;
					if( group_index_answer_rank[0] == my_group_index)
					{
						group_index_answer_rank[1] = YES;
						printf("I answer YES to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
						MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, status.MPI_SOURCE, ANSWER, MPI_COMM_WORLD);
					}
					else
					{
						group_index_answer_rank[1] = NO;
                                                printf("I answer NO2 to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
                                                MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, status.MPI_SOURCE, ANSWER, MPI_COMM_WORLD);
					}
				}
				else
				{
					shmdt(i_want_to_drink2);
					up(semaphore_drink_id);

					if(my_group_index < group_index_answer_rank[0])
					{
						my_group_index = group_index_answer_rank[0];
					}
					group_index_answer_rank[0] = my_group_index;
					group_index_answer_rank[1] = NO;
					group_index_answer_rank[2] = rank;
					printf("I answer NO to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
					MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, status.MPI_SOURCE, ANSWER, MPI_COMM_WORLD);
				}

			}
			else if( status.MPI_TAG == ANSWER)
			{
	/*			answer_count++;
				if( group_index_answer_rank[1] == YES && invalid == FALSE)
				{
					Add_Mate_To_Group( status.MPI_SOURCE, all_mates_in_group, size );
				}
				else if( group_index_answer_rank[0] == my_group_index)
				{
					if( group_index_answer_rank[0] > my_group_index)
					{
						my_group_index = group_index_answer_rank[0];
					}
					invalid = TRUE;
					memset(all_mates_in_group, -1, sizeof(int)* size);
				}

				if(answer_count == size - 1)
				{
				//	printf("My group %d, %d, my rank is %d\n", all_mates_in_group[0], all_mates_in_group[1], rank);
					answer_count = 0;
					invalid = FALSE;
				} */
				// It is comment just to debug segmentation fault
			}
		}
	}
	printf("WAIT\n");
	wait(0);
	semctl(semaphore_drink_id, 0, IPC_RMID);
	shmctl(i_want_to_drink_id, IPC_RMID, NULL);
	printf("MPI finallize\n");
	MPI_Finalize();
	return 0;
}
