#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define DIM 10

enum SEM_TYPE
{
	FAT,
	MOD,
	OUT
};

typedef struct
{
	int n;
	int type;
}shared_mem;

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

void mod(int semid,int shmid,int mod)
{
	shared_mem *mem;
	if((mem = (shared_mem*) shmat(shmid,NULL,0)) == (shared_mem *)-1)
	{
		perror("shmat");
		exit(1);
	}
	int done = 0;
	while(!done)
	{
		WAIT(semid,MOD);
		for(int i = 0; i < DIM; i++)
		{
			if(mem[i].type == 0)
			{
				mem[i].n %= mod;
				mem[i].type = 1;
			}
			if(mem[i].type == 3)
			{
				done = 1;
				SIGNAL(semid,OUT);
				break;
			}
		}
		SIGNAL(semid,OUT);
	}
	//printf("M ENDS\n");
	exit(0);
}

void out(int semid,int shmid)
{
	shared_mem *mem;
	if((mem = (shared_mem*) shmat(shmid,NULL,0)) == (shared_mem *)-1)
	{
		perror("shmat");
		exit(1);
	}
	int done = 0;
	while(!done)
	{
		WAIT(semid,OUT);
		for(int i = 0; i < DIM;i++)
		{
			if(mem[i].type == 1)
			{
				printf("%d\n",mem[i].n);
				mem[i].type = 2;
			}
			if(mem[i].type == 3)
			{
				done = 1;
				SIGNAL(semid,FAT);
				break;
			}
		}
		SIGNAL(semid,FAT);
		
	}	
	//printf("O ENDS\n");
	exit(0);
}

int main(int argc,char* argv[])
{
	if(argc != 3)
	{
		fprintf(stderr,"Bad usage -> %s <input file> <modulus number>\n",argv[0]);
		exit(1);
	}
	int semid,shmid;
	shared_mem* mem;
	int modulus = atoi(argv[2]);
	if((semid = semget(IPC_PRIVATE,3,IPC_CREAT | 0660)) == -1)
	{
		perror("semget");
		exit(1);
	}
	
	if((shmid = shmget(IPC_PRIVATE,sizeof(shared_mem) * DIM,IPC_CREAT | 0660)) == -1)
	{
		perror("shmget");
		exit(1);
	}
	
	if((mem = (shared_mem*) shmat(shmid,NULL,0)) == (shared_mem *)-1)
	{
		perror("shmat");
		exit(1);
	}
	for(int i = 0; i < DIM;i++)
	{
		mem[i].type = -1;
	}	
	if(semctl(semid,FAT,SETVAL,1) == -1)
	{
		perror("semctl FAT SETVAL");
		exit(1);
	}
	
	if(semctl(semid,MOD,SETVAL,0) == -1)
	{
		perror("semctl MOD SETVAL");
		exit(1);
	}
	
	if(semctl(semid,OUT,SETVAL,0) == -1)
	{
		perror("semctl OUT SETVAL");
		exit(1);
	}
	
	if(fork() == 0)
	{
		mod(semid,shmid,modulus);
	}
	
	else if(fork() == 0)
	{
		out(semid,shmid);
	}
	
	FILE  *file = fopen(argv[1],"r");
	if(file == NULL)
	{
		perror("fopen");
		exit(1);
	}
	char b[DIM];
	
	while(fgets(b,DIM,file) != NULL)
	{
		int x = atoi(b);
		WAIT(semid,FAT);
		for(int i = 0; i < DIM;i++)
		{				
			if(mem[i].type == -1 || mem[i].type == 2)
			{
				mem[i].n = x;
				mem[i].type = 0;
				break;
			}	
		}
		SIGNAL(semid,MOD);	
	}
	WAIT(semid,FAT);
	for(int i = 0; i < DIM; i++)
	{
		mem[i].type = 3;
		mem[i].n = -1;
	}
	SIGNAL(semid,MOD);
	wait(NULL);
	wait(NULL);
	return 0;
	
}
