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
#include <time.h>
#include <stdbool.h>

#define MAXEVENTS 4

static int socket_fd, epoll_fd;
struct epoll_event event, *events;
int events_fd[MAXEVENTS];

pthread_t read_thread, write_thread, control_thread, timer_thread;
int read_thread_ret, write_thread_ret, control_thread_ret, timer_thread_ret;
char move[4] = {'s','a','w','d'};
int game_time = 5;
int game_size = 15;
int number_of_events = 0, food[2] = {-1, -1};
bool playing = false, ready_to_go = false;
bool player_ready[MAXEVENTS];
int number_of_players = 0;

static void
socket_create_bind_local()
{
	struct sockaddr_in server_addr;
	int opt = 1;

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Socket");
		exit(1);
	}

	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1){
		perror("Setsockopt");
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(5000);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(server_addr.sin_zero), 8);

	if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1){
		perror("Unable to bind");
		exit(1);
	}
}

static int make_socket_non_blocking(int sfd){
	int flags;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1){
		perror("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(sfd, F_SETFL, flags) == -1){
		perror("fcntl");
		return -1;
	}

	return 0;
}

void accept_and_add_new(){
	struct sockaddr in_addr;
	socklen_t in_len = sizeof(in_addr);
	int infd;
	number_of_events++;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	while ((infd = accept(socket_fd, &in_addr, &in_len)) != -1){

		if (getnameinfo(&in_addr, in_len,
						hbuf, sizeof(hbuf),
						sbuf, sizeof(sbuf),
						NI_NUMERICHOST | NI_NUMERICHOST) == 0){
			number_of_players++;
			printf("Accepted connection on descriptor %d (host=%s, port=%s)\nnumber of players = %d\n",
				   infd, hbuf, sbuf, number_of_players);
		}
		/* Make the incoming socket non-block
		 * and add it to list of fds to
		 * monitor*/
		if (make_socket_non_blocking(infd) == -1){
			abort();
		}

		for (int i = 0; i < MAXEVENTS; i++){
			if (events_fd[i] == 0)
			{
				events_fd[i] = infd;
				break;
			}
		}
		event.data.fd = infd;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event) == -1){
			perror("epoll_ctl");
			abort();
		}
		in_len = sizeof(in_addr);
	}

	if (errno != EAGAIN && errno != EWOULDBLOCK)
		perror("accept");
	/* else
	 *
	 * We have processed all incomming connections
	 *
	 */
}

void process_new_data(int fd){
	ssize_t count;
	char buf[20];

	while ((count = read(fd, buf, sizeof(buf) - 1))){
		if (count == -1){
			if (errno == EAGAIN)
				return;
			perror("read");
			break;
		}
		buf[count] = '\0';

		if (playing){
			if (count == 3){
				food[0] = buf[1] - '0';
				food[1] = buf[2] - '0';
			}
			else{
				food[0] = -1;
				food[1] = -1;
			}
			move[fd - 5] = buf[0];
		}
		else{
			if (memcmp(buf, "/czas ", 6) == 0){
				char time_string[5];
				for (int i = 6; i < count; i++){
					time_string[i - 6] = buf[i];
				}
				game_time = atoi(time_string);
				if (game_time < 1)
					game_time = 1;
				if (game_time > 20)
					game_time = 20;
			}

			if (memcmp(buf, "/rozmiar ", 9) == 0)
			{
				char size_string[5];
				for (int i = 9; i < count; i++){
					size_string[i - 9] = buf[i];
				}
				game_size = atoi(size_string);
				if (game_size < 9)
					game_size = 9;
				if (game_size > 30)
					game_size = 30;
			}
			if (memcmp(buf,"p", 1) == 0)
			{
				player_ready[fd - 5] = true;
			}
			if (memcmp(buf, "!ready", 6) == 0)
			{
				player_ready[fd - 5] = false;
			}
		}

		/*printf("Czas gry to %d\nRozmiar planszy to %d\n", game_time, game_size);
		if (player_ready[fd - 5] == true)
			printf("Player %d is ready",fd);
		else
			printf("Player %d is not ready", fd);*/

		/* Write buffer to stdout */
		printf("\n%s \n", buf);
	}
	

	number_of_events--;
	for (int i = 0; i < number_of_events; i++){
		if (events_fd[i] == fd)
		events_fd[i] = 0;
	}
	player_ready[fd - 5] = false;
	number_of_players--;
	printf("number of players = %d\n", number_of_players);
	printf("Close connection on descriptor: %d\n", fd);
	close(fd);
}

void sleep_ms(int miliseconds){
	struct timespec ts;
	ts.tv_sec = miliseconds / 1000;
	ts.tv_nsec = (miliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

void *read_client(void *ptr){
	while (1){
		int n, i;
		n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP ||
				!(events[i].events & EPOLLIN)){
				/* An error on this fd or socket not ready */
				perror("epoll error");
				close(events[i].data.fd);
			}
			else if (events[i].data.fd == socket_fd){
				/* New incoming connection */
				accept_and_add_new();
			}
			else{
				/* Data incoming on fd */
				process_new_data(events[i].data.fd);
			}
		}
	}
}

bool countdown(int time_in_seconds){
	clock_t startTime, currentTime;
	int miliseconds = 0, seconds = 0;
	int time_left = time_in_seconds, time_old = time_in_seconds;
	startTime = clock();
	while(time_left > 0){
		currentTime = clock();
		miliseconds = currentTime - startTime;
		seconds = (int)(miliseconds/(CLOCKS_PER_SEC));
		time_left = time_in_seconds - seconds;
		if(!ready_to_go)
			return false;
		if(time_left != time_old){
			printf("%d!\n", time_left + 1);
			time_old = time_left;
		}
	}
	return true;
}

void *control_client(void *ptr){
	while(1){
		sleep_ms(1000);
		int counter = 0;
		for (int i = 0; i < MAXEVENTS; i++){
			if (player_ready[i] == true)
				counter++;
		}

		if (counter == number_of_players && number_of_players >= 2){
			ready_to_go = true;
		}
		else{
			ready_to_go = false;
		}
	}
}

void *timer_client(void *ptr){
	while(1){
		if (!playing && ready_to_go)
			playing = countdown(10);
		if (playing){
			playing = countdown(game_time * 60);
			if (playing)
				playing = false;
			for (int i = 0; i < MAXEVENTS; i++){
				player_ready[i] = false;
			}
		}

	}
}

void *write_client(void *ptr){
	char buf[7];

	while(1) {
		if(playing){
			for (int i = 0; i < 4; i++){
				buf[i] = move[i];
			}
			if (food[0] != -1){
				buf[4] = food[0] + '0';
				buf[5] = food[1] + '0';
				buf[6] = '\0';
			}
			else{
				buf[4] = '\0';
			}
			sleep_ms(250);
			for (int i = 0; i < MAXEVENTS; i++){
				if (events_fd[i] != 0)
					write(events_fd[i], buf, sizeof(buf));
			}
		}
	}
}

int main(){

	for (int i = 0; i < 64; i++){
		events_fd[i] = 0;
	}

	socket_create_bind_local();

	if (make_socket_non_blocking(socket_fd) == -1)
		exit(1);

	if (listen(socket_fd, 5) == -1){
		perror("Listen");
		exit(1);
	}

	printf("\nTCPServer Waiting for client on port 6969\n");
	fflush(stdout);

	epoll_fd = epoll_create1(0);

	if (epoll_fd == -1){
		perror("epoll_create1");
		exit(1);
	}

	event.data.fd = socket_fd;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1){
		perror("epoll_ctl");
		exit(1);
	}

	events = calloc(MAXEVENTS, sizeof(event));

	read_thread_ret = pthread_create(&read_thread, NULL, read_client, (void *)"read works");
	if (read_thread_ret){
		fprintf(stderr, "Error - pthread_create() return code: %d\n", read_thread_ret);
		exit(EXIT_FAILURE);
	}

	control_thread_ret = pthread_create(&control_thread, NULL, control_client, (void *)"control works");
	if (control_thread_ret)
	{
		fprintf(stderr, "Error - pthread_create() return code: %d\n", control_thread_ret);
		exit(EXIT_FAILURE);
	}

	timer_thread_ret = pthread_create(&timer_thread, NULL, timer_client, (void *)"timer works");
	if (timer_thread_ret)
	{
		fprintf(stderr, "Error - pthread_create() return code: %d\n", timer_thread_ret);
		exit(EXIT_FAILURE);
	}

	write_thread_ret = pthread_create(&write_thread, NULL, write_client, (void *)"write works");
	if (write_thread_ret){
		fprintf(stderr, "Error - pthread_create() return code: %d\n", write_thread_ret);
		exit(EXIT_FAILURE);
	}

	pthread_join(write_thread, NULL);
	pthread_join(read_thread, NULL);
	pthread_join(control_thread, NULL);
	pthread_join(timer_thread, NULL);

	free(events);
	close(socket_fd);
	return 0;
}
