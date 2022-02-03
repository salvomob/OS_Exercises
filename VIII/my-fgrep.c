/*

da sistemare o rifare

*/


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<sys/ipc.h>
#include<fcntl.h>
#include<sys/sem.h>
#include<sys/wait.h>

#define DIM 2048


void reader(int fd[2],char *filename)
{
	int f;
	FILE *file,*out;
	if((f = open(filename,O_RDONLY)) == -1)
	{
		perror(filename);
		exit(1);
	}
	if((out = fdopen(fd[1],"w")) == NULL)
	{
		perror("fdopen");
		exit(1);
	}
	if((file = fdopen(f,"r")) == NULL)
	{
		perror("fdopen");
		exit(1);
	}
	
	char line[DIM];
	sleep(1);
	printf("READER STARTS\n");
	while(fgets(line,DIM,file)!=NULL)
	{

		fputs(line,out);
		fprintf(stdout,"passing :%s\n",line);
	}
	char stop[5] = {'s','t','o','p','\0'};
	fputs(stop,out);
	fprintf(stdout,"passing :%s\n",stop);
	printf("READER ENDED\n");
	exit(0);
}

void filterer(char *file,int fd[2])
{	
	char line[DIM];
	int fifo;
	int done = 0;
	printf("FILTERER STARTS\n");
	while(!done)
	{
		close(fd[1]);
		read(fd[0],line,sizeof(char)*DIM);
		fprintf(stdout,"got :%s\n",line);
		close(fd[0]);
		if(!strcmp(line,"stop")) done = 1;

		if ((fifo = open("pipe",O_WRONLY)) == -1)
		{
			perror("open");
			exit(1);
		}
		if( write(fifo,line,strlen(line)) == -1)
		{
			printf("ERROR on %s\n",line);
			exit(1);
		}
		else 
		{
			printf("sent:%s\n",line);
		}
		close(fifo);
	}	
	printf("FILTERER ENDED\n");
	exit(0);
}

void writer(char *file)
{
	int done = 0;
	int fifo;
	char line[DIM];
	printf("WRITER STARTS\n");
	while(!done)
	{
		sleep(1);
		if((fifo = open("pipe", O_RDONLY)) == -1)
		{
			perror("fifo not working");
			exit(1);
		}
		read(fifo,line,DIM);
		if(!strcmp(line,"stop")) done = 1;
		printf("WRITER : %s\n",line);
		close(fifo);
	}
	printf("WRITER ENDS\n");
	exit(0);
}

int main(int argc,char *argv[])
{
	//0 read 1 write for pipe
	int fd[2];
	int fifo;
	int semid;
	if(argc < 3)
	{
		fprintf(stderr,"Bad usage-> %s [params] <string> [file(can also be null -> stdin)]\n",argv[0]);
		exit(1);
	}
	
	if(pipe(fd) == -1)
	{
		perror("pipe");
		exit(1);
	}
	
	if((fifo = mkfifo("pipe",0777)) == 1)
	{
		perror("mkpipe");
		exit(1);
	}
	
	
	if(fork() == 0)
	{
		reader(fd,argv[3]);	
	}
	else if(fork() == 0)
	{
		filterer("pipe",fd);
	}
	if(fork() == 0)
	{
		writer("pipe");
	}
	
	for(int i = 0; i < 3 ; i++)
	{
		wait(NULL);
	}
	
	semctl(semid, 0, IPC_RMID, 0);
	
	
}
