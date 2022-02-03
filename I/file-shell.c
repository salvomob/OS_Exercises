#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>


#define TEXT_DIM 900
#define BUFSIZE 4096
#define DEST_PARENT 1000


typedef enum
{
	CMD_LIST,
	CMD_SIZE,
	CMD_SEARCH,
	CMD_EXIT,
	REPLY_ERROR,
	REPLY_DATA,
	REPLY_STOP
	
} type_payload;

typedef struct
{
	long type;
	type_payload payload;
	char text[TEXT_DIM];
	long number;
} msg;

void child_dir(int num,char *dir,int queue)
{
	msg message;
	struct stat statbuf;
	DIR *dp;
	FILE *file;	
	struct dirent *entry;
	char buffer[BUFSIZE];
	char occurrences;
	char done = 0;
	
	fprintf(stdout,"Child D-%d on directory %s running\n",num,dir);
	//ci spostiamo nella directory puntata da dir
	if(chdir(dir)==-1)
	{
		perror(dir);
		exit(1);
	}
	
	do
	{
		//prendiamo il messaggio
		if(msgrcv(queue,&message,sizeof(msg)-sizeof(long),num,0) == -1)
		{
			perror("msgrcv");
			exit(1);
		}
		
		switch(message.payload)
		{
			case CMD_LIST:
				//legge il contenuto della cartella corrente
				dp = opendir(".");
				message.type = DEST_PARENT;
				//scorro le varie voci della directory
				while((entry = readdir(dp)) != NULL)
				{
					lstat(entry->d_name, &statbuf);
					//file regolare
					if(S_ISREG(statbuf.st_mode))
					{
						//invio di un messaggio per ogni file
						message.payload = REPLY_DATA;
						strncpy(message.text, entry->d_name, TEXT_DIM);
                        msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
					}
				}
				//invio messaggio di fine lista
				message.payload = REPLY_STOP;
                msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
                closedir(dp);
                break;
                
           case CMD_SIZE:
           		message.type = DEST_PARENT;
                if ((lstat(message.text, &statbuf) == -1) || (!S_ISREG(statbuf.st_mode))) {
                    message.payload = REPLY_ERROR;
                    msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
                    break;
                }
                /* invia un messaggio con la dimensione in byte */
                message.payload = REPLY_DATA;
                message.number = statbuf.st_size;
                msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
                break;
                
         	case CMD_SEARCH:
         		  /* si fanno una serie di controlli sul file e
                   poi si tenta di aprirlo in lettura         */
                if ((lstat(message.text, &statbuf) == -1) || (!S_ISREG(statbuf.st_mode)) || ((file = fopen(message.text, "r")) == NULL)) {
                    /* in caso di problemi si invia un messaggio di tipo REPLY_ERROR */
                    message.type = DEST_PARENT;
                    message.payload = REPLY_ERROR;
                    msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
                    
                    /* si estrae il secondo messaggio per pulire la coda */
                    msgrcv(queue, &message, sizeof(msg) - sizeof(long), num, 0);
                    break;
                }
                
                /* riceve il secondo messaggio di tipo CMD_SEARCH
                   con la parola da cercare                       */
                msgrcv(queue, &message, sizeof(msg) - sizeof(long), num, 0);
                occurrences = 0;
                while (fgets(buffer, BUFSIZE, file))
                    if (strstr(buffer, message.text))
                        occurrences++;
                
                /* invia un messaggio con il numero di occorrenze */
                message.type = DEST_PARENT;
                message.payload = REPLY_DATA;
                message.number = occurrences;
                msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
                break;
         	case CMD_EXIT:
         		done = 1;
         		break;					
		}
		
	}while(!done);
	fprintf(stdout,"Child D-%d on directory %s terminated\n",num,dir);
	exit(0);
}

int main(int argc, char* argv[])
{
	int queue;
	int children,len,i;
	struct stat statbuf;
	char buffer[BUFSIZE];
	msg message;
	char *token;
	int done = 0;
	if(argc<2)
	{
		fprintf(stderr,"bad usage -> file_shell <dir> <dir> ... \n");
		exit(1);
	}
	//creazione coda
	if ((queue = msgget(IPC_PRIVATE, IPC_CREAT|0600)) == -1) {
        perror("msgget");
        exit(1);
    }
    //creazione processi figli
    children = 0;
    for(int i = 1 ; i < argc ; i++)
    {
    	if(stat(argv[i],&statbuf)== -1 || (!S_ISDIR(statbuf.st_mode)))
    	{
    		fprintf(stdout,"Problems occurred on %s, skipped!\n",argv[i]);
    		continue;
    	}
    	children++;
    	if(fork() == 0)
    	{
    		child_dir(i, argv[i], queue);
    	}
    }
    	if(children == 0)
    	{
    		fprintf(stdout,"No parameters passed valid, terminating process!");
    		exit(1);
    	}
    	sleep(1);
    	
    	//creazione nano-shell per i comandi
    	
    	do
    	{
    		fprintf(stdout,"file-shell>");
    		fgets(buffer,BUFSIZE,stdin);
    		len = strlen(buffer);
       		if (buffer[len-1] == '\n')
            	buffer[len-1] = '\0';
    		token = strtok(buffer," ");//cmd extraction
    		//list cmd got
    		if(!strcmp(token,"list"))
    		{
    			token = strtok(NULL," ");
    			if(!token)
				{
					printf("Must specify the directory number\n");
					continue;
				}
    			i = atoi(token);
    			if(i >= 1 && i <= children)
    			{
    				message.type = i;
    				message.payload = CMD_LIST;
    				msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
    				while(1)
    				{
    					 msgrcv(queue, &message, sizeof(msg) - sizeof(long), DEST_PARENT, 0);
                         if (message.payload == REPLY_ERROR) {
                         fprintf(stderr, "P - errore dal figlio\n");
                         break;
                         } else if (message.payload == REPLY_STOP)
                           	break;
                         printf("\t%s\n", message.text);	
    				}
    			}
    		}
    		else if(!strcmp(token,"clear"))
    		{
    			 printf("\033[H\033[J");
    		}
    		else if(!strcmp(token,"size"))
    		{
    			if (token = strtok(NULL, " ")) {    // estrae il secondo token: il numero del figlio
                    i = atoi(token);
                    if ((i >= 1) && (i <= children)) {
                        message.type = i;
                        if (token = strtok(NULL, " ")) {    // estrae il terzo token: il nome del file
                            /* invia un messaggio al figlio indicato con la richista CMD_SIZE */
                            strncpy(message.text, token, TEXT_DIM);
                            message.payload = CMD_SIZE;
                            msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);

                            /* riceve un messaggio con la risposta */
                            msgrcv(queue, &message, sizeof(msg) - sizeof(long), DEST_PARENT, 0);
                            if (message.payload == REPLY_ERROR)
                                fprintf(stderr, "P - errore dal figlio\n");
                            else
                                printf("\t%ld byte\n", message.number);
                        }
                    }
                }
    		}
    		else if(!strcmp(token,"search"))
    		{
    		
    			 if (token = strtok(NULL, " ")) {    // estrae il secondo token: il numero del figlio
                    i = atoi(token);
                    if ((i >= 1) && (i <= children)) {
                        message.type = i;
                        if (token = strtok(NULL, " ")) {    // estrae il terzo token: il nome del file
                            /* invierà due messaggi di tipo CMD_SEARCH:
                               uno con il nome del file e un altro 
                               con la parola da cercare
                               
                               qui viene intanto preparato il primo     */
                            strncpy(message.text, token, TEXT_DIM);
                            message.payload = CMD_SEARCH;
                            if (token = strtok(NULL, " ")) {    // estrae il quarto token: la parola da cercare
                                /* invia il primo messaggio solo dopo essere sicuri di avere la parola */
                                msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);
                                
                                /* crea ed invia il secondo messaggio */
                                strncpy(message.text, token, TEXT_DIM);
                                message.payload = CMD_SEARCH;
                                msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0);

                                /* riceve un messaggio con la risposta */
                                msgrcv(queue, &message, sizeof(msg) - sizeof(long), DEST_PARENT, 0);
                                if (message.payload == REPLY_ERROR)
                                    fprintf(stderr, "P - errore dal figlio\n");
                                else
                                    printf("\t%ld occorrenza/e\n", message.number);
                            }
                        }
                    }
                }
    		}
    		else if(!strcmp(token,"exit"))
    		{
    			printf("done!\n");
    			done = 1;
    			
    		}
    		else
    		{
    			fprintf(stdout,"%s command not found\n",token);
    			
    		}	
    	}while(!done);
    
 	/* invia una serie di messaggi di tipo CMD_EXIT
       a tutti i figli e ne aspetta la terminazione */
    for (i = 1; i <= children; i++) {
        message.type = i;
        message.payload = CMD_EXIT;
        if (msgsnd(queue, &message, sizeof(msg) - sizeof(long), 0) == -1)
            perror("msgsnd");
    }
    for (i = 1; i <= children; i++)
        wait(NULL);
    msgctl(queue, IPC_RMID, NULL);   
    return 0;
}
