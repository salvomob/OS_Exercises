#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<sys/stat.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<string.h>
#include<fcntl.h>
#include<unistd.h>

#define DIM 1024

enum SEM_TYPE
{
	READER,
	FILE_C,
	DIR_C,
	FATHER,
	MUTEX
};

typedef enum 
{
	START,
	STOP,
	DATA,
	EXIT
}mem_payload;	

typedef struct
{
	char pathname[DIM];
	char name[DIM];
	long size;
	int n;
	mem_payload payload;
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

void Reader(int semid,int shmid,char *path,int num)
{
	shared_mem *mem;
	DIR *dir;
	if(( mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem* ) -1)
	{
		perror("shmat");
		exit(1);
	}
	struct dirent *entry;
	struct stat statbuf;
	if((chdir(path)) == -1)
	{
		perror("chdir");
		exit(1);
	}
	if((dir = opendir(path)) == NULL)
	{
		perror("opendir");
		exit(1);
	}
	while((entry = readdir(dir)) != NULL)
	{
		int fd;
		WAIT(semid,READER);
		if(( fd = open(entry->d_name,O_RDONLY)) == -1)
		{
			perror("open");
			exit(1);
		}
		if((fstat(fd,&statbuf)) == -1)
		{
			perror("fstat");
			exit(1);
		}
		if(!strcmp(".",entry->d_name) || !strcmp("..",entry->d_name))
		{
			SIGNAL(semid,READER);
			continue;
		}
		if((S_ISDIR(statbuf.st_mode)))
		{
			mem->payload = DATA;
			strcpy(mem->pathname,path);
			for(int i = 0; i < strlen(entry->d_name); i++)
			{
				mem->name[i] = entry->d_name[i];
			}
			mem->size = 0;
			SIGNAL(semid,DIR_C);
		}
		if((S_ISREG(statbuf.st_mode)))
		{
			mem->payload = DATA;
			strcpy(mem->pathname,path);
			for(int i = 0; i < strlen(entry->d_name); i++)
			{
				mem->name[i] = entry->d_name[i];
			}
			mem->size = statbuf.st_size;
			SIGNAL(semid,FILE_C);
		}
	}
	WAIT(semid,READER);
	mem->payload = STOP;
	SIGNAL(semid,READER);	
	SIGNAL(semid,MUTEX);
	
	//printf("R%d ends\n",num);
	exit(0);		
}

void File_consumer(int semid,int shmid)
{
	shared_mem *mem;
	if(( mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem* ) -1)
	{
		perror("shmat");
		exit(1);
	}
	int done = 0; 
	while(!done)
	{
		WAIT(semid,FILE_C);
		if(mem->payload == EXIT)
		{
			mem->payload = EXIT;
			done = 1;
			SIGNAL(semid,DIR_C);
			continue;
		}
		else
		{
			 printf("%s%s [file di %li bytes]\n",mem->pathname,mem->name,mem->size);
			 for(int i = 0; i < DIM; i++)
			{
				mem->pathname[i] = 0;
				mem->name[i] = 0;
			}
			 SIGNAL(semid,READER);
		}
	}
	//printf("FILE CONSUMER ENDS\n");
	exit(0);
}

void Dir_consumer(int semid,int shmid)
{
	shared_mem *mem;
	if(( mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem* ) -1)
	{
		perror("shmat");
		exit(1);
	}
	int done = 0; 
	while(!done)
	{
		WAIT(semid,DIR_C);
		if(mem->payload == EXIT)
		{
			done = 1;
			SIGNAL(semid,FATHER);
			continue;
		}
		else
		{
			printf("%s%s [directory]\n",mem->pathname,mem->name);
			for(int i = 0; i < DIM; i++)
			{
				mem->pathname[i] = 0;
				mem->name[i] = 0;
			}
			SIGNAL(semid,READER);
		} 
	}
	//printf("DIR CONSUMER ENDS\n");
	exit(0);
}

int main(int argc,char *argv[])
{
	if(argc < 2)
	{
		printf("Bad Usage -> %s <dir> ... <dir>\n",argv[0]);
		exit(1);
	}
	
	shared_mem *mem;
	int children = argc-1;
	int semid,shmid;
	if((semid = semget(IPC_PRIVATE,5,IPC_CREAT | 0660)) == -1)
	{
		perror("semget");
		exit(1);
	}
	
	if((shmid = shmget(IPC_PRIVATE,sizeof(shared_mem),IPC_CREAT | 0660)) == -1)
	{
		perror("shmget");
		exit(1);
	}
	
	if(( mem = (shared_mem *) shmat(shmid,NULL,0)) == (shared_mem* ) -1)
	{
		perror("shmat");
		exit(1);
	}
	mem->n = 0;
	if((semctl(semid,READER,SETVAL,1)) == -1)
	{
		perror("setval READER");
		exit(1);
	}
	
	if((semctl(semid,FILE_C,SETVAL,0)) == -1)
	{
		perror("setval FILE_C");
		exit(1);
	}
	
	if((semctl(semid,DIR_C,SETVAL,0)) == -1)
	{
		perror("setval DIR_C");
		exit(1);
	}
	
	if((semctl(semid,MUTEX,SETVAL,1)) == -1)
	{
		perror("setval MUTEX");
		exit(1);
	}
	
	if((semctl(semid,FATHER,SETVAL,0)) == -1)
	{
		perror("setval FATHER");
		exit(1);
	}
	
	for(int i = 1; i <= children; i++)
	{	
		if(fork() == 0)
			Reader(semid,shmid,argv[i],children);
	}
	if(fork() == 0)
		File_consumer(semid,shmid);
	else if(fork() == 0)
		Dir_consumer(semid,shmid);
	sleep(1);
	int x = 0;
	for(int i = 0; i < children+1 ; i++)
	{
		WAIT(semid,MUTEX);
		if(mem->payload == STOP)
		{
			x++;
		//	printf("STOP %d GOT\n",x);
		}
		if(x == children)
		{
			mem->payload = EXIT;
			SIGNAL(semid,FILE_C);
		}
		//SIGNAL(semid,MUTEX);	
	}
	for(int i = 0; i < children+2 ; i++)
	wait(NULL);
	
	if((shmctl(shmid,IPC_RMID,NULL))== -1)
	{
		perror("shmctl");
		exit(1);
	}
		
	if(semctl(semid,0,IPC_RMID) == -1)
	{
		perror("semctl FINAL");
		exit(1);
	}
	
} 
