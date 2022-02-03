#include<stdio.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<unistd.h>
#include<dirent.h>
#include<fcntl.h>
#include<string.h>

#define DIM 1024

enum SEM_TYPE
{
	READER,
	FATHER,
	MUTEX,
	FILE_CONS,
	DIR_CONS
};

typedef struct
{
	int size;
	char name[DIM];
	int done;
	int n;
}shm_mem;

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
	shm_mem *mem;
	struct dirent *entry;
	struct stat statbuf;
	DIR *dp;
	if((mem = (shm_mem *) shmat(shmid,NULL,0)) == (shm_mem *)-1)
	{
		printf("shmat");
		exit(1);
	}
	if(chdir(path) == -1)
	{
		perror("chdir");
		exit(1);
	} 
	dp = opendir(".");
	if(dp == NULL)
	{
		perror("opendir");
		exit(1);
	}
	while((entry = readdir(dp)) != NULL)
	{

		lstat(entry->d_name,&statbuf);
	//	WAIT(semid,MUTEX);
		WAIT(semid,READER);
		//printf("scanning dir\n");
		if(S_ISREG(statbuf.st_mode))
		{
			mem->size = statbuf.st_size;
			strcpy(mem->name,entry->d_name);
		//	printf("signal FILE from READ\n");
		//	SIGNAL(semid,MUTEX);
			SIGNAL(semid,FILE_CONS);
		}
		if(S_ISDIR(statbuf.st_mode))
		{
			if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))
			{
				//printf(". or ..\n");
				SIGNAL(semid,READER);
				continue;
			}
			else
			{
				mem->size = 0;	
				strcpy(mem->name,path);
				char delim = '/';
				strncat(mem->name,&delim,1);
				strcat(mem->name,entry->d_name);
			//	printf("signal DIR from READ\n");
		//		SIGNAL(semid,MUTEX);		
				SIGNAL(semid,DIR_CONS);
			}	
		}

	}
	if(mem->n == num-1)
	{
		WAIT(semid,READER);
		mem->done = 1;
		SIGNAL(semid,FILE_CONS);
		//printf("R ENDS\n");
		exit(0);
	}
	else{
		WAIT(semid,MUTEX);
		mem->n++;
		//printf("stopped by F X = %d\n",mem->n);
		SIGNAL(semid,FATHER);
		exit(0);	
	}
}

void File_Consumer(int semid,int shmid)
{
	shm_mem *mem;
	if((mem = (shm_mem *) shmat(shmid,NULL,0)) == (shm_mem *)-1)
	{
		printf("shmat");
		exit(1);
	}
	while(1)
	{
		//WAIT(semid,MUTEX);
		WAIT(semid,FILE_CONS);
	//	printf("file\n");
		if(mem->done)
		{
			SIGNAL(semid,DIR_CONS);
			//printf("F ENDS\n");
			//SIGNAL(semid,MUTEX);
			exit(0);
		}
		else printf("%s [file di %d bytes]\n",mem->name,mem->size);
	//	SIGNAL(semid,MUTEX);
		SIGNAL(semid,READER);
	}
	exit(0);
}

void Dir_Consumer(int semid,int shmid)
{
	shm_mem *mem;
	if((mem = (shm_mem *) shmat(shmid,NULL,0)) == (shm_mem *)-1)
	{
		printf("shmat");
		exit(1);
	}
	while(1)
	{
	//	WAIT(semid,MUTEX);
		WAIT(semid,DIR_CONS);
	//	printf("dir\n");
		if(mem->done)
		{
		//	printf("D ENDS\n");
			SIGNAL(semid,READER);
			exit(0);
		}
		else printf("%s [directory]\n",mem->name);
//		SIGNAL(semid,MUTEX);
		SIGNAL(semid,READER);
	}
	exit(0);
}

int main(int argc,char* argv[])
{
	if(argc < 2)
	{
		printf("Bad usage-> %s <dir> ... <dir>\n",argv[0]);
		exit(1);
	}
	int semid,shmid;
	shm_mem *mem;
	int children = argc-1;
	if((shmid = shmget(IPC_PRIVATE,sizeof(shm_mem),IPC_CREAT | 0660)) == -1)
	{
		perror("shmget");
		exit(1);
	}
	
	if((mem = (shm_mem *) shmat(shmid,NULL,0)) == (shm_mem *)-1)
	{
		printf("shmat");
		exit(1);
	}
	mem->n = 0;
	if((semid = semget(IPC_PRIVATE,5,IPC_CREAT | 0660)) == -1)
	{
		perror("semget");
		exit(1);
	}
	
	if((semctl(semid,MUTEX,SETVAL,0)) == -1)
	{
		perror("semctl MUTEX");
		exit(1);
	}
	
	if((semctl(semid,FATHER,SETVAL,1)) == -1)
	{
		perror("semctl FATHER");
		exit(1);
	}
	
	if((semctl(semid,READER,SETVAL,1)) == -1)
	{
		perror("semctl READER");
		exit(1);
	}
	
	if((semctl(semid,FILE_CONS,SETVAL,0)) == -1)
	{
		perror("semctl FILE_CONS");
		exit(1);
	}
	
	if((semctl(semid,DIR_CONS,SETVAL,0)) == -1)
	{
		perror("semctl DIR_CONS");
		exit(1);
	}
	
	for(int i = 0; i < children;i++)
	{
		if(fork() == 0)
		{
			Reader(semid,shmid,argv[i+1],children);
		}
	}
	
	if(fork() == 0)
	{
		File_Consumer(semid,shmid);
	}
	else if(fork() == 0)
	{
		Dir_Consumer(semid,shmid);
	
	}
	for(int i = 0; i < (children); i++)
	{
		WAIT(semid,FATHER);
		SIGNAL(semid,MUTEX);
	}
	for(int i = 0; i < (children+2); i++)
	{
		wait(NULL);
	}
	if((shmctl(shmid,IPC_RMID,NULL))== -1)
	{
		perror("shmctl");
		exit(1);
	}
	
	if((semctl(semid,0,IPC_RMID)) == -1)
	{
		perror("semctl rmid");
		exit(1);
	}
	return 0;
}
