#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#define MAX_LEN 64
#define ERROR (char*) "ERROR"

enum SEM_TYPE
{
	S_MUTEX,
	READER,
	WRITER,
	FATHER
};

typedef struct
{
	char buffer[MAX_LEN];
	char type; //1 ok 2 non ok
} shd_mem;

int WAIT(int sem_des, short unsigned int sem_num) {
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_des, ops, 1);
}

int SIGNAL(int sem_des, short unsigned int sem_num) {
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_des, ops, 1);
}

void clean(shd_mem mem)
{
	for(int i = 0; i < MAX_LEN;	i++)
		mem.buffer[i] = '\0';
	mem.type = 0;	
}

int isPal(char *str)
{
	int x = 1;
	int len = strlen(str) - 1;
	
	for(int i = 0; i <= len/2;i++){
		if(str[i] != str[len-1-i]) 
		{
			//printf("%c & %c\n",str[i],str[len-1-i]);
			x = -1;
		}
	}
	return x;
}	

void reader(char *filename,shd_mem *shared_mem,int sem_des)
{
	
	char *str = malloc(MAX_LEN*sizeof(char));
	FILE *file;
	printf("READER ON %s\n",filename);
	if((file=fopen(filename,"r"))==NULL)
	{
		perror("fopen");
		exit(1);
	}
	while((str = fgets(str,MAX_LEN,file)))
	{
		
		WAIT(sem_des,READER);
		WAIT(sem_des,S_MUTEX);
		memcpy(shared_mem->buffer,str,strlen(str));
		shared_mem->type = 1;
		sleep(1);
		//printf("string : %s \ntype : %d\n",shared_mem->buffer,shared_mem->type);
		SIGNAL(sem_des,S_MUTEX);
		SIGNAL(sem_des,FATHER);
	}

	WAIT(sem_des,READER);
	WAIT(sem_des,S_MUTEX);
	memcpy(shared_mem->buffer,"end\0",4);
	shared_mem->type = 2;	
	SIGNAL(sem_des,S_MUTEX);
	SIGNAL(sem_des,FATHER);	
	if(fclose(file) == -1)
	{
		perror("fclose");
		exit(1);
	}
	printf("READER ENDED\n");
	exit(0);
}
void writer(char *filename,shd_mem *shared_mem,int sem_des)
{
	FILE *file;
	printf("WRITER ON %s\n",filename);
	int fd;
	if((fd = open(filename,O_CREAT | O_TRUNC | O_RDWR )) == -1)
	{
		perror("open");
		exit(1);
	}
	if((file = fdopen(fd,"w+"))==NULL)
	{
		perror(filename);
		exit(1);
	}
    int done = 0;
    
	while(!done)//!strcmp("END",shared_mem->buffer)
	{
		WAIT(sem_des,WRITER);
		WAIT(sem_des,S_MUTEX);
		//printf("WRITER ON %s\n",filename);
		//if(isPal(shared_mem->buffer))
		for(int i = 0; i < strlen(shared_mem->buffer);i++)
			if(shared_mem->buffer[i] == '\n') shared_mem->buffer[i] = '\0';
			if(shared_mem->type == 1)
			fprintf(file,"%s\n",shared_mem->buffer);			
		if(shared_mem->type == 2) done = 1;
		sleep(1);
		SIGNAL(sem_des,S_MUTEX);
		SIGNAL(sem_des,READER);
	}
	if(fclose(file) == -1)
	{
		perror("fclose");
		exit(1);
	}
	
	printf("WRITER ENDED\n");
	exit(0);
}

int main(int argc,char* argv[])
{
	if(argc<2)
	{
		fprintf(stdout,"bad usage : %s <file> <file>(can be null)\n",argv[0]);
		exit(1);
	}
	shd_mem *mem;
	int shm_des,sem_des;
	if ((shm_des = shmget(IPC_PRIVATE , sizeof(shd_mem), IPC_CREAT | 0660)) ==-1) 
	{
        perror("shmget");
        exit(1);
    }
    if ((mem = (shd_mem *)shmat(shm_des, NULL, 0)) == (shd_mem *)-1) {
        perror("shmat");
        exit(1);
    }
    if ((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | 0600)) == -1) {
        perror("semget");
        exit(1);
    }

    if (semctl(sem_des, S_MUTEX, SETVAL, 1) == -1) {
        perror("semctl SETVAL S_MUTEX");
        exit(1);
    }
    if (semctl(sem_des, READER, SETVAL, 1) == -1) {
        perror("semctl SETVAL READER");
        exit(1);
    }
    if (semctl(sem_des, WRITER, SETVAL, 0) == -1) {
        perror("semctl SETVAL WRITER");
        exit(1);
    }
     if (semctl(sem_des, FATHER, SETVAL, 0) == -1) {
        perror("semctl SETVAL FATHER");
        exit(1);
    }
	
	int done = 0;
	
	if(fork() == 0)
		reader(argv[1],mem,sem_des);
	
	if(fork() == 0)
    	writer(argv[2],mem,sem_des);	
	while(mem->type != 2)
	{
		WAIT(sem_des,FATHER);
		WAIT(sem_des,S_MUTEX);
		if(isPal(mem->buffer) == -1)
		{
			mem->type = 3;
			printf("not pal\n"); 
			//memcpy(mem->buffer,ERROR,sizeof(char*));
			
		}
		else
		{
			printf("SENT PAL\n");
		}
		SIGNAL(sem_des,S_MUTEX);
		SIGNAL(sem_des,WRITER);
	}
	wait(NULL);
	wait(NULL);
	
	
		  
    shmctl(shm_des, IPC_RMID, NULL);
    semctl(sem_des, 0, IPC_RMID, 0);
}
