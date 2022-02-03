#include<stdio.h>
#include<ctype.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/sem.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>


typedef struct
{
	char c;
	char eof;
} shared_mem;

enum SEM_TYPE { 
	MUTEX,
	READER,
	FATHER 
};

int WAIT(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
	return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
	return semop(sem_des, operazioni, 1);
}

void child(int shmid,int semid,FILE *file)
{
 	shared_mem *p;
    int c;
    
    if ((p = (shared_mem *) shmat(shmid, NULL, 0)) == (shared_mem *) -1) {
        perror("shmat");
        exit(1);
    }
    
    p->eof = 0;
    while ((c = fgetc(file)) != EOF) {
        WAIT(semid, READER);
        p->c = (char)c;
        SIGNAL(semid, FATHER);
    }

    WAIT(semid, READER);
    p->eof = 1;
    SIGNAL(semid, FATHER);
    
    exit(0);
}

int main(int argc, char * argv[])
{
	shared_mem *mem;
	int shmid,semid,fd;
	char c;
	FILE *file;
	if(argc != 2)
	{
		fprintf(stderr,"bad usage -> %s <filename>\n",argv[0]);
		exit(1);
	}
	
	if((fd = open(argv[1], O_RDONLY)) == -1)
	{
		perror("open");
		exit(1);
	}
	
	if((file = fdopen(fd,"r")) == NULL)
	{
		perror("fdopen");
		exit(1);
	}
	
	if ((shmid = shmget(IPC_PRIVATE , sizeof(shared_mem), IPC_CREAT | 0660)) ==-1) 
	{
        perror("shmget");
        exit(1);
    }
    
    if((semid = semget(IPC_PRIVATE , 3 , IPC_CREAT | 0660)) == -1)
    {
    	perror("semget");
    	exit(1);
    }
    
    if ((mem = (shared_mem *)shmat(shmid, NULL, 0)) == (shared_mem *)-1) {
        perror("shmat");
        exit(1);
    }
	
	if (semctl(semid, MUTEX, SETVAL, 1) == -1) {
        perror("semctl SETVAL MUTEX");
        exit(1);
    }
    
    if (semctl(semid, READER, SETVAL, 1) == -1) {
        perror("semctl SETVAL READER");
        exit(1);
    }
    
    if (semctl(semid, FATHER, SETVAL, 0) == -1) {
        perror("semctl SETVAL FATHER");
        exit(1);
    }
    int words = 0,lines = 0,characters = 0;
    int done = 0; 
    if((fork() == 0))
    {
    	child(shmid,semid,file);
    }
    else
    {
   		printf("FATHER\n");
    	while(mem->eof != 1)
    	{
			WAIT(semid,MUTEX);
	 		WAIT(semid,FATHER);
	 		printf("recieved : %c\n", mem->c);
	 		if(mem->c != '\0') characters++;
	 		if(mem->c == '\n' || mem->c == '\r') lines++;
	 		if(isspace(mem->c) || mem->c == ';' || mem->c == ',' || mem->c == '.' || mem->c == ':') words++; 
	 		SIGNAL(semid,MUTEX);
	 		SIGNAL(semid,READER);
    	}
    	printf("FATHER ENDS\n");		   	
    }
    

    
	fprintf(stdout,"%s : %d\t%d\t%d\n",argv[1],characters,words,lines);
	
	if((semctl(semid,0,IPC_RMID,0)) == -1)
	{
		perror("sectl IPC_RMID");
		exit(1);
	}
	if((shmctl(shmid,IPC_RMID,0)) == -1)
	{
		perror("shmctl IPC_RMID");
		exit(1);
	}
	return 0;
}
