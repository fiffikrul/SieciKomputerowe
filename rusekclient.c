#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


int main()
{
	int sock; 
	char send_data[1024];
	struct hostent *host;
	struct sockaddr_in server_addr; 

	host = gethostbyname("127.0.0.1");

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	    perror("Socket");
	    exit(1);
	}

	server_addr.sin_family = AF_INET;    
	server_addr.sin_port = htons(5000);  
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_addr.sin_zero),8);

	if (connect(sock, (struct sockaddr *)&server_addr,
		    sizeof(struct sockaddr)) == -1)
	{
	    perror("Connect");
	    exit(1);
	}

	printf("\nSEND (q or Q to quit) : ");

	while(1) {

        //printf("Wpisz wiadomosc: ");
        //scanf("%c", send_data);
		gets(send_data);

		if (strcmp(send_data , "q") != 0 && strcmp(send_data , "Q") != 0)
		    send(sock,send_data,strlen(send_data), 0);
		else
		    break;
	}
	close(sock);
	return 0;
}