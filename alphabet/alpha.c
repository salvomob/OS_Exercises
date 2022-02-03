#include<stdio.h>
#include<stdlib.h>
#include<sys/msg.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>


#define DIM 2048
#define ALPH 26
#define FATHER 1000
#define COUNTER 2000

typedef enum
{
	START,
	STRING,
	NUMBERS,
	STOP,
	END
}msg_payload; 

typedef struct
{
	long type;
	char buffer[DIM];
	msg_payload payload;
	int occ[ALPH];
	int num;
}msg;

void reader(int msgid1,int msgid2,char *filename,int num)
{
	msg message;
	FILE *file;
	char str[DIM];
	int occ[ALPH];
	if((file = fopen(filename,"r")) == NULL)
	{
		perror("fopen");
		exit(1);
	}
	if((msgrcv(msgid1,&message,sizeof(msg),num,0)) == -1)
	{
		perror("msgrcv");
		exit(1);
	}
	if(message.payload != START)
	{
		printf("error in chain, START missing P %d\n",num);
		exit(1);
	}
	while((fgets(str,DIM,file))!= NULL)
	{
		int len = strlen(str);
		str[len-1] = '\0';
		for(int i = 0; i < len; i++)
		{
			message.buffer[i] = str[i];
		}
		message.type = COUNTER;
		message.payload = STRING;
		message.num = num;
		if((msgsnd(msgid2,&message,sizeof(msg),0)) == -1)
		{
			perror("msgsnd STRING");
			exit(1);
		}
		if((msgrcv(msgid2,&message,sizeof(msg),num,0)) == -1)
		{
			perror("msgrcv STRING");
			exit(1);
		}
		if(message.payload != NUMBERS)
		{
			printf("error in chain, NUMBERS missing P %d\n",num);
			exit(1);
		}
		else
		{
			for(int i = 0 ; i < ALPH; i++)
			{
				occ[i] += message.occ[i];
				//printf("occ[%d] = %d\n",i,occ[i]);
				message.occ[i] = 0;
			}
		}
	}
	if((msgrcv(msgid1,&message,sizeof(msg),num,0)) == -1)
	{
		perror("msgrcv");
		exit(1);
	}
	if(message.payload != STOP)
	{
		printf("error in chain, missing STOP P %d\n",num);
		exit(1);
	}
	message.payload = END;
	message.type = FATHER;
	for(int i = 0 ; i < ALPH; i++)
	{
		message.occ[i] = occ[i];
	}
	if((msgsnd(msgid1,&message,sizeof(msg),0)) == -1)
	{
		perror("msgsnd");
		exit(1);
	}
	exit(0);
}

void counter(int msgid)
{
	msg message;
	int occ[ALPH];
	char str[DIM];
	int num = 0;
	if((msgrcv(msgid,&message,sizeof(msg),COUNTER,0)) == -1)
	{
		perror("msgrcv C");
		exit(1);
	}
	if(message.payload != START)
	{
		printf("error in chain C missed START\n");
		exit(1);
	}
	int done = 0;
	while(!done)
	{
		for(int i = 0 ; i < ALPH;i++)
			message.occ[i] = 0;
		if((msgrcv(msgid,&message,sizeof(msg),COUNTER,0)) == -1)
		{
			perror("msgrcv C");
			exit(1);
		}
		switch(message.payload)
		{
			case STOP:
				done = 1;
				break;
			case STRING:
				num = message.num;
				message.type = num;
				int len = strlen(message.buffer);
				for(int i = 0; i < len; i++)
				str[i] = tolower(message.buffer[i]);
				for(int i = 0 ; i < len; i++)
				{
					if(str[i] < 'a' || str[i] > 'z')
						continue;
					else
					{
						int offset = ((int) str[i] - (int) 'a');
						message.occ[offset]++;	
					}	
				}
				message.payload = NUMBERS;
				if((msgsnd(msgid,&message,sizeof(msg),0)) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				break;
			default :
				break;	
		}
				
	}
	exit(0);
}

int main(int argc,char* argv[])
{
	if(argc < 2)
	{
		printf("Bad usage -> %s <file-1> ... <file-n>\n",argv[0]);
		exit(1);
	}
	int children = argc-1;
	int msgid1, msgid2;
	msg message;
	int occ[ALPH];
	for(int i = 0 ; i < ALPH; i++)
	{
		message.occ[i] = 0;
		occ[i] = 0;
	}
	if((msgid1 = msgget(IPC_PRIVATE,IPC_CREAT | 0660)) == -1)
	{
		perror("msgget 1");
		exit(1);
	}
	if((msgid2 = msgget(IPC_PRIVATE,IPC_CREAT | 0660)) == -1)
	{
		perror("msgget 1");
		exit(1);
	}
	for(int i = 1 ; i <= children; i++)
	{
		if(fork() == 0)
		{
			reader(msgid1,msgid2,argv[i],i);
		}
	}
	if(fork() == 0)
	{
		counter(msgid2);
	}
	for(int i = 1 ; i <= children; i++)
	{
		message.type = i;
		message.payload = START;
		msgsnd(msgid1,&message,sizeof(msg),0);
	}
	message.type = COUNTER;
	message.payload = START;
	msgsnd(msgid2,&message,sizeof(msg),0);
	sleep(1);
	for(int i = 1 ; i <= children; i++)
	{
		message.type = i;
		message.payload = STOP;
		msgsnd(msgid1,&message,sizeof(msg),0);
	}
	message.type = COUNTER;
	message.payload = STOP;
	msgsnd(msgid2,&message,sizeof(msg),0);
	for(int i = 0; i < children; i++)
	{
		if((msgrcv(msgid1,&message,sizeof(msg),FATHER,0)) == -1)
		{
			perror("msgrcv");
			exit(1);
		}
		if(message.payload == END)
		{
			for(int j = 0 ; j < ALPH;j++)
				occ[j] += message.occ[j];	
		}
		else
			printf("ERROR in chain, missing END\n"); 
		
	}
	for(int i = 0 ; i < children; i++)
		wait(NULL);
	wait(NULL);
	for(int i = 0; i < ALPH; i++)
	printf("%c = %d\n",(char) 'a'+i,occ[i]);
	if((msgctl(msgid1,IPC_RMID,NULL)) == -1)
	{
		perror("msgctl 1");
		exit(1);
	}
	if((msgctl(msgid2,IPC_RMID,NULL)) == -1)
	{
		perror("msgctl 1");
		exit(1);
	}
	printf("end\n");
	return 0;
}
