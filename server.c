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

#define MAXEVENTS 16
#define MAXPLAYERS 4

//GLOBALS
static int socket_fd, epoll_fd;
struct epoll_event event, *events;
char ip_string[16], port_string[6];

//PLAYER STRUCT
struct Player{
	pthread_t read_thread, write_thread;
	int read_thread_ret, write_thread_ret;
	bool in_room, ready;
	int my_fd;
	int room_number, player_number;
};

//ROOM ATTRIBS
struct Room{
	pthread_t control_thread, timer_thread;
	pthread_mutex_t settings_lock;
	pthread_mutex_t players_lock;
	pthread_cond_t synchronization;
	pthread_mutex_t synch_lock;
	int control_thread_ret, timer_thread_ret;
	int game_time;
	int game_size;
	int food[2];
	char move[4];
	bool playing, ready_to_go;
	int number_of_players, number_of_ready;
	int player_fd[MAXPLAYERS];
	int countdown;
	int synchronized;
};

struct Room rooms[4];

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

static void socket_create_bind_local(){
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

	get_ipport();

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(port_string));
	server_addr.sin_addr.s_addr = inet_addr(ip_string);
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

void sleep_ms(int miliseconds){
	struct timespec ts;
	ts.tv_sec = miliseconds / 1000;
	ts.tv_nsec = (miliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

void *read_client(void* my_player){
	ssize_t count;
	struct Player *me = (struct Player*)my_player;
	char read_buf[200];
	int room_nr = -1;

	while ((count = read(me->my_fd, read_buf, sizeof(read_buf) - 1)) > 0){
		read_buf[count] = '\0';
		if (me->in_room){
			if (rooms[room_nr].playing){
				if (count == 3){
					rooms[room_nr].food[0] = read_buf[1] - '0';
					rooms[room_nr].food[1] = read_buf[2] - '0';
				}
				else{
					rooms[room_nr].food[0] = -1;
					rooms[room_nr].food[1] = -1;
				}
				rooms[room_nr].move[me->player_number] = read_buf[0];
			}
			else{
				if (memcmp(read_buf, "/czas ", 6) == 0){
					pthread_mutex_lock(&rooms[room_nr].settings_lock);
					printf("time\n");
					char time_string[5];
					for (int i = 6; i < count; i++){
						time_string[i - 6] = read_buf[i];
					}
					rooms[room_nr].game_time = atoi(time_string);
					if (rooms[room_nr].game_time < 1)
						rooms[room_nr].game_time = 1;
					if (rooms[room_nr].game_time > 20)
						rooms[room_nr].game_time = 20;
					pthread_mutex_unlock(&rooms[room_nr].settings_lock);
				}

				if (memcmp(read_buf, "/rozmiar ", 9) == 0){
					pthread_mutex_lock(&rooms[room_nr].settings_lock);
					printf("size\n");
					char size_string[5];
					for (int i = 9; i < count; i++){
						size_string[i - 9] = read_buf[i];
					}
					rooms[room_nr].game_size = atoi(size_string);
					if (rooms[room_nr].game_size < 9)
						rooms[room_nr].game_size = 9;
					if (rooms[room_nr].game_size > 30)
						rooms[room_nr].game_size = 30;
					pthread_mutex_unlock(&rooms[room_nr].settings_lock);
				}
				if (memcmp(read_buf, "/ready", 6) == 0 && !me->ready){
					pthread_mutex_lock(&rooms[room_nr].players_lock);
					printf("ready\n");
					me->ready = true;
					rooms[room_nr].number_of_ready++;
					if (rooms[room_nr].number_of_ready == rooms[room_nr].number_of_players && rooms[room_nr].number_of_players >= 2){
						rooms[room_nr].ready_to_go = true;
					}
					pthread_mutex_unlock(&rooms[room_nr].players_lock);
				}
				if (memcmp(read_buf, "!ready", 6) == 0 && me->ready){
					pthread_mutex_lock(&rooms[room_nr].players_lock);
					printf("not ready\n");
					me->ready = false;
					rooms[room_nr].number_of_ready--;
					rooms[room_nr].ready_to_go = false;
					pthread_mutex_unlock(&rooms[room_nr].players_lock);
				}
				if (memcmp(read_buf, "/exit", 6) == 0){
					pthread_mutex_lock(&rooms[room_nr].players_lock);
					printf("lobby\n");
					if (me->ready){
						rooms[room_nr].number_of_ready--;
						me->ready = false;
					}
					me->in_room = false;
					rooms[room_nr].number_of_players--;
					if (rooms[room_nr].number_of_players < 2 && rooms[room_nr].ready_to_go){
						rooms[room_nr].ready_to_go = false;
					}
					pthread_mutex_unlock(&rooms[room_nr].players_lock);
				}
			}
		}
		else{
			int possible_number = read_buf[0] - '0';
			if (possible_number <= 4 && possible_number >= 1){
				pthread_mutex_lock(&rooms[possible_number - 1].players_lock);
				if (!rooms[possible_number - 1].playing && rooms[possible_number - 1].number_of_players < MAXPLAYERS){
					room_nr = possible_number - 1;
					if (rooms[room_nr].ready_to_go)
						rooms[room_nr].ready_to_go = false;
					rooms[room_nr].number_of_players++;
					me->room_number = room_nr;
					me->in_room = true;
					for (int i = 0; i < MAXPLAYERS; i++){
						if (rooms[room_nr].player_fd[i] == -1){
							me->player_number = i;
							rooms[room_nr].player_fd[i] = me->my_fd;
							break;
						}
					}
					printf("Ma numer %d i id %d\nTeraz w tym pokoju jest %d graczy\n", me->player_number, me->my_fd, rooms[room_nr].number_of_players);
				}
				if(room_nr == -1)
					printf("Nie bylo slota\n");
				pthread_mutex_unlock(&rooms[room_nr].players_lock);
			}
		}
	}
	if (count == -1){
		perror("read");
		return &me->my_fd;
	}
	printf("Closed connection on descriptor: %d\n", me->my_fd);
	if (me->in_room){
		pthread_mutex_lock(&rooms[room_nr].players_lock);
		for (int i = 0; i < MAXPLAYERS; i++){
			if (rooms[room_nr].player_fd[i] == me->my_fd)
				rooms[room_nr].player_fd[i] = -1;
		}
		if (me->ready){
			rooms[room_nr].ready_to_go = false;
			rooms[room_nr].number_of_ready--;
		}
		rooms[room_nr].number_of_players--;
		pthread_mutex_unlock(&rooms[room_nr].players_lock);
	}
	close(me->my_fd);
	free(my_player);
	return NULL;
}

void *write_client(void *my_player){
	struct Player *me = (struct Player *)my_player;
	ssize_t count;
	char write_buf[200];
	int send_size;
	int room_nr;
	char number_string[2], time_string[2], size_string[2];
	int countdown = rooms[room_nr].countdown;

	do{
		sleep_ms(1000);
		if (me->in_room){
			room_nr = me->room_number;
			if (rooms[room_nr].playing){
				/*pthread_mutex_lock(&rooms[room_nr].synch_lock);
				rooms[room_nr].synchronized++;
				if (rooms[room_nr].synchronized == rooms[room_nr].number_of_ready){
					pthread_cond_broadcast(&rooms[room_nr].synchronization);
				}
				else{
					pthread_cond_wait(&rooms[room_nr].synchronization, &rooms[room_nr].synch_lock);
				}
				pthread_mutex_unlock(&rooms[room_nr].synch_lock);*/

				for (int i = 0; i < 4; i++){
					write_buf[i] = rooms[room_nr].move[i];
				}
				if (rooms[room_nr].food[0] != -1){
					write_buf[4] = rooms[room_nr].food[0] + '0';
					write_buf[5] = rooms[room_nr].food[1] + '0';
					write_buf[6] = '\0';
					send_size = 7;
				}
				else{
					write_buf[4] = '\0';
					send_size = 5;
				}
			}
			else if (rooms[room_nr].ready_to_go){
				sprintf(number_string, "%d", countdown);
				if (countdown == 10){
					write_buf[0] = number_string[0];
					write_buf[1] = number_string[1];
					write_buf[2] = '\0';
					send_size = 3;
				}
				else{
					write_buf[0] = number_string[0];
					write_buf[1] = '\0';
					send_size = 2;
				}
				countdown--;
				if (countdown == -1){
					rooms[room_nr].playing = true;
					rooms[room_nr].ready_to_go = false;
				}
			}
			else{
				countdown = rooms[room_nr].countdown;
				sprintf(time_string, "%d", rooms[room_nr].game_time);
				sprintf(size_string, "%d", rooms[room_nr].game_size);
				for (int i = 0; i < 2; i++){
					write_buf[i] = time_string[i];
					write_buf[i + 2] = size_string[i];
				}
				if (rooms[room_nr].game_time < 10)
					write_buf[1] = 'n';
				if (rooms[room_nr].game_size < 10)
					write_buf[3] = 'n';
				write_buf[4] = rooms[room_nr].number_of_players + '0';
				write_buf[5] = rooms[room_nr].number_of_ready + '0';
				write_buf[6] = '\0';
				send_size = 7;
			}
		}
		else{
			for(int i = 0; i < 4; i++){
				pthread_mutex_lock(&rooms[i].players_lock);
				write_buf[2 * i] = rooms[i].number_of_players + '0';
				if (rooms[i].playing)
					write_buf[2 * i + 1] = '1';
				else
					write_buf[2 * i + 1] = '0';
				pthread_mutex_unlock(&rooms[i].players_lock);
			}
			write_buf[8] = '\0';
			send_size = 9;
		}
	} while ((count = write(me->my_fd, write_buf, send_size)) > 0);
	if (count == -1){
		perror("write");
		return &me->my_fd;
	}
	else if (count == 0){
		printf("Lost connection with descriptor %d\n", me->my_fd);
	}
	return NULL;
}

void new_client(int fd){
	struct Player *new_player;
	//każdy gracz otrzymuje wątek czytania i pisania
	new_player = malloc(sizeof(struct Player));
	new_player->in_room = false;
	new_player->ready = false;
	new_player->room_number = -1;
	new_player->player_number = -1;
	new_player->my_fd = fd;

	new_player->write_thread_ret = pthread_create(&new_player->write_thread, NULL, write_client, (void *)new_player);
	if (new_player->write_thread_ret){
		fprintf(stderr, "Error - pthread_create() return code: %d\n", new_player->write_thread_ret);
		exit(EXIT_FAILURE);
	}
	new_player->read_thread_ret = pthread_create(&new_player->read_thread, NULL, read_client, (void *)new_player);
	if (new_player->read_thread_ret){
		fprintf(stderr, "Error - pthread_create() return code: %d\n", new_player->read_thread_ret);
		exit(EXIT_FAILURE);
	}
}

void accept_and_add_new(){
	struct sockaddr in_addr;
	socklen_t in_len = sizeof(in_addr);
	int infd;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	while ((infd = accept(socket_fd, &in_addr, &in_len)) != -1)
	{

		if (getnameinfo(&in_addr, in_len,
						hbuf, sizeof(hbuf),
						sbuf, sizeof(sbuf),
						NI_NUMERICHOST | NI_NUMERICHOST) == 0)
		{
			printf("Accepted connection on descriptor %d (host=%s, port=%s)\n",
				   infd, hbuf, sbuf);
		}
		/*if (make_socket_non_blocking(infd) == -1){
			abort();
		}*/

		event.data.fd = infd;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event) == -1)
		{
			perror("epoll_ctl");
			abort();
		}
		in_len = sizeof(in_addr);
		new_client(infd);
	}
}

void accept_client(){
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
				accept_and_add_new();
			}
		}
	}
}

/*bool countdown(int time_in_seconds, struct Room* room){
	clock_t startTime, currentTime;
	int miliseconds = 0, seconds = 0;
	int time_left = time_in_seconds, time_old = time_in_seconds;
	startTime = clock();
	while(time_left > 0){
		currentTime = clock();
		miliseconds = currentTime - startTime;
		seconds = (int)(miliseconds/(CLOCKS_PER_SEC));
		time_left = time_in_seconds - seconds;
		if(!room->ready_to_go)
			return false;
		if(time_left != time_old){
			printf("%d!\n", time_left + 1);
			time_old = time_left;
		}
	}
	return true;
}*/

/*void *control_client(void *room){
	struct Room *me = (struct Room *)room;
	while(1){
		sleep_ms(1000);
		int counter = 0;

		if (counter == (*me).number_of_players && (*me).number_of_players >= 2){
			(*me).ready_to_go = true;
		}
		else{
			(*me).ready_to_go = false;
		}
	}
}

void *timer_client(void *room){
	struct Room *me = (struct Room *)room;
	while(1){
		sleep_ms(1000);
		if (!me->playing && me->ready_to_go)
			me->playing = countdown(10, me);
		if (me->playing){
			me->playing = countdown(me->game_time * 60, me);
			me->playing = false;
			me->number_of_ready = 0;
		}

	}
}*/

void create_room(struct Room *room){
	room->game_time = 5;
	room->game_size = 15;
	room->food[0] = -1;
	room->food[1] = -1;
	room->move[0] = 's';
	room->move[1] = 'a';
	room->move[2] = 'd';
	room->move[3] = 'w';
	room->playing = false;
	room->ready_to_go = false;
	room->number_of_players = 0;
	room->number_of_ready = 0;
	room->countdown = 10;
	room->synchronized = 0;
	pthread_mutex_init(&room->settings_lock, NULL);
	pthread_mutex_init(&room->players_lock, NULL);

	for (int i = 0; i < MAXPLAYERS; i++){
		room->player_fd[i] = -1;
	}
	/*room->control_thread_ret = pthread_create(&room->control_thread, NULL, control_client, (void *)room);
	if (room->control_thread_ret){
		fprintf(stderr, "Error - pthread_create() return code: %d\n", room->control_thread_ret);
		exit(EXIT_FAILURE);
	}

	room->timer_thread_ret = pthread_create(&room->timer_thread, NULL, timer_client, (void *)room);
	if (room->timer_thread_ret){
		fprintf(stderr, "Error - pthread_create() return code: %d\n", room->timer_thread_ret);
		exit(EXIT_FAILURE);
	}*/
}

int main(){

	socket_create_bind_local();

	if (make_socket_non_blocking(socket_fd) == -1)
		exit(1);

	if (listen(socket_fd, 5) == -1){
		perror("Listen");
		exit(1);
	}

	printf("TCP Server Waiting for client on\nIP: %s\tport: %s\n", ip_string, port_string);
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

	for (int i = 0; i < 4; i++){
		create_room(&rooms[i]);
		printf("Powstał pokój nr %d\n", i + 1);
	}
	accept_client();
	
	pthread_join(rooms[0].control_thread, NULL);
	pthread_join(rooms[0].timer_thread, NULL);
	free(events);
	close(socket_fd);
	return 0;
}
