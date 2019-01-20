#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

#define MAXEVENTS 64

int test_ile_graczy = 0;

char test[48];

int gracze[MAXEVENTS];
int id[MAXEVENTS];
char ostatni_ruch[MAXEVENTS];

static int socket_fd, epoll_fd;
struct epoll_event event, *events;

void *licz_sekundy( void *ptr )
	{
		while(1)
		{
			sleep(1);
			for(int i=0;i<MAXEVENTS;i++)
			{
				if(id[i]!=0)
				{
					printf("\n\n%d\n\n",i);
					write(id[i],&ostatni_ruch[i],sizeof(ostatni_ruch));
				}
			}
			printf("minela sekunda\n");
		}
	}


static void socket_create_bind_local()
{
	struct sockaddr_in server_addr;
	int opt = 1;

        if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Socket");
            exit(1);
        }

        if (setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(int)) == -1) {
            perror("Setsockopt");
            exit(1);
        }
       
        server_addr.sin_family = AF_INET;        
        server_addr.sin_port = htons(5000);    
        server_addr.sin_addr.s_addr = INADDR_ANY;
        bzero(&(server_addr.sin_zero),8);

        if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))
                                                                       == -1) {
            perror("Unable to bind");
            exit(1);
        }

}

static int make_socket_non_blocking(int sfd)
{
	int flags;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(sfd, F_SETFL, flags) == -1) {
		perror("fcntl");
		return -1;
	}

	return 0;
}

void accept_and_add_new()
{
	struct epoll_event event;
	struct sockaddr in_addr;
	socklen_t in_len = sizeof(in_addr);
	int infd;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	while ((infd = accept(socket_fd, &in_addr, &in_len)) != -1) {

		if (getnameinfo(&in_addr, in_len,
				hbuf, sizeof(hbuf),
				sbuf, sizeof(sbuf),
				NI_NUMERICHOST | NI_NUMERICHOST) == 0) {
			printf("Accepted connection on descriptor %d (host=%s, port=%s)\n",
					infd, hbuf, sbuf);
		}
		/* Make the incoming socket non-block
		 * and add it to list of fds to
		 * monitor*/
		if (make_socket_non_blocking(infd) == -1) {
			abort();
		}

		event.data.fd = infd;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event) == -1) {
			perror("epoll_ctl");
			abort();
		}
		in_len = sizeof(in_addr);

	//-----------------------------------------------------------------------------------------------------------------dodaje poczatkowe dane gracze--
		for(int i=0;i<MAXEVENTS;i++)
		{
			if(gracze[i]==0)
			{
				gracze[i] = 1;
				id[i] = infd;
				ostatni_ruch[i] = i;
				test[0] = i;
				printf("Gracz %d o fd %d, ostatni ruch: %c\n",i,infd,ostatni_ruch[i]);
				printf("\n%d to jest id[i]",i);
				write(id[i],test,sizeof(test));
				break;
			}
		}
	}

	if (errno != EAGAIN && errno != EWOULDBLOCK)
		perror("accept");



	/* else
	 *
	 * We hae processed all incomming connectioins
	 *
	 */
}

void process_new_data(int fd)
{
	ssize_t count;
	char buf[512];
	int id_gracza;

	for(int i=0;i<64;i++)
	{
		if(id[i]==fd)
		{
			id_gracza = i;
			printf("Id gracza: %d\n",i);
			break;
		}
	}

	while ((count = read(fd, buf, sizeof(buf) - 1))) {
		if (count == -1) {
			/* EAGAIN, read all data */
			if (errno == EAGAIN)
				return;

			perror("read");
			break;
		}

		/* Write buffer to stdout */
		buf[count] = '\0';
		printf("%s \n", buf);
		//ostatni_ruch[id_gracza] = fd;
		ostatni_ruch[id_gracza] = buf[0];
		printf("Gracz tab = %d ",id_gracza);
		printf("Zmieniono ostatni ruch, wynosi on %c\n",ostatni_ruch[id_gracza]);////////////////////////////////////////////////////////////////////////////////////////////////
	}
	
	printf("Close connection on descriptor: %d\n", fd);
	close(fd);
}

void *czekaj_na_cos( void *ptr )
{
	while(1)
	{
		int n, i;
		n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
		for (i = 0; i < n; i++)
		{
				if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP ||
				    !(events[i].events & EPOLLIN)) {
					/* An error on this fd or socket not ready */
					perror("epoll error");
					close(events[i].data.fd);
				} else if (events[i].data.fd == socket_fd) {
					/* New incoming connection */
					accept_and_add_new();
				} else {
					/* Data incoming on fd */
					process_new_data(events[i].data.fd);
				}
		}
	}
}

int main()
{

	socket_create_bind_local();

	//--zerowanie tablic
	for (int i=0;i<64;i++)
	{
		gracze[i] = 0;
		id[i] = 0;
		ostatni_ruch[i] = '0';
	}

	if (make_socket_non_blocking(socket_fd) == -1)
		exit(1);
       
        if (listen(socket_fd, 5) == -1) {
            perror("Listen");
            exit(1);
        }
  
	printf("\nTCPServer Waiting for client on port 5000\n");
        fflush(stdout);

	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("epoll_create1");
		exit(1);
	}

	event.data.fd = socket_fd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1) {
		perror("epoll_ctl");
		exit(1);
	}

	events = calloc(MAXEVENTS, sizeof(event));

	pthread_t t1,t2;
	int  iret1,iret2;
	iret1 = pthread_create( &t1, NULL, licz_sekundy, (void*) "dzoala1");
	if(iret1)
	     {
	         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
	         exit(EXIT_FAILURE);
	     }

	iret2 = pthread_create( &t2, NULL, czekaj_na_cos, (void*) "dzoala1");
	if(iret2)
	     {
	         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
	         exit(EXIT_FAILURE);
	     }

	pthread_join(t2,NULL);


	pthread_join(t1,NULL);

	free(events);
	close(socket_fd);
	return 0;
}
