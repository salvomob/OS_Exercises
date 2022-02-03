#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>

#define MAX_LEN_WORD 64
#define SORTER 1000
#define COMPARER 2000


typedef enum
{
	S_S1,
	S_S2,
	ACK,
	DATA_SENT,
	S_END,
	C_END,
} msg_payload;

typedef struct
{
	long type;
	char buffer[MAX_LEN_WORD];
	msg_payload payload;
	long number;
} msg;


void scambia(char **v, int i, int j){
	char* tmp = v[j];
	//printf("BEFORE : %s\t%s\n",v[i],v[j]);
	v[j] = v[i];
	v[i] = tmp;
	//printf("AFTER : %s\t%s\n",v[i],v[j]);
}


void sorter(int msgid,int fd[2],char* filename)
{
	//printf("Sorter started\n");
	msg message;
	FILE *file;
	char tmp[MAX_LEN_WORD];
	int n = 0;
	if((file = fopen(filename,"r")) == NULL)
	{
		perror("fopen");
		exit(1);
	}
	
	//preliminari : estraggo il numero di elementi dalla lista e riavvio lo stream
	while((fgets(tmp,MAX_LEN_WORD,file))!= NULL)
	{
		n++;
	}
	rewind(file);
	//printf("n = %d\n",n);
	char *arr[n];
	for(int i = 0; i < n; i++)
	{
		arr[i] = malloc(MAX_LEN_WORD * sizeof(char));
	}
	int i = 0;
	while((fgets(tmp,MAX_LEN_WORD,file))!= NULL)
	{
		strcpy(arr[i],tmp);
		int len = strlen(tmp);
		arr[i][len-1] = '\0';
		i++; 
	}
	/*for(int j = 0; j < n; j++)
	{
		printf("arr[%d] = %s\n",j,arr[j]);
	}*/
	for(i=0;i<n;i++){
		int j=i;
		while(j>0){
			//v[j-1]>v[j]
			message.type = COMPARER;
			message.payload = S_S1;
			strcpy(message.buffer,arr[j-1]);
			msgsnd(msgid,&message,sizeof(msg),0);
		//	printf("SENT S_S1\n");
			msgrcv(msgid,&message,sizeof(msg),SORTER,0);
			if(message.payload == ACK)
			{
				message.type = COMPARER;
				strcpy(message.buffer,arr[j]);
				message.payload = S_S2;
				msgsnd(msgid,&message,sizeof(msg),0);
			//	printf("SENT S_S2\n");
			}
			msgrcv(msgid,&message,sizeof(msg),SORTER,0);
			if(message.payload != DATA_SENT)
				exit(1);	
			if(message.number < 0)
				break;		
			scambia(arr,j,j-1);
			j--;		
		}
	}
	
	message.payload = S_END;
	message.type = COMPARER;
	msgsnd(msgid,&message,sizeof(msg),0);
	msgrcv(msgid,&message,sizeof(msg),SORTER,0);
	if(message.payload == C_END)
	{
		/*printf("SORTER ENDED SUCCESSFULLY\n");
		for(int j = 0; j < n; j++)
		{
			printf("arr[%d] = %s\n",j,arr[j]);
		}*/
		if(fclose(file) == -1)
		{
			perror("fclose");
			exit(1);
		}
	}
	close(fd[0]);
	write(fd[1],&n,sizeof(int));
	//printf("piped %d\n",n);
	
	for(int i = 0; i < n; i++)	
	{
		int len = strlen(arr[i]);
		if(write(fd[1],&len,sizeof(int)) == -1)
		{
			perror("write");
			exit(1);
		}
		//printf("piped %d\n",len);
		write(fd[1],arr[i],len * sizeof(char));
	//	printf("piped %s\n",arr[i]);
	
	}
	close(fd[1]);
	//printf("SORTER ENDED SUCCESSFULLY\n");	
	exit(0);
}

void comparer(int msgid)
{
	char s1[MAX_LEN_WORD], s2[MAX_LEN_WORD];
	msg message;
	int done = 0;
	while(!done)
	{
		if(msgrcv(msgid,&message,sizeof(msg),COMPARER,0) == -1)
		{
			perror("msgrcv C1");
			exit(1);
		}
		switch(message.payload)
		{
			case S_END:
				//printf("S_END GOT\n");
				message.payload = C_END;
				message.type = SORTER;
				msgsnd(msgid,&message,sizeof(msg),0);
				done = 1;
			case S_S1:
				for(int i = 0; i < strlen(message.buffer); i++)
				{
					s1[i] = message.buffer[i];
				}
			//	printf("got %s\n",s1);
				message.type = SORTER;
				message.payload = ACK;
				msgsnd(msgid,&message,sizeof(msg),0);
				break;
			case S_S2:
				for(int i = 0; i < strlen(message.buffer); i++)
				{
					s2[i] = message.buffer[i];
				}
			//	printf("got %s & %s\n",s1,s2);	
				int ret = strcasecmp(s1,s2);
				//printf("ret = %d\n",ret);
				message.number = (long) ret;
				//printf("m.n = %li\n",message.number);
				message.type = SORTER;
				message.payload = DATA_SENT;
				if(msgsnd(msgid,&message,sizeof(msg),0) == -1)
				{
					perror("msgsnd");
					exit(1);
				}
				break;					
		}
	}	
	//printf("COMPARER ENDED SUCCESSFULLY\n");
	exit(0);	
}

int main(int argc,char* argv[])
{
	if(argc!=2)
	{
		fprintf(stdout,"Bad usage -> %s <path-to-filename>\n",argv[0]);
		exit(1);
	}
	int msgid;
	int fds[2];

	if((pipe(fds)) == -1)
	{
		perror("pipe");
		exit(1);
	}
	
	if((msgid = msgget(IPC_PRIVATE,IPC_CREAT | 0660)) == -1)
	{
		perror("msgget");
		exit(1);
	}
		
	if(fork() == 0)
	{
		close(fds[0]);
		sorter(msgid,fds,argv[1]);
	}
	else if(fork() == 0)
	{
		comparer(msgid);
	}
	wait(NULL);
	wait(NULL);
	char buffer[MAX_LEN_WORD];
	for(int i = 0; i < MAX_LEN_WORD;i++)
		buffer[i] = 0;
	int num = 0;
	int x = 0;
	close(fds[1]);
	read(fds[0],&num,sizeof(int));
	//printf("1 read from pipe %d\n",num);
	for(int i = 0; i < num; i++)
	{
		read(fds[0],&x,sizeof(int));
		read(fds[0],buffer,x * sizeof(char));
		printf("%s\n",buffer);
		//sleep(1);
	}
	close(fds[0]);
	msgctl(msgid, IPC_RMID, NULL);   
	return 0;
}
