#include<stdio.h>
#include<stdlib.h>
#include<sys/sem.h>
#include<sys/ipc.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/shm.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<time.h>

#define DIM 8

typedef enum
{
	REQ,
	DATA,
	EXIT,
	
}shared_mem_payload;

typedef struct
{
	char m1;
	char m2;
	int winner;
	shared_mem_payload payload;
}shared_mem;

enum SEM_TYPE
{
	P1,
	P2,
	G,
	T,
	F
};

int WAIT(int sem_des, int num_semaforo)
{
	struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
	return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo)
{
	struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
	return semop(sem_des, operazioni, 1);
}

void p1(int semid,int shmid)
{
	shared_mem *mem;
	if((mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem *) -1)
	{
		perror("shmat");
		exit(1);
	}
	while(1)
	{
		WAIT(semid,P1);
		if(mem->payload == EXIT)
		{
			mem->payload = EXIT;
			mem->m1 = 0;
			mem->m2 = 0;
			mem->winner = -1;
			printf("P1 ends\n");
			SIGNAL(semid,P2);
			break;
		}
		if(mem->payload == REQ)
		{
			int x = rand() % 12 + 1;
			mem->payload = DATA;
			if(x  >= 1 && x < 5)
			{
					mem->m1 = 's';
					printf("P1 mossa sasso\n");
					SIGNAL(semid,P2);
					continue;
			}
			else if(x >= 5 && x < 9)
			{		mem->m1 = 'c';
					printf("P1 mossa carta\n");
					SIGNAL(semid,P2);
					continue;
			}
			else if(x >= 9 && x < 12)
			{		mem->m1 = 'f';
					printf("P1 mossa forbici\n");
					SIGNAL(semid,P2);
					continue;		
			}
		}
		else{
			printf("ERROR in chain REQ missing\n");
			exit(1);
		}
	}
	exit(0);
}

void p2(int semid,int shmid)
{
	shared_mem *mem;
	if((mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem *) -1)
	{
		perror("shmat");
		exit(1);
	}
	while(1)
	{
		WAIT(semid,P2);
		if(mem->payload == EXIT)
		{
			mem->payload = EXIT;
			mem->m1 = 0;
			mem->m2 = 0;
			mem->winner = -1;
			printf("P2 ends\n");
			SIGNAL(semid,G);
			break;
		}
		else if(mem->payload == DATA)
		{
			mem->payload = DATA;
			int x = rand() % 30 ;
			if(x >= 0 && x < 10)
			{
					mem->m2 = 's';
					printf("P2 mossa sasso\n");
					SIGNAL(semid,G);
					continue;
					
			}
			else if(x >= 10 && x < 20)
			{
					mem->m2 = 'c';
					printf("P2 mossa carta\n");
					SIGNAL(semid,G);
					continue;
			}		
			else if(x >= 20 && x <= 30)
			{
				mem->m2 = 'f';
				printf("P2 mossa forbici\n");
				SIGNAL(semid,G);
				continue;
			}
		}
		else{
			printf("ERROR in chain DATA missing to P2\n");
			exit(1);
		}
	}
	exit(0);
}

void g(int semid,int shmid)
{
	shared_mem *mem;
	if((mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem *) -1)
	{
		perror("shmat");
		exit(1);
	}
	int game = 0;
	while(1)
	{
		WAIT(semid,G);
		game++;
		if(mem->payload == EXIT)
		{
			mem->payload = EXIT;
			mem->m1 = 0;
			mem->m2 = 0;
			mem->winner = -1;
			printf("G ends\n");
			break;
		}
		if(mem->m1 == mem->m2)
		{
			printf("G: partita n.%d passa e quindi da ripetere\n",game);
			mem->winner = 0;
			mem->m1 = 0;
			mem->m2 = 0;
			mem->payload = REQ;
			SIGNAL(semid,P1);
			continue;
		}
		if(mem->m1 == 'c')
		{
			if(mem->m2 == 'f')
			{
				mem->winner = 2;
				printf("G: vince P2\n");
				SIGNAL(semid,T);
				continue;
			}
			else if(mem->m2 == 's')
			{
				mem->winner = 1;
				printf("G: vince P1\n");
				SIGNAL(semid,T);
				continue;	
			}
			else
			{
				printf("error occurred, no draws here\n");
				exit(1);
			}
		}
		else if(mem->m1 == 'f')
		{
			if(mem->m2 == 'c')
			{
				mem->winner = 1;
				printf("G: vince P1\n");
				SIGNAL(semid,T);
				continue;
			}
			else if(mem->m2 == 's')
			{
				mem->winner = 2;
				printf("G: vince P2\n");
				SIGNAL(semid,T);
				continue;					
			}
			else
			{
				printf("error occurred, no draws here\n");
				exit(1);
			}
		}
		else if(mem->m1 == 's')
		{
			if(mem->m2 == 'c')
			{
				mem->winner = 2;
				printf("G: vince P2\n");
				SIGNAL(semid,T);
				continue;
			}
			else if(mem->m2 == 'f')
			{
				mem->winner = 1;
				printf("G: vince P1\n");
				SIGNAL(semid,T);	
				continue;
			}
			else
			{
				printf("error occurred, no draws here\n");
				exit(1);
			}				
		}			
	}				
	exit(0);
}


void t(int semid,int shmid,int games)
{
	int x = 0,y = 0,draws = 0; 
	shared_mem *mem;
	if((mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem *) -1)
	{
		perror("shmat");
		exit(1);
	}
	int c = 0;
	while(1)	
	{
		WAIT(semid,T);
		c++;
		if(mem->payload == DATA)
		{
			if(mem->winner == 1)
			{
				 x++;
				 printf("T: classifica parziale p1 = %d p2 = %d\n",x,y);
			}
			if(mem->winner == 2)
			{
				y++;
				printf("T: classifica parziale p1 = %d p2 = %d\n",x,y);	
			}
			if(mem->winner == 0)
			{
				printf("PROBLEMS, draws here\n");
				exit(1);
			}
			if(c == games)
			{
				printf("T: classifica finale p1 = %d p2 = %d\n",x,y);
				if(x > y)
					printf("T: vince P1\n");
				if(x < y)
					printf("T: vince P2\n");
				if(x == y)
					printf("T: pareggio fra P1 e P2\n");		
				mem->m1 = 0;
				mem->m2 = 0;
				mem->payload = EXIT;
				mem->winner = -1;
				SIGNAL(semid,F);
				printf("T ends\n");
				break;
			}
			else
			{
				mem->payload = REQ;
				SIGNAL(semid,P1);
			}				
		}
	}
	exit(0);	
}	


int main(int argc, char* argv[])
{
	srand(time(NULL));
	if(argc != 2)
	{
		printf("Bad usage -> %s <num games>\n",argv[0]);
		exit(1);
	}
	int num = atoi(argv[1]);
	int shmid,semid;
	shared_mem *mem;
	if((shmid = shmget(IPC_PRIVATE,sizeof(shared_mem), IPC_CREAT | 0660)) == -1)
	{
		perror("msgget");
		exit(1);
	}
	
	if((semid = semget(IPC_PRIVATE ,5 ,IPC_CREAT | 0660)) == -1)
	{
		perror("semget");
		exit(1);
	}
	
	if((mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem *) -1)
	{
		perror("shmat");
		exit(1);
	}
	if((semctl(semid,P1,SETVAL,0)) == -1)
	{
		perror("semctl P1");
		exit(1);
	}
	
	if((semctl(semid,P2,SETVAL,0)) == -1)
	{
		perror("semctl P2");
		exit(1);
	}
	
	if((semctl(semid,G,SETVAL,0)) == -1)
	{
		perror("semctl G");
		exit(1);
	}
	
	if((semctl(semid,T,SETVAL,0)) == -1)
	{
		perror("semctl T");
		exit(1);
	}
	
	if((semctl(semid,F,SETVAL,1)) == -1)
	{
		perror("semctl F");
		exit(1);
	}	
	
	if(fork() == 0)
	{
		p1(semid,shmid);
	}
	else if(fork() == 0)
	{
		p2(semid,shmid);
	}
	else if(fork() == 0)
	{
		g(semid,shmid);
	}
	else if(fork() == 0)
	{
		t(semid,shmid,num);
	}
	WAIT(semid,F);
	mem->m1 = 0;
	mem->m2 = 0;
	mem->winner = -1;
	mem->payload = REQ;
	SIGNAL(semid,P1);
	WAIT(semid,F);
	if(mem->payload == EXIT)
	{
		mem->payload = EXIT;
		SIGNAL(semid,P1);
	}
	for(int i = 0; i < 4; i++)
		wait(NULL);
		
	if((shmctl(shmid, IPC_RMID, NULL)) == 1)
	{
		perror("shmctl");
		exit(1);
	}
	if((semctl(semid,0,IPC_RMID,0)) ==-1)
	{
		perror("semtcl finale");
		exit(1);
	}
	
	return 0;	
}
