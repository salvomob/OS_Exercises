#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

int isPal(char *str);


int main()
{
	char x[128];
	while(1)
	{
		fgets(x,128,stdin);
		int len = strlen(x);
		printf("x=%s\nlen = %d\n",x,len);
		if(isPal(x) == 1)
		{
			printf("OK\n");
		}
		else if(isPal(x) == -1)
		{
			printf("NO\n");
		}
	}
}

int isPal(char *str)
{
	int x = 1;
	int len = strlen(str) - 1;
	
	for(int i = 0; i <= len/2;i++){
		if(str[i] != str[len-1-i]) 
		{
			printf("%c & %c\n",str[i],str[len-1-i]);
			x = -1;
		}
	}
	return x;
}	

