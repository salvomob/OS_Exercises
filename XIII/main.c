#define _GNU_SOURCE 

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>

#define MAX_LEN 512
#define FILTERER 1000

typedef enum
{
	NORMAL,
	INVERSE,
	INSENSITIVE,
	DATA_SENT,
	BOTH,
	ACK,
	R_END,
	F_END,	
} msg_payload;

typedef struct
{
	long type;
	char buffer[MAX_LEN];
	msg_payload payload;
	long number;
}msg;


void reader(int msgid,char *filename,int num,int op1,int op2)
{
	FILE *file;
	msg message;
	if((file = fopen(filename,"r")) == NULL)
	{
		perror(filename);
		exit(1);
	}
	if(op1 && op2)
	{
		message.payload = BOTH;
	}			
	else if(op1)
	{
		message.payload = INVERSE;
	}
	else if(op2)
	{
		message.payload = INSENSITIVE;	
	}
	else if(!op1 && !op2)
	{
		message.payload = NORMAL;			
	}
	for(int i  = 0; i < MAX_LEN;i++)
		message.buffer[i] = 0;
	message.type = FILTERER;	
	message.number = num;
	//first message send with the payload
	
	if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
	{
		perror("1 R msgsnd");
		exit(1);
	}
	
	//recieving ACK
	
	if((msgrcv(msgid,&message,sizeof(msg),num,0)) == -1)
	{
		perror("1 R msgrcv");
		exit(1);
	}
	if(message.payload != ACK)
	{
		fprintf(stderr,"ERROR IN MESSAGE QUEUE\n");
		exit(1);
	}
	else{
		//printf("ACK recieved\n");
	}
	
	
	char str[MAX_LEN];
	while((fgets(str,MAX_LEN,file))!=NULL)
	{
		message.type = FILTERER;
		message.payload = DATA_SENT;
		int len = strlen(str);
		str[len-1] = 0;
		strcpy(message.buffer,str);
		if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
		{
			perror("msgsnd");
			exit(1);
		}
	}
	
	//END OK
	
	message.payload = R_END;
	message.type = FILTERER;
	if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
	{
		perror("3 R msgsnd");
  	 	exit(1);
	}
	//printf("R_END sent\n");
	if((msgrcv(msgid,&message,sizeof(msg),num,0)) == -1)
	{
		perror("2 R msgrcv");
		exit(1);
	}
	if(message.payload != F_END)
	{
		fprintf(stderr,"ERROR IN MESSAGE QUEUE ON END F\n");
		exit(1);
	}
	fclose(file);
	exit(0);
}

void filterer(int msgid,char *word,char* filename,int fd[2])
{
	msg message;
	int mode;//0 normal , 1 -i , 2 -v ,3 -i -v
	close(fd[0]);
	char buff[MAX_LEN];
	if((msgrcv(msgid,&message,sizeof(msg),FILTERER,0)) == -1)
	{
		perror("1 F msgrcv");
		exit(1);
	}
	int num = message.number;
	int done = 0;
	for(int i = 0; i < MAX_LEN; i++)
		message.buffer[i] = 0;
	//printf("file : %s\n",filename);
	switch(message.payload)
	{
		case NORMAL:
			message.payload = ACK;
			//printf("NORMAL F %d\n",num);
			message.type = num;
			mode = 0;
			if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
			{
				perror("1 F msgsnd");
		  	 	exit(1);
			}
			break;		
		case BOTH:
			message.payload = ACK;
			//printf("BOTH\n");
			message.type = num;
			mode = 3;
			if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
			{
				perror("1 F msgsnd");
		  	 	exit(1);
			}
			break;	
		case INVERSE:
			message.payload = ACK;
			//printf("INVERSE\n");
			message.type = num;
			mode = 2;
			if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
			{
				perror("1 F msgsnd");
		  	 	exit(1);
			}
			break;
		case INSENSITIVE:
			message.payload = ACK;
			//printf("INSENSITIVE\n");
			message.type = num;
			mode = 1;
			if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
			{
				perror("1 F msgsnd");
		  	 	exit(1);
			}
			break;
	}
	//here switch on mode
	while(!done)
	{
		if((msgrcv(msgid,&message,sizeof(msg),FILTERER,0)) == -1)
		{
			perror("1 F msgrcv");
			exit(1);
		}
		if(message.payload == DATA_SENT)
		{
			//printf("recieved : %s\n",message.buffer);
			switch(mode)
			{
				case 0:
					if(strstr(message.buffer,word) != NULL)
					{
						printf("%s:%s\n",filename,message.buffer);
					}
					//pipe it
					break;
				case 1:
					if(strcasestr(message.buffer,word) != NULL)
					{
						printf("occurred in INSENSITIVE\n");
					}
					//pipe it
					break;
				case 2:
					if(strstr(message.buffer,word) == NULL)
					{
						printf("not occurred -> INVERSE OK\n");
					}
					//pipe it
					break;
				case 3:
					if(strcasestr(message.buffer,word) == NULL)
					{
						printf("not occurred -> INVERSE OK\n");
					}
					//pipe it
					break;
			}	
		}
		if(message.payload == R_END)
		{
			//printf("F_END RECIEVED\n");
			message.payload = F_END;
			message.type = num;
			if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
			{
				perror("msgsnd");
				exit(1);
			}
			done = 1;
		}
	}
	
	//END
	
	//printf("filterer ended\n");
	exit(0);
}

int main(int argc, char* argv[])
{
	
	if(argc < 3)
	{
		fprintf(stderr,"Bad usage -> %s [options] <file 1> <file 2> ...\n",argv[0]);
		exit(1);
	}
	char *word;
	msg message;
	int msgid;
	int fd[2];
	int num;
	if((msgid = msgget(IPC_PRIVATE,IPC_CREAT | 0777)) == -1)
	{
		perror("msgget");
		exit(1);
	}
	
	if((pipe(fd)) == -1)
	{
		perror("pipe");
		exit(-1);
	}
	
	if(strcmp(argv[1],"-v") && strcmp(argv[1], "-i"))
	{
		//printf("no options put\n");
		num = argc -2;
		//printf("n files = %d\n", num);
		//NORMAL
		for(int i = 1; i <= num; i++)
		{
			sleep(1);
			if(fork() == 0)
			{
				reader(msgid,argv[1+i],i,0,0);
			}
			if(fork() == 0)
			{
				filterer(msgid,argv[1],argv[i+1],fd);
			}
		}
		
	}
	
	if(!strcmp(argv[1],"-i"))
	{
		if(strcmp(argv[2],"-v"))
		{
			//printf("only -i\n");
			num = argc -3;
			//printf("n files = %d\n", num);
			
		}
	}
	
	if(!strcmp(argv[1],"-v"))
	{
		if(strcmp(argv[2],"-i"))
		{
			printf("only -v\n");
			num = argc -3;
			printf("n files = %d\n", num);
		}
	}
	
	if(!strcmp(argv[1],"-v") || !strcmp(argv[1],"-i"))
	{
		if(!strcmp(argv[2],"-i") || !strcmp(argv[2],"-v"))
		{
			printf("both\n");
			num = argc -4;
			printf("n files = %d\n", num);
		}
	}  
	
	
	/*if(fork() == 0)
	{
		reader(msgid,argv[2],1,0,0);
	}
	else if(fork() == 0)
	{
		filterer(msgid,argv[1],1,fd);
	}*/
	for(int i = 0; i < num+1; i++)
	{
		wait(NULL);
		wait(NULL);
	}
	msgctl(msgid,IPC_RMID,NULL);
	
}
	
