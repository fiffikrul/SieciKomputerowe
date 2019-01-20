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

struct Gracz
{
	int glowaX;
	int glowaY;
	int ogonX;
	int ogonY;
	//char ruchy[512];
	int dlugosc_weza;
	int ktory_ruch;
	int fd;
	int id;
	unsigned char ostatni_ruch;
	
	int punkty;
} Gracz;

struct Gracz gracze[10];

int ile_graczy;
unsigned char moj_numer;
int moje_id;
unsigned char pierwszy = 'g';




	int dlugosc_weza = 10;
	char ruchy[512];
    	WINDOW * mainwin, * childwin;
	int pozX = 1;
	int pozY = 1;
	int ogonX = 1;
	int ogonY = 1;
	int ktory = 0;
	char ch;
		

	    int      width = 80, height = 20;



	int sock; 
	struct hostent *host;
	struct sockaddr_in server_addr; 
	ssize_t count;
	char buf[512];

void maluj (char gdzie)
{
				switch (gdzie)
				{
					case 3:
					{
					    if ( gracze[moje_id].glowaY > 0 )
						{
						--gracze[moje_id].glowaY;
						mvwaddstr(childwin, gracze[moje_id].glowaY, gracze[moje_id].glowaX, "X");
						}
					    break;
					}
					case 2:
					{
						if ( gracze[moje_id].glowaY < height )
						{
							++gracze[moje_id].glowaY;
							mvwaddstr(childwin, gracze[moje_id].glowaY, gracze[moje_id].glowaX, "#");
						}
						break;
					}
					case 4:
					{
					    if ( gracze[moje_id].glowaX > 0 )
						{
						--gracze[moje_id].glowaX;
						mvwaddstr(childwin, gracze[moje_id].glowaY, gracze[moje_id].glowaX, "^");
						}
					    break;
					}
					case 5:
					{
					    if ( gracze[moje_id].glowaX < width )
						{
						++gracze[moje_id].glowaX;
						mvwaddstr(childwin, gracze[moje_id].glowaY, gracze[moje_id].glowaX, ">");
						}
					    break;
					}
				}
}

void czysc (int ktory)
{
		if(ktory > (dlugosc_weza - 1))
				{
				
					switch (ruchy[ktory-dlugosc_weza])
					{
						case 3:
						{
							mvwaddstr(childwin, ogonY, ogonX, " ");
							--ogonY;
						    break;
						}
						case 2:
						{
								mvwaddstr(childwin, ogonY, ogonX, " ");
								++ogonY;
							break;
						}
						case 4:
						{
							mvwaddstr(childwin, ogonY, ogonX, " ");
							--ogonX;
						    break;
						}
						case 5:
						{
							mvwaddstr(childwin, ogonY, ogonX, " ");
							++ogonX;
						    break;
						}
					}
					
				}
}



void *print_message_function( void *ptr )
	{
	//unsigned char gdzie;
		while(1) {

			while ((count = read(sock, buf, sizeof(buf) - 1)))
			{
				if (count == -1) {
					/* EAGAIN, read all data */
					if (errno == EAGAIN)
						return 0;

					perror("read");
					break;
				}



			

				buf[count] = '\0';
				gracze[moje_id].ostatni_ruch = buf[0];
				

					if(moj_numer=='O')
				{
					read(sock,buf,sizeof(buf));
					moj_numer = buf[0];
					mvwaddstr(childwin, gracze[moje_id].glowaY, gracze[moje_id].glowaX, "@");

					if(moj_numer=='A')
						moje_id = 0;
					else if(moj_numer=='B')
						moje_id = 1;
					else
					{
						moje_id = 2;
					}

				}
				
				maluj(gracze[moje_id].ostatni_ruch);
				czysc(ktory);

					ruchy[ktory++] = gracze[moje_id].ostatni_ruch;
				 	wrefresh(childwin);
				break;
			}
		}
	return NULL;
	}

void *print_message_function1( void *ptr )
{
	while((ch = getch()) != 'q')
	{
	    send(sock,&ch,sizeof(ch), 0);
	}

	return NULL;
}



int main()
{
	//ustawianie poczatkowych wartosci
	for(int i=0;i<10;i++)
	{
		gracze[i].glowaX = (2 * i) + 1;
		gracze[i].glowaY = 1;
		gracze[i].ogonX = gracze[i].glowaX;
		gracze[i].ogonY = gracze[i].glowaY;
		gracze[i].dlugosc_weza = 8;
		gracze[i].ostatni_ruch = 2;
		
		gracze[i].punkty = 0;
	}

	moj_numer = 'O';
	ile_graczy = -1;
	moje_id = -1;




    int      rows  = 25, cols   = 80;
    int      x = (cols - width)  / 2;
    int      y = (rows - height) / 2;


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

    refresh();

/* Create independent threads each of which will execute function */
	     iret1 = pthread_create( &t1, NULL, print_message_function1, (void*) "dzoala1");
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
