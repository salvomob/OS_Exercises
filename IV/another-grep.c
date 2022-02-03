#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/msg.h>
#include <fcntl.h>

#define MAX_LEN 1024
#define WRITER 1000	

typedef struct
{
	char buffer[MAX_LEN];
	long type;
} msg;

void reader(char *filename, int fds[2])
{
	close(fds[0]);
	printf("READER BEGINS\n");
	FILE *file;
	int fd;
	char buff[MAX_LEN];
	if((fd = open(filename,0660)) == -1)
	{
		perror("open");
		exit(1);
	}
	
	if((file = fdopen(fd,"r"))== NULL)
	{
		perror("fdopen");
		exit(1);
	}	
	
	while(fgets(buff,MAX_LEN,file)!=NULL)
	{
		sleep(1);
		int len = strlen(buff);
		buff[len-1] = '\0';	
		int n = len + 1;
		write(fds[1], &n, sizeof(int));
		write(fds[1],&buff,n*(sizeof(char)));
		//printf("string copied : %s\n",buff);
	}
	sleep(1);
	//printf("sent -1\n");
	int stop = -1;
	write(fds[1], &stop, sizeof(int));
	close(fds[1]);
	close(fd);
	printf("END READER\n");
	exit(0);
}

void writer(int queue,pid_t pid)
{
	printf("WRITER BEGINS\n");
	msg message;
	int done = 0;
	while(1)
	{

		if((msgrcv(queue,&message,sizeof(msg) - sizeof(long), 0 ,0)) == -1)
		{
			perror("msgrcv");

		}
		printf("msg type = %ld\n",message.type);
		if(message.type == 2) break;
		printf("MESSAGE RECIEVED : %s\n",message.buffer);
	}
	waitpid(pid,NULL,0);
	printf("WRITER ENDS\n");
	exit(0);
}

int main(int argc,char* argv[])
{
	int pipefd[2];
	int n,queue;
	pid_t r,w;
	pipe(pipefd);
	msg message;
	char buffer[MAX_LEN];
	if((queue = msgget(IPC_PRIVATE, IPC_CREAT|0600)) == -1)
	{
		perror("msgget");
		exit(1);
	}
	if((r = fork()) == 0)
	{
		reader(argv[2],pipefd);		
	}
	else if((w = fork()) == 0)
	{
		writer(queue,r);		
	}
	else
	{
		close(pipefd[1]);
		int done = 0;
		while(1)
		{
			read(pipefd[0], &n , sizeof(int));
			read(pipefd[0], &buffer , n * sizeof(char));
			if( n == -1)
			{
				message.type = 2;
				if((msgsnd(queue,&message, sizeof(msg) - sizeof(long),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				break;
			} 
			if (strstr(argv[1],buffer))
			{
				strcpy(message.buffer,buffer);
				printf("%s\n",message.buffer);
				message.type = 1;
				if((msgsnd(queue,&message, sizeof(msg) - sizeof(long),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
			}
		}
		close(pipefd[0]);
	}
	wait(NULL);
	wait(NULL);	
	if((msgctl(queue,IPC_RMID,NULL) == -1))
	{
		perror("msgctl");
		exit(1);
	}
	
	return 0;
}
