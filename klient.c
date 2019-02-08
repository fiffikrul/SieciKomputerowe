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
#include <time.h>
#include <curses.h>
#include <string.h>

#define snake_length 3
#define max_players 4
#define board_size 24
#define board_sizeX 60
#define board_sizeY 20

typedef struct Snake
{
	int x;
	int y;
	struct Snake *next;
} Snake;

Snake snakes[max_players];
int length_snake[max_players];
int feed[max_players];
int fail[max_players];
int board[board_size][board_size];
int number_of_moves = 0;
int food[2];
int randA = -1;
int randB = -1;
char new_food[8];
WINDOW * mainwin, * childwin;
int      width = 80, height = 20;

void deleteAll(struct Snake** head) 
{ 
	/* deref head to get the real head */

	struct Snake* current = *head; 
	struct Snake* next;
	while (current != NULL)  
	{ 
		board[current->x][current->y] = 0;
		next = current->next; 
		free(current); 
		current = next; 
	} 
	/* deref head to affect the real head back in the caller. */
	*head = NULL; 
} 


/* Add a new node to the top of a list */
Snake * insert_top(char move, Snake *head, int id) {
	int a,b;
	a = head->x;
	b = head->y;
	Snake *new_node;
	new_node = (Snake *) malloc(sizeof(Snake));
	switch(move)
	{
		case 's':
		{
			++a;
			break;
		}
		case 'w':
		{
			--a;
			break;
		}
		case 'a':
		{
			--b;
			break;
		}
		case 'd':
		{
			++b;
			break;
		}
	}
	new_node->x = (a + board_size) % board_size;
	new_node->y = (b + board_size) % board_size;
	new_node->next = head;
	head = new_node;
	if(board[new_node->x][new_node->y]!=0&&board[new_node->x][new_node->y]!=7)
	{
		fail[id-1] = 1;
		deleteAll(&snakes[id-1].next);
	}
	else
		board[new_node->x][new_node->y] = id;
	
	return head;
}

void deleteLastNode(Snake *head)
{
    struct Snake *toDelete, *secondLastNode;

    if(head == NULL)
    {
        //printf("List is already empty.");
    }
    else
    {
        toDelete = head;
        secondLastNode = head;

        /* Traverse to the last node of the list */
        while(toDelete->next != NULL)
        {
            secondLastNode = toDelete;
            toDelete = toDelete->next;
        }

        if(toDelete == head)
        {
            head = NULL;
        }
        else
        {
            /* Disconnect link of second last node with last node */
            secondLastNode->next = NULL;
        }
	
	board[toDelete->x][toDelete->y] = 0;

        /* Delete the last node */
        free(toDelete);

        //printf("SUCCESSFULLY DELETED LAST NODE OF LIST\n");
    }
}


	int sock; 
	char send_data[1024];
	struct hostent *host;
	struct sockaddr_in server_addr; 
	ssize_t count;
	char buf[512];



void *print_message( void *ptr ) {
	while(1) {
		while ((count = read(sock, buf, sizeof(buf) - 1))) {
			if (count == -1) {
				/* EAGAIN, read all data */
				if (errno == EAGAIN)
					return 0;

				perror("read");
				break;
			}


			/* Write buffer to stdout */
			buf[count] = '\0';
			//printf("%s \n", buf);

			//printf("4 i 5 = %c %c abc\n\nfdsfds",buf[4],buf[5]);

			if(randA!=-1&&randB!=-1)
			{
				//printf("tutaj");
				food[0] = buf[4];// - '0';
				food[1] = buf[5];// - '0';
				board[food[0]][food[1]] = 7;
				randA = -1;
				randB = -1;
			}
			for(int i=0;i<max_players;i++)
			{
				if(fail[i]==1)
					continue;
				snakes[i].next = insert_top(buf[i],snakes[i].next,i+1);
			}


			//check if eaten
			for(int i=0;i<max_players;i++)
			{
				if(fail[i]==1)
					continue;
				if((snakes[i].next->x==food[0])&&(snakes[i].next->y==food[1]))
					{
						feed[i] = 1;
						//printf("zjedzono cos!!!\n");
					}
			}

			if(number_of_moves>snake_length-1)
			{
				for(int i=0;i<max_players;i++)
					{
						if(fail[i]==1)
							continue;
						if(feed[i]==0)
							deleteLastNode(snakes[i].next);
						else
						{
							//char foodA[2];
							//char foodB[2];
							feed[i] = 0;
							randA = rand() % board_size;
							randB = rand() % board_size;
							while(board[randA][randB]!=0)
							{
								randA = rand() % board_size;
								randB = rand() % board_size;
							}
							new_food[0] = buf[i];
							new_food[1] = randA + '0';
							new_food[2] = randB + '0';
							//new_food[1] = buf[i];
							//new_food[2] = buf[i];
							//new_food[3] = buf[i];
							//itoa(randA,foodA,10);
							 //itoa(randB,foodB,10);
							//new_food[1] = foodA[0];
							//new_food[2] = foodA[1];
							//new_food[3] = foodB[0];
							//new_food[4] = foodB[1];
							send(sock,new_food,strlen(new_food),0);
						}
					}
			}
			else
			{
				++number_of_moves;
			}


			//draw board	
			for(int i=0;i<board_size;i++)
			{
				for(int j=0;j<board_size;j++)
				{
					//printf("%d",board[i][j]);
					if(board[i][j]==0)
					{
					mvwaddstr(childwin, i, j, " ");
					}
					else if(board[i][j]==7)
					{
						mvwaddstr(childwin, i, j, "*");
					}
					else
						mvwaddstr(childwin,i,j,"X");
				}
				//printf("\n");
			}

			wrefresh(childwin);

		
			//for(int i=0;i<max_players;i++)
				//printf("glowa[%d].x:%d,y:%d. nie zyje : %d.\n",i+1,snakes[i].next->x,snakes[i].next->y,fail[i]);

			break;
		}
	}
}

void *send_message( void *ptr )
{
	char ch;
	while((ch = getch()) != 'q')
	{
	    send(sock,&ch,sizeof(ch), 0);
	}

	return NULL;
}


int main()
{
	srand(time(0));
	
	for(int i=0;i<board_size;i++)
	{
		for(int j=0;j<board_size;j++)
			board[i][j] = 0;
	}


    int      rows  = 25, cols   = 80;
    int      x = (cols - width)  / 2;
    int      y = (rows - height) / 2;


	food[0] = board_size/2;
	food[1] = board_size/2;
	buf[4] = food[0];
	buf[5] = food[1];
	board[food[0]][food[1]] = 7;

	Snake *new_node;
	for(int i=0;i<max_players;i++)
	{
		length_snake[i] = snake_length;
		feed[i] = 0;
		fail[i] = 0;
		new_node = (Snake *) malloc(sizeof(Snake));

		switch(i)
		{
			case 0:
			{
				new_node->x = 0;
				new_node->y = 0;
				break;
			}
			case 1:
			{
				new_node->x = 0;
				new_node->y = board_size-1;
				break;
			}
			case 2:
			{
				new_node->x = board_size-1;
				new_node->y = 0;
				break;
			}
			case 3:
			{
				new_node->x = board_size-1;
				new_node->y = board_size-1;
				break;
			}
		}
		new_node->next = NULL;
		snakes[i].next = new_node;
		//printf("x:%d,y:%d\n",snakes[i].next->x,snakes[i].next->y);
		board[snakes[i].next->x][snakes[i].next->y] = i+1;
	}







	host = gethostbyname("150.254.32.135");

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

	//printf("\nSEND (q or Q to quit) : ");

	pthread_t t1,t2;
	int  iret1, iret2;

	if ( (mainwin = initscr()) == NULL ) {
	fprintf(stderr, "Error initialising ncurses.\n");
	exit(EXIT_FAILURE);
    	}

    noecho();
    keypad(mainwin, TRUE);
	curs_set(0); //niewidzialny

    /*  Make our child window, and add
	a border and some text to it.   */
 
    childwin = subwin(mainwin, height, width, y, x);
    box(childwin, 0, 0);

mvwaddstr(childwin, 1, 1, "Wcisnij p, aby zaczac");

    refresh();



	/* Create independent threads each of which will execute function */
	iret1 = pthread_create( &t1, NULL, send_message, (void*) "dzoala1");
	if(iret1)
	{
	 fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
	 exit(EXIT_FAILURE);
	}
	iret2 = pthread_create( &t2, NULL, print_message, (void*) "dziala2");
	if(iret2)
	{
	 fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
	 exit(EXIT_FAILURE);
	}
		
	


	/*wait for terminate*/
	pthread_join(t1,NULL);
	pthread_join(t2,NULL);


	close(sock);
	return 0;
}
