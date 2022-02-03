#include <stdio.h>
#include<ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include<fcntl.h>

#define ALPHABET_SIZE 26

enum SEM_TYPE { AL,MZ,FATHER };

typedef struct
{
	char c;
	int stats[ALPHABET_SIZE];
	int done;
} shared_memory;

int WAIT(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
	return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
	return semop(sem_des, operazioni, 1);
}

void al(int semid,int shmid)
{
	shared_memory *mem;
	if ((mem = (shared_memory *) shmat(shmid, NULL, 0)) == (shared_memory *) -1) {
        perror("shmat");
        exit(1);
    }	
	int done = 0;
	while(!done)
	{
		WAIT(semid,AL);
		if(mem->done == 0)
		{
			char c = mem->c;
			int offset = (int) c - (int) 'a';
			mem->stats[offset]++;
		}
		if(mem->done == 1)
		{
			done = 1;
			SIGNAL(semid,FATHER);
		}
		SIGNAL(semid,FATHER);
	}	
	exit(0);
}

void mz(int semid,int shmid)
{
	shared_memory *mem;
	if ((mem = (shared_memory *) shmat(shmid, NULL, 0)) == (shared_memory *) -1) {
        perror("shmat");
        exit(1);
    }
	int done = 0;
	while(!done)
	{
		WAIT(semid,MZ);
		if(mem->done == 0)
		{
			char c = mem->c;
			int offset = (int) c - (int) 'a';
			mem->stats[offset]++;
		}
		else if(mem->done == 1)
			done = 1;
		SIGNAL(semid,FATHER);
	}	
	exit(0);
}


int main(int argc,char* argv[])
{
	if(argc!=2)
	{
		fprintf(stdout,"bad usage : %s <file>\n",argv[0]);
		exit(1);
	}
	
	shared_memory *mem;
	int shm_des,sem_des,fd;
	char *map;
	struct stat statbuf;
	if((shm_des = shmget(IPC_PRIVATE , sizeof(shared_memory), IPC_CREAT | 0660)) ==-1) 
	{
        perror("shmget");
        exit(1);
    }
    
    if ((mem = (shared_memory *)shmat(shm_des, NULL, 0)) == (shared_memory *)-1) 
    {
        perror("shmat");
        exit(1);
    }
    mem->done = 0;
    if ((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1) 
    {
        perror("semget");
        exit(1);
    }
    
    if (semctl(sem_des, AL, SETVAL, 0) == -1) 
    {
        perror("semctl SETVAL AL");
        exit(1);
    }
    
    if (semctl(sem_des, MZ, SETVAL, 0) == -1) 
    {
        perror("semctl SETVAL MZ");
        exit(1);
    }
  	
  	if (semctl(sem_des, FATHER, SETVAL, 1) == -1) 
    {
        perror("semctl SETVAL FATHER");
        exit(1);
    }
  	
  	if((fd = open(argv[1],O_RDONLY)) == -1)
  	{
  		perror("open");
  		exit(1);
  	}
  	
  	if((fstat(fd,&statbuf)) == -1)
  	{
  		perror("fstat");
  		exit(1);
  	}
  	if((map = mmap(NULL,statbuf.st_size,PROT_READ,MAP_PRIVATE,fd,0)) == MAP_FAILED)
  	{
  		perror("mmap");
  		exit(1);
  	}
  	
  	if(fork() == 0)
  	{
  		al(sem_des,shm_des);
  	}
  	else if(fork() == 0)
  	{
  		mz(sem_des,shm_des);
  	}
  	
  	for(int i = 0; i < statbuf.st_size;i++)
  	{
  		if((tolower(map[i]) >= 'a') && (tolower(map[i]) <= 'l'))
  		{
  			WAIT(sem_des,FATHER);
  			mem->c = tolower(map[i]);
  			char ch = mem->c;
  			SIGNAL(sem_des,AL);
  		}
  		if((tolower(map[i]) >= 'm') && (tolower(map[i]) <= 'z'))
  		{
  			WAIT(sem_des,FATHER);
  			mem->c = tolower(map[i]);
  			char ch = mem->c;
  			SIGNAL(sem_des,MZ);
  		}
  		else
  		{
  			WAIT(sem_des,FATHER);
  			mem->c = 0;
  			SIGNAL(sem_des,FATHER);
  		}
  	}
 	WAIT(sem_des,FATHER);
	mem->done = 1;
  	SIGNAL(sem_des,AL);
	WAIT(sem_des,FATHER);
	mem->done = 1;
  	SIGNAL(sem_des,MZ);
	wait(NULL);
	wait(NULL);
	for(int i = 0; i < ALPHABET_SIZE; i++)
	{
		printf("%c = %d\n",((char)'a'+i),mem->stats[i]);
	}
	if((munmap(map,statbuf.st_size)) == -1)
	{
		perror("munmap");
		exit(1);
	}
	if((shmctl(shm_des, IPC_RMID, NULL)) == 1)
	{
		perror("shmctl");
		exit(1);
	}
	if((semctl(sem_des,0,IPC_RMID,0)) == -1)
	{
		perror("semctl");
		exit(1);
	}
	return 0;	
}

