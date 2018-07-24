#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ONE_INT 1
#define THREE_INT 3
#define WANT_TO_DRINK 1
#define ANSWER 2
#define YES 1
#define NO 0

struct package
{
	int group_index;
	int answer;
	int rank;
};

struct package* Send_To_All_And_Receive_From_All( int data, int size, int my_rank)
{
	int recv_count = 0;
        int group_index_and_answer_and_rank[3];
        struct package* recv_packages = malloc(sizeof(struct package) * (size));
        MPI_Status status;

	for( int i = 0; i < size; i++)
	{
		if(i != my_rank)
		{
			MPI_Send(&data, ONE_INT, MPI_INT, i, WANT_TO_DRINK, MPI_COMM_WORLD);
			printf("I sent to %d and my rank is %d\n", i, my_rank);
			MPI_Recv(group_index_and_answer_and_rank, THREE_INT, MPI_INT, i, ANSWER, MPI_COMM_WORLD, &status);
			recv_packages[recv_count].group_index = group_index_and_answer_and_rank[0];
                	recv_packages[recv_count].answer = group_index_and_answer_and_rank[1];
                	recv_packages[recv_count].rank = group_index_and_answer_and_rank[2];
                	recv_count++;
			sleep(1);
		}
	}
	
	printf("Receive from all my rank is %d\n", my_rank);
	return recv_packages;
}


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

int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);
	int size, rank, i_want_to_drink = -1, range = 100, my_group_index = 1;
	int * all_ranks_in_group = malloc(sizeof(int)* size);
	memset(all_ranks_in_group, -1, sizeof(int)* size);

	struct package* packages = malloc(sizeof(struct package)* (size - 1));

	MPI_Status status;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	srand(time(0));

	if(fork() != 0) //root
	{
		sleep(1);
		printf("start losu losu\n");
		while(i_want_to_drink != 1)
		{
			i_want_to_drink = rand()%range;
			sleep(rank);
		}

		printf("Chce pic! %d\n", rank);

		printf("I send to all and wait for all and my rank is %d\n", rank);
		packages = Send_To_All_And_Receive_From_All(my_group_index, size, rank);
	
	//	packages = Recv_Package_From_All(size);
	
		for( int i = 0 ; i < size - 1; i++)
		{
			printf("Package number %d, group index %d, my rank %d\n", i, packages[i].group_index, rank);
		}

		/*	int temp_group_index = Check_If_I_Can_Drink(packages, size);
			if( temp_group_index == -1 || temp_group_index == -2)
			{
		//	my_group_index++;
		//	Send_To_All(my_group_index, ONE_INT, size, WANT_TO_DRINK, rank);
		//	packages = Recv_Package_From_All(THREE_INT, size);

		}
		else
		{
		my_group_index = temp_group_index;
		}
		 */

		while(1){sleep(1);}	
		//Get_All_Ranks_From_Group(packages, size, my_group_index, all_ranks_in_group);

	}
	else //child
	{
		int his_group_index = 1, group_index_answer_rank[3];
		
		while(1)
		{
			printf("i am waiting, my rank is %d\n", rank);
			MPI_Recv(&his_group_index, ONE_INT, MPI_INT, MPI_ANY_SOURCE, WANT_TO_DRINK, MPI_COMM_WORLD, &status);
			printf("I received invitation from %d and my rank is %d\n", status.MPI_SOURCE, rank);
			if( status.MPI_TAG == WANT_TO_DRINK)
			{
				if(i_want_to_drink == 1 && his_group_index == my_group_index)
				{
					group_index_answer_rank[0] = my_group_index;
					group_index_answer_rank[1] = YES;
					group_index_answer_rank[2] = rank;
					printf("I answer YES to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
			//		MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, status.MPI_SOURCE, ANSWER, MPI_COMM_WORLD);	
				}
				else
				{
					if(my_group_index < his_group_index)
					{
						my_group_index = his_group_index;
					}
					group_index_answer_rank[0] = my_group_index;
					group_index_answer_rank[1] = NO;
					group_index_answer_rank[2] = rank;
					printf("I answer NO to  %d and my rank is %d\n", status.MPI_SOURCE, rank);
					MPI_Send(group_index_answer_rank, THREE_INT, MPI_INT, status.MPI_SOURCE, ANSWER, MPI_COMM_WORLD);
				}

			}

		}
	}
	wait(0);
	printf("MPI finallize\n");
	MPI_Finalize();
	return 0;
}
