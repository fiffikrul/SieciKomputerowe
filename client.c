#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

int sock; 
char send_data[1024];
struct hostent *host;
struct sockaddr_in server_addr; 
ssize_t count;
char buf[512];
char ip_string[16], port_string[6];

void *print_message_function( void *ptr ){
	while(1) {
		while ((count = read(sock, buf, sizeof(buf) - 1))) {/////////////////////////////////////////////////////
			if (count == -1) {
				/* EAGAIN, read all data */
				if (errno == EAGAIN)
					return 0;

				perror("read");
				break;
			}

			/* Write buffer to stdout */
			buf[count] = '\0';
			printf("%s \n", buf);//////////////////////////////////////////////////////////////////
			break;
		}
	}
}

void *print_message_function1( void *ptr ){
	while(1){
		gets(send_data);

		if (strcmp(send_data , "q") != 0 && strcmp(send_data , "Q") != 0)
			send(sock,send_data,strlen(send_data), 0);
		else
			break;
	}
}

void get_ipport(){
	FILE *config_file;

	config_file = fopen("config.txt", "r");
	if (config_file == NULL){
		printf("Couldn't find config.txt\n");
	}
	else{
		fscanf(config_file, "%s", ip_string);
		fscanf(config_file, "%s", port_string);
	}
}

int main()
{
	//host = gethostbyname("127.0.0.1");

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	    perror("Socket");
	    exit(1);
	}

	get_ipport();

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(port_string));
	server_addr.sin_addr.s_addr = inet_addr(ip_string);
	bzero(&(server_addr.sin_zero),8);

	if (connect(sock, (struct sockaddr *)&server_addr,
		    sizeof(struct sockaddr)) == -1)
	{
	    perror("Connect");
	    exit(1);
	}

	printf("\nSEND (q or Q to quit) : ");

	pthread_t t1,t2;
	int  iret1, iret2;

/* Create independent threads each of which will execute function */
	     iret1 = pthread_create( &t1, NULL, print_message_function1, (void*) "dziala1");
	     if(iret1)
	     {
	         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
	         exit(EXIT_FAILURE);
	     }
	     iret2 = pthread_create( &t2, NULL, print_message_function, (void*) "dziala2");
	     if(iret2)
	     {
	         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
	         exit(EXIT_FAILURE);
	     }


	
	pthread_join(t1,NULL);
	pthread_join(t2,NULL);


	close(sock);
	return 0;
}
