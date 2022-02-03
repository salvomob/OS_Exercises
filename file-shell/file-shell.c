#include<stdio.h>
#include<stdlib.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<dirent.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>

#define DIM 1024
#define FATHER 1000

typedef enum
{
	NUM_FILES,
	TOT_SIZE,
	SEARCH_CHAR,
	STOP,
	DATA,
	END
}msg_payload;

typedef struct
{
	long type;
	char buffer[DIM];
	msg_payload payload;
	long number;
	char c;
}msg;


void clean(msg message)
{
	for(int i = 0; i < DIM; i++)
		message.buffer[i] = 0;
}

void child(int msgid,char* directory,int num)
{
	msg message;
	struct stat statbuf;
	struct dirent *entry;
	DIR *dp;
	int fd;
	char *map;
	if(chdir(directory) == -1)
	{
		perror("chdir");
		exit(1);
	}
	int done = 0;
	while(!done)
	{
		int counter = 0;
		long total = 0;
		char c = 0;
		if((msgrcv(msgid,&message,sizeof(message),num,0)) == -1)
		{
			perror("msgrcv");
			exit(1);
		}
		switch(message.payload)
		{
			case STOP:
				done = 1;
				message.payload = END;
				message.type = FATHER;
				clean(message);
				message.number = 0;
				if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				break;
			case NUM_FILES:
				 
				if((dp = opendir(directory)) == NULL)
				{
					perror("opendir");
					exit(1);
				}
				while((entry = readdir(dp)) != NULL)
				{
					lstat(entry->d_name,&statbuf);
					if(S_ISREG(statbuf.st_mode))
						counter++;
				}
				message.type = FATHER;
				message.payload = DATA;
				clean(message);
				message.number = counter;
				if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				break;
			case TOT_SIZE:
				
				if((dp = opendir(directory)) == NULL)
				{
					perror("opendir");
					exit(1);
				}
				while((entry = readdir(dp)) != NULL)
				{
					lstat(entry->d_name,&statbuf);
					if(S_ISREG(statbuf.st_mode))
						total+=statbuf.st_size;
				}
				message.type = FATHER;
				message.payload = DATA;
				clean(message);
				message.number = total;
				if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				break;
			case SEARCH_CHAR:
				c = message.c;
				lstat(message.buffer,&statbuf);
				if(!S_ISREG(statbuf.st_mode))
				{	
					printf("ERROR WITH %s\n",message.buffer);
					exit(1);
				}
				fd = open(message.buffer,0660);
				if(fd == -1)
				{
					perror("open");
					exit(1);
				}
				if((map = mmap(NULL,statbuf.st_size,PROT_READ,MAP_PRIVATE,fd,0)) == MAP_FAILED )
				{
					perror("mmap");
					exit(1);
				}
				for(int i = 0; i < statbuf.st_size; i++)
				{
					if(map[i] == c)
						counter++;
				}
				message.type = FATHER;
				message.payload = DATA;
				clean(message);
				message.number = counter;
				if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				munmap(map,statbuf.st_size);
				close(fd);
				break;
		}
	}
	exit(0);
}

int main(int argc, char* argv[])
{
	int children = argc-1;
	if(argc < 2)
	{
		printf("Baf usage -> %s <dir> ... <dir>\n",argv[0]);
		exit(1);
	}
	msg message;
	int msgid;
	
	if((msgid = msgget(IPC_PRIVATE,IPC_CREAT | 0660)) == -1)
	{
		perror("msgget");
		exit(1);
	}
	clean(message);
	message.number = 0;
	int done = 0;
	for(int i = 1; i <= children; i++)
	{
		if(fork() == 0)
			child(msgid,argv[i],i);
	}
	while(!done)
	{
		printf("file-shell>");
		char line[DIM];
		fgets(line,DIM,stdin);
		int len = strlen(line);
		line[len-1] = '\0';
		char *token;
		if((token = strtok(line," ")) == NULL)
		{
			perror("strtok");
			exit(1);
		}
		if(!strcmp(token,"num-files"))
		{
			if((token = strtok(NULL,"\0")) == NULL)
			{
				perror("strtok");
			}
			int num = atoi(token);
			message.payload = NUM_FILES;
			message.type = num;
			if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
			{
				perror("msgsnd");
				exit(1);
			}
		//	sleep(1);
			if((msgrcv(msgid,&message,sizeof(msg),FATHER,0)) == -1)
			{
				perror("msgrcv");
				exit(1);
			}
			if(message.payload != DATA)
			{
				printf("ERROR IN CHAIN DATA MISSING(NUM-FILES)\n");
				exit(1);
			}
			else{
				printf("%ld files\n",message.number);
			}
		}
		if(!strcmp(token,"clear"))
		{
			system("clear");
		}
		if(!strcmp(token,"total-size"))
		{
			if((token = strtok(NULL,"\0")) == NULL)
			{
				perror("strtok");
			}
			int num = atoi(token);
			message.payload = TOT_SIZE;
			message.type = num;
			if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
			{
				perror("msgsnd");
				exit(1);
			}
		//	sleep(1);
			if((msgrcv(msgid,&message,sizeof(msg),FATHER,0)) == -1)
			{
				perror("msgrcv");
				exit(1);
			}
			if(message.payload != DATA)
			{
				printf("ERROR IN CHAIN DATA MISSING(TOT-SIZE)\n");
				exit(1);
			}
			else{
				printf("%ld bytes\n",message.number);
			}
		}
		if(!strcmp(token,"search-char"))
		{
			token = strtok(NULL," ");
			int num = atoi(token);
			token = strtok(NULL," ");
			for(int i = 0; i <= strlen(token);i++)
				message.buffer[i] = token[i];
			token = strtok(NULL,"\0");
			char c = token[0]; 
			//printf("c : %c\nbuffer : %s\n",c,message.buffer);
			message.c = c;
			message.type = num;
			message.payload = SEARCH_CHAR;
			if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
			{
				perror("msgsnd");
				exit(1);
			}
			if((msgrcv(msgid,&message,sizeof(msg),FATHER,0)) == -1)
			{
				perror("msgrcv");
				exit(1);
			}
			if(message.payload != DATA)
			{
				printf("ERROR IN CHAIN DATA MISSING(TOT-SIZE)\n");
				exit(1);
			}
			else{
				printf("%ld\n",message.number);
			}
			 
		}
		if(!strcmp(token,"exit"))
		{
			done = 1;
		}	
	}
	
	for(int i = 1; i <= children; i++)
	{
		message.payload = STOP;
		message.type = i;
		if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
		{
			perror("msgsnd");
			exit(1);
		}
	}
	for(int i = 0; i < children; i++)
		wait(NULL);
	if((msgctl(msgid,IPC_RMID,NULL)) == -1)
	{
		perror("msgctl");
		exit(1);
	}	
	return 0;
}

