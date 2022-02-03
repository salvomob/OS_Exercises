#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include<fcntl.h>
#include <sys/mman.h>

#define MAX_PATH 2048
#define PARENT 1000

typedef struct
{
	long type;
	char pathname[MAX_PATH];
	char name[128];
	int number;
	int id;
	int nfiles;
} msg;

void clean_buffers(msg message)
{
	for(int i = 0; i < MAX_PATH;i++)
	{
		message.pathname[i] = '\0';
	}
	for(int i = 0; i < 128;i++)
	{
		message.name[i] = '\0';
	}
	printf("clean\n");
}


char* conc(char s1[],char s2[]){
        unsigned int i1 = strlen(s1);
        unsigned int i2 = strlen(s2);
        unsigned int len = i1+i2;
        char *ret = malloc(len*sizeof(char));
        for(int i = 0 ; i < len;i++){
                while(i < i1) {
                        ret[i] = s1[i];
                        i++;
                }
                while(i>=i1 && i-i1 <= i2 ){
                        ret[i] = s2[i-i1];
                        i++;
                }
        }
        for(int i = 0; i < len; i++)
        	printf("%c",ret[i]);
       	printf("\n");
        return ret;
}



void scanner(int queue)
{	
	msg message;
	clean_buffers(message);
	struct dirent *entry;
	DIR *dir;
	int num = 0;
	if((msgrcv(queue,&message,sizeof(msg) - sizeof(long),1,0)) < 0)
	{
		perror("recieve");
		exit(1);
	}
	printf("SCANNER ON QUEUE %d : %s\n",queue,message.pathname);
	if(message.id ==  0)
	{
		//printf("message recieved:\ntype : %ld\npathname : %s\nnumber : %d\n",message.type,message.pathname,message.number);
		if(chdir(message.pathname) == -1)
		{
			perror(message.pathname);
			exit(1);
		}
		if((dir = opendir(message.pathname)) == NULL)
		{
			perror("opendir");
			exit(1);
		}
		while((entry = readdir(dir))!= NULL)
		{
			clean_buffers(message);
					
			for(int i = 0; i < strlen(entry->d_name);i++)
				message.name[i] = entry->d_name[i];
			msgsnd(queue,&message,sizeof(msg) - sizeof(long), 0);
			printf("SCANNER : %s%s\n",message.pathname,message.name);
		}
	}
	else if(message.id == 1)	
	{
		if(chdir(message.pathname) == -1)
		{
			perror(message.pathname);
			exit(1);
		}
		if((dir = opendir(message.pathname)) == NULL)
		{
			perror("opendir");
			exit(1);
		}
		while((entry = readdir(dir))!= NULL)
		{
			clean_buffers(message);
			for(int i = 0; i < strlen(entry->d_name);i++)
				message.name[i] = entry->d_name[i];
			msgsnd(queue,&message,sizeof(msg) - sizeof(long), 0);
		}
	}
	char *done = "DONE";
	for(int i = 0;  i < 4; i++)
		message.name[i] = done[i];
	msgsnd(queue,&message,sizeof(msg) - sizeof(long), 0);
	printf("SCANNER : %s%s\n",message.pathname,message.name);
	printf("Scanner terminated\n");
	exit(0);
}

void analyzer(int queue)
{

	msg message;

	int counter = 0;
	int done = 0;
	printf("ANALYZER ON QUEUE %d\n",queue);
	sleep(1);
	char *map;
	struct stat statbuf;
	int fd;
	FILE *file;
	while((strcmp(message.name,"DONE")))
	{
		clean_buffers(message);
		counter = 0;
		if((msgrcv(queue,&message,sizeof(msg) - sizeof(long),1,0)) < 0)
		{
			perror("recieve");
		}
		char *new;
		new = conc(message.pathname,message.name);
		
		printf("ANALYZER : %s%s\n",message.pathname,message.name);	
		
		if(chdir(message.pathname) == -1)
		{
			perror("chdir");
			continue;
		}
		
		if ((lstat(new, &statbuf) == -1))
		{
			perror("lsat\n");
			continue;
		}
		if (!S_ISREG(statbuf.st_mode))
		{
			perror("not regular");
			continue;
		}
		if((fd = open(new, 0600)) == -1)
		{
			perror("open");
			continue;
		}
		if ((map = mmap(&map, statbuf.st_size, PROT_READ,MAP_SHARED, fd, 0)) == MAP_FAILED)
		{
			perror("mmap");
			continue;
		}
		for(int i = 0; i < statbuf.st_size; i++ )
		{
			if((map[i] >= 'A' && map[i] <='Z') ||( map[i] >= 'a' && map[i] <='z') )
				counter++;
		}
		//printf("counter : %d\n",counter);
		if((munmap(map,statbuf.st_size)) == -1)
		{
			perror("munmap");
		}
		message.type = PARENT;
		message.number = counter;
		if(msgsnd(queue,&message,sizeof(msg) - sizeof(long), PARENT) == -1)
		{
			perror("SENDER");
			continue;
		}
		
	}
	printf("Analyzer terminated\n");
	exit(0);
}

int main(int argc,char *argv[])
{
	msg message;
	int queue;
	key_t key;
	char cwd[MAX_PATH];
	getcwd(cwd,MAX_PATH);
	message.type = 1;
	message.number = 0;	
	clean_buffers(message);
	if(argc > 2)
	{
		fprintf(stdout,"too many arguments\n");
		exit(1);
	}
	
	if(argc != 1)
	{
		for(int i = 0; i < strlen(argv[1]);i++)
		{
			message.pathname[i] = argv[1][i];
		}
	}
	int spaces;
	if(argc == 1)
	{	
		message.id = 1;
		for(int i = 0; i < strlen(cwd);i++)
		{
			message.pathname[i] = cwd[i];
		}
		
		key = ftok(cwd,'a');
		//printf("type = %ld\npath = %s\n",message.type,message.pathname);
		queue = msgget(key,0600 |IPC_CREAT);
		if((msgsnd(queue,&message,sizeof(msg) - sizeof(long),0)) < 0 )
		{
			perror("send");
			exit(1);
		}
		if( fork() == 0)
		{
			scanner(queue);
		}
		
		else if( fork() == 0)
		{
			sleep(2);
			analyzer(queue);
		}
		
	}
	if(argc == 2)
	{
		message.id = 0;
		key = ftok(argv[1],'a');
		queue = msgget(key,0600 |IPC_CREAT);
		if((msgsnd(queue,&message,sizeof(msg) - sizeof(long),0)) < 0 )
		{
			perror("send");
			exit(1);
		}
		if(fork() == 0)
		{
			scanner(queue);
		}
		
		else if(fork() == 0)
		{
			sleep(2);
			analyzer(queue);
		}
	}
	while(strcmp(message.name,"DONE"))
	{
		msgrcv(queue,&message,sizeof(msg) - sizeof(long), PARENT,0);
		printf("message to parent : %s , %s , %d\n",message.pathname,message.name,message.number);
	}
	wait(NULL);
	wait(NULL);
	msgctl(queue,IPC_RMID,NULL);
}
