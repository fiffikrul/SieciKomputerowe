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
#include <curses.h>
#include <time.h>
#include <string.h>
#include <arpa/inet.h>

typedef struct Snake
{
	int x;
	int y;
	struct Snake *next;
} Snake;

Snake snakes[4];

int sock; 
char send_data[1024];
struct hostent *host;
struct sockaddr_in server_addr; 
ssize_t count;
char buf[512];
char ip_string[16], port_string[6];
WINDOW * mainwin, * childwin, * info;
int      width = 60, height = 20;
char moje_id = 'X';
int moj_id = -1;
int stan = -1;
int startX = 0, startY = 0;
int liczba_graczy = -1;
int board[20][60];
int minimalna_dlugosc = 4;
int ruchy = 0;
char ch;
int kto_zjadl = -1;
int foodX;
int foodY;
char losowanie[4] = "MMMM";
int zywy[4];
int punkty[4];


void deleteAll(struct Snake** head) 
{ 
	/* deref head to get the real head */

	struct Snake* current = *head; 
	struct Snake* next;
	while (current != NULL)  
	{ 
		board[current->y][current->x] = 0;
		next = current->next; 
		free(current); 
		current = next; 
	} 
	/* deref head to affect the real head back in the caller. */
	*head = NULL; 
} 

void ustawStart()
{
	ruchy = 0;
	liczba_graczy = -1;
	moje_id = 'X';
	moj_id = 0;
	char chwila;
	for(int i=0;i<liczba_graczy;i++)
	{
		chwila = i + '0';
		mvwaddstr(childwin, 4+i, 2, "Gracz ");
		mvwaddch(childwin, 4+i, 9, chwila);
		mvwaddstr(childwin, 4+i, 10, ":");
		chwila = punkty[i] + '0';
		mvwaddch(childwin, 4+i, 11, chwila);
	}
	wrefresh(childwin);
	wrefresh(info);

	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			board[i][j] = 0;
		}
	}

	Snake *new_node;
	for(int i=0;i<liczba_graczy;i++)
	{
		if(zywy[i]==1)
			deleteAll(&snakes[i].next);
		else
			zywy[i] = 1;

		punkty[i] = 0;
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
				new_node->x = width-1;
				new_node->y = 0;
				break;
			}
			case 2:
			{
				new_node->x = 0;
				new_node->y = height-1;
				break;
			}
			case 3:
			{
				new_node->x = width-1;
				new_node->y = height-1;
				break;
			}
		}
		new_node->next = NULL;
		snakes[i].next = new_node;
		//printf("x:%d,y:%d\n",snakes[i].next->x,snakes[i].next->y);
		board[snakes[i].next->y][snakes[i].next->x] = i+1;
	}
	stan = -1;
}

void pokazStart()
{
	wclear(info);
	char znak;
	mvwaddstr(info, 0, 0, "Wybierz pokoj:");
	mvwaddstr(info, 2, 0, "Pokoj 1: liczba graczy ");
	mvwaddstr(info, 5, 0, "Pokoj 2: liczba graczy ");
	mvwaddstr(info, 8, 0, "Pokoj 3: liczba graczy ");
	mvwaddstr(info, 11, 0, "Pokoj 4: liczba graczy ");

	znak = buf[0];
	mvwaddstr(info, 2, 24, &znak);
	if(buf[1]=='1')
		mvwaddstr(info, 3, 0, "Trwa rozgrywka...");
	else
		mvwaddstr(info, 3, 0, "Oczekiwanie na start...");

	znak = buf[2];
	mvwaddstr(info, 5, 24, &znak);
	if(buf[3]=='1')
		mvwaddstr(info, 6, 0, "Trwa rozgrywka...");
	else
		mvwaddstr(info, 6, 0, "Oczekiwanie na start...");

	znak = buf[4];
	mvwaddstr(info, 8, 24, &znak);
	if(buf[5]=='1')
		mvwaddstr(info, 9, 0, "Trwa rozgrywka...");
	else
		mvwaddstr(info, 9, 0, "Oczekiwanie na start...");

	znak = buf[6];
	mvwaddstr(info, 11, 24, &znak);
	if(buf[7]=='1')
		mvwaddstr(info, 12, 0, "Trwa rozgrywka...");
	else
		mvwaddstr(info, 12, 0, "Oczekiwanie na start...");
}

void pokazOczekiwanie()
{
	wclear(info);
	mvwaddstr(info, 10, 0, "e - powrót");
	mvwaddstr(info, 11, 0, "p - zglos gotowosc");
	mvwaddstr(info, 12, 0, "r - anuluj gotowosc");
	mvwaddstr(info, 13, 0, "y - ustaw czas - 1 minuta");
	mvwaddstr(info, 14, 0, "u - ustaw czas - 2 minuty");
	mvwaddstr(info, 15, 0, "h - ustaw czas - 3 minuty");
	mvwaddstr(info, 16, 0, "j - ustaw czas - 5 minut");
	mvwaddstr(info, 17, 0, "n - ustaw czas - 8 minut");
	mvwaddstr(info, 18, 0, "m - ustaw czas - 10 minut");
}

void ustawWeza()
{
	liczba_graczy = buf[5] - '0';

	Snake *new_node;
	for(int i=0;i<liczba_graczy;i++)
	{
		zywy[i] = 1;
		punkty[i] = 0;
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
				new_node->x = width-1;
				new_node->y = 0;
				break;
			}
			case 2:
			{
				new_node->x = 0;
				new_node->y = height-1;
				break;
			}
			case 3:
			{
				new_node->x = width-1;
				new_node->y = height-1;
				break;
			}
		}
		new_node->next = NULL;
		snakes[i].next = new_node;
		//printf("x:%d,y:%d\n",snakes[i].next->x,snakes[i].next->y);
		board[snakes[i].next->y][snakes[i].next->x] = i+1;
	}
	foodX = width/2;
	foodY = height/2;

	board[foodY][foodX] = 5;
}

/* Add a new node to the top of a list */
Snake * insert_top(char move, Snake *head, int id) {
	int a,b,nie_ma;
	a = head->x;
	b = head->y;
	nie_ma = 0;
	Snake *new_node;
	new_node = (Snake *) malloc(sizeof(Snake));

	switch(move)
	{
		case 's':
		{
			++b;
			break;
		}
		case 'w':
		{
			--b;
			break;
		}
		case 'a':
		{
			--a;
			break;
		}
		case 'd':
		{
			++a;
			break;
		}
		default:
		{
			nie_ma = 1;
			break;
		}
	}
	new_node->x = (a + width) % width;
	new_node->y = (b + height) % height;
	new_node->next = head;
	head = new_node;
	if(board[new_node->y][new_node->x]==0 || board[new_node->y][new_node->x]==5)
		board[new_node->y][new_node->x] = id+1;
	else if(nie_ma==0)
	{
		zywy[id] = 0;
		//deleteAll(&snakes[id].next);
	}
	
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
	
	board[toDelete->y][toDelete->x] = 0;

        /* Delete the last node */
        free(toDelete);

        //printf("SUCCESSFULLY DELETED LAST NODE OF LIST\n");
    }
}

void ustawMojaGlowe()
{
	moje_id = buf[0];
	moj_id = moje_id - '0';
	switch(moje_id)
	{
		case '0':
		{
			startX = 0;
			startY = 0;
			break;
		}
		case '1':
		{
			startX = width-1;
			startY = 0;
			break;
		}
		case '2':
		{
			startX = 0;
			startY = height-1;
			break;
		}
		case '3':
		{
			startX = width-1;
			startY = height-1;
			break;
		}
	}
}

void pokazTablice()
{

	char temp;
	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			if(board[i][j]!=0)
			{
				temp = board[i][j] + '0';
				mvwaddstr(childwin, i, j, &temp);
			}
			else
				mvwaddstr(childwin, i, j, " ");
		}
	}
	refresh();
	wrefresh(childwin);
}

void wykonajRuch()
{
	if (count == 7)
	{
		foodY = buf[4] - '0';
		foodX = buf[5] - '0';
		board[foodY][foodX] = 5;
		mvwaddstr(childwin, foodY, foodX, "5");

		wrefresh(childwin);
	}
	if(ruchy>minimalna_dlugosc-1)
	{
		for(int i=0;i<liczba_graczy;i++)
		{
			if(i==kto_zjadl)
			{
				kto_zjadl = -1;
			}
			else
				deleteLastNode(snakes[i].next);
		}
	}
	else
	{
		++ruchy;
	}


	for(int i=0;i<liczba_graczy;i++)
	{

		if(zywy[i]<1)
		{
			continue;
		}
		snakes[i].next = insert_top(buf[i],snakes[i].next,i);
		

		if((snakes[i].next->x==foodX)&&(snakes[i].next->y==foodY))
		{
			kto_zjadl = i;
			punkty[i]++;
			if(moje_id==(i+'0'))
			{	
				losowanie[0] = buf[i];

				int randX = rand() % height;
				int randY = rand() % width;
				while(board[randX][randY]!=0)
				{
					randX = rand() % height;
					randY = rand() % width;
				}
				losowanie[1] = randX + '0';
				losowanie[2] = randY + '0';
				
				mvwaddstr(childwin, 4, 4, "JUPI!!!");
				send(sock,losowanie,sizeof(losowanie),0);
				wrefresh(childwin);
			}
		}

		if(zywy[i]==0 || ((zywy[i]==1)&&(buf[i]=='n')))
		{
			deleteAll(&snakes[i].next);
			zywy[i] = -1;
		}
	}
}

void *rozgrywka( void *ptr ){
	while(1) {
		while ((count = read(sock, buf, sizeof(buf) - 1))) {
			/* Write buffer to stdout */
			buf[count] = '\0';
			if(stan==1)
			{
				ustawMojaGlowe();
				stan = 2;
				pokazTablice();
			}
			if(stan==2 && count==2)
			{
				ustawWeza();
				stan = 3;
				pokazTablice();
				wrefresh(info);

			}
			else if(count==2 && buf[0]=='e')
			{
				mvwaddstr(childwin, 2, 2, "KONIEC GRY!!!!");
				mvwaddstr(childwin, 3, 2, "Wcisnij e, aby zaczac od nowa");
				ustawStart();
				
				sleep(5);
			}
			if(stan==3 && buf[0]=='0') //the game has started
			{
				stan = 4;
			}
			if(stan==4)
			{
				wykonajRuch();
				pokazTablice();
			}

			switch(stan)
			{
				case -1:
				{
					pokazStart();
					break;
				}
				case 2:
				{
					pokazOczekiwanie();
					break;
				}
				case 4:
				{
					wclear(info);
					char pkt;
					for(int i=0;i<4;i++)
					{
						pkt = punkty[i] + '0';
						mvwaddch(info, i, 0, pkt);
					}
				}
			}



			//mvwaddstr(info, 1, 1, buf);
			//mvwaddstr(childwin, startY, startX, &moje_id);
			wrefresh(info);
			wrefresh(childwin);
			//printf("%s \n", buf);
			break;
		}
	}
}

void *send_message( void *ptr )
{
	while((ch = getchar()) != 'q')
	{

	    if( (ch=='d'&&buf[moj_id]=='a') || (ch=='a'&&buf[moj_id]=='d') || (ch=='w'&&buf[moj_id]=='s') ||  (ch=='s'&&buf[moj_id]=='w') )
		continue;
	    if(ch=='o')
	    {
		send(sock,"MMM",sizeof("MMM"),0);
		continue;
	    }
	    if(stan==-1 && (ch=='1'||ch=='2'||ch=='3'||ch=='4'))
	    {
		stan = 1;
	    }
	    if(ch=='e')
	    {
		stan = -1;
		send(sock,"/exit",sizeof("/exit"),0);
		continue;
	    }
	    if(ch=='y')
	    {
		send(sock,"/czas 01",sizeof("/czas 01"),0);
		continue;
	    }
	    if(ch=='u')
	    {
		send(sock,"/czas 02",sizeof("/czas 02"),0);
		continue;
	    }
	    if(ch=='h')
	    {
		send(sock,"/czas 03",sizeof("/czas 03"),0);
		continue;
	    }
	    if(ch=='j')
	    {
		send(sock,"/czas 05",sizeof("/czas 04"),0);
		continue;
	    }
	    if(ch=='n')
	    {
		send(sock,"/czas 08",sizeof("/czas 08"),0);
		continue;
	    }
	    if(ch=='m')
	    {
		send(sock,"/czas 10",sizeof("/czas 10"),0);
		continue;
	    }
	    if(moje_id==(kto_zjadl+'0'))
	    {
		losowanie[0] = ch;
		send(sock,losowanie,sizeof(losowanie),0);
		continue;
	    }
	    send(sock,&ch,sizeof(ch), 0);
	}

	return NULL;
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
	srand(time(0));

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	    perror("Socket");
	    exit(1);
	}

	ustawStart();

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


	pthread_t t1,t2;
	int  iret1, iret2;

	/* Create independent threads each of which will execute function */
	iret1 = pthread_create( &t1, NULL, send_message, (void*) "dziala1");
	if(iret1)
	{
	fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
	exit(EXIT_FAILURE);
	}
	iret2 = pthread_create( &t2, NULL, rozgrywka, (void*) "dziala2");
	if(iret2)
	{
	fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
	exit(EXIT_FAILURE);
	}

	if ( (mainwin = initscr()) == NULL ) {
	fprintf(stderr, "Error initialising ncurses.\n");
	exit(EXIT_FAILURE);
    	}
	
	start_color();
	init_pair(1,COLOR_WHITE, COLOR_CYAN);
	init_pair(2,COLOR_RED, COLOR_YELLOW);
	noecho();
	keypad(mainwin, TRUE);
	curs_set(0); //niewidzialny
	//Dodajemy okna
	childwin = subwin(mainwin, height, width, 0, 0);
	info = subwin(mainwin, height, 25, 0, width);
	wbkgd(info, COLOR_PAIR(2));
	//box(childwin, 0, 0);
	wbkgd(childwin, COLOR_PAIR(1));
	mvwaddstr(childwin, 1, 1, "Wybierz pokoj.");

	refresh();

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);


	close(sock);
	return 0;
}
