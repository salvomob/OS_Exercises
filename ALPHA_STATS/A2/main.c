#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/wait.h>
#include<string.h>
#include<sys/mman.h>
#include<ctype.h>

#define ALPH 26

enum SEM_TYPE
{
	CHILD,
	MUTEX,
};

typedef struct
{
	char c;
	int occ[ALPH];
	int nl;
}shared_mem;

int WAIT(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
	return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
	return semop(sem_des, operazioni, 1);
}

void child(int semid,int shmid,char *filename)
{
	//printf("child on %s\n",filename);
	shared_mem *mem;
	int fd;
	char *map;
	struct stat statbuf;
	if ((mem = (shared_mem *)shmat(shmid, NULL, 0)) == (shared_mem *)-1) 
    {
        perror("shmat");
        exit(1);
    }
    
   	if((fd = open(filename,O_RDONLY | 0660)) == -1)
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
	printf("size file %s : %ld\n",filename,statbuf.st_size);
	int i = 0;
	char c;
	for(i; i < statbuf.st_size ; i++)
	{
		c = tolower(map[i]);
		if(c <'a' || c >'z')
		{
			WAIT(semid,CHILD);
			mem->nl++;
			SIGNAL(semid,CHILD);			
		}
		else
		{
			WAIT(semid,CHILD);
			mem->c = c;
			//printf("map[i] = %c\n",c);
			int off = (int) c - (int) 'a';
			mem->occ[off]++;
			SIGNAL(semid,CHILD);
		}	
	}
	if((munmap(map,statbuf.st_size)) == -1)
	{
		perror("munmap");
		exit(1);
	}
	
	if((close(fd)) == -1)
	{
		perror("close");
		exit(1);
	}
	exit(0);
}

int main(int argc,char* argv[])
{
	if(argc<2)
	{
		fprintf(stderr,"Basd usage -> %s <file 1> ...  <file n>\n",argv[0]);
		exit(1);
	}
	int nfiles = argc - 1;
	int semid,shmid;
	shared_mem *mem;
	
	if((shmid = shmget(IPC_PRIVATE,sizeof(shared_mem),IPC_CREAT | 0660)) == -1)
	{
		perror("shmget");
		exit(1);
	}
	
	if ((mem = (shared_mem *)shmat(shmid, NULL, 0)) == (shared_mem *)-1) 
    {
        perror("shmat");
        exit(1);
    }
	
	if((semid = semget(IPC_PRIVATE,2,IPC_CREAT | 0660)) == -1)
	{
		perror("semget");
		exit(1);
	}
	
	if((semctl(semid,CHILD,SETVAL,1)) == -1)
	{
		perror("semctl CHILD");
		exit(1);
	}
	
	if((semctl(semid,MUTEX,SETVAL,1)) == -1)
	{
		perror("semctl MUTEX");
		exit(1);
	}
	
	for(int i = 1; i <= nfiles; i++)
	{
		if(fork() == 0)
		{
			child(semid,shmid,argv[i]);
		}
	}
	
	for(int i = 1; i <= nfiles; i++)
	{
		wait(NULL);
	}
	
	/*
	
	
	stampa a video
	
	*/
	
	double total = 0;// = mem->nl;
	for(int i = 0; i < ALPH; i++)
	{
		total += mem->occ[i];
	}
	//printf("total = %f\n",total);
	for(int i = 0; i < ALPH; i++)
	{
		printf("%c : %.2f%\n",((char) 'a' + i),(double) 100*((double)mem->occ[i]/(double)total));
	}
	//printf("NOT LETTER : %.2f%\n",(double)(mem->nl/total)*100);
	if((shmctl(shmid, IPC_RMID, NULL)) == 1)
	{
		perror("shmctl");
		exit(1);
	}
	
	if((semctl(semid,0,IPC_RMID,NULL)) == -1)
	{
		perror("semctl");
		exit(1);
	}	
	
	return 0;
}
