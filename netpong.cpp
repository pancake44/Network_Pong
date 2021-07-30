/*

 * netpong.cpp

 * Authors: Harry Snow (hsnow), (gfernan2)

 *

 * Peer-to-peer pong game using UDP.

 *

 * Assigned port: 41043, 41017

 *

 * Adapted in part from starter code, Beej's Guide to Network

 * Programming, and pg1-3.

 *

 */




#include <ncurses.h>

#include <stdlib.h>

#include <pthread.h>

#include <unistd.h>

#include <string.h>

#include <sys/time.h>

#include <stdio.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <netdb.h>

#include <arpa/inet.h>

#include <iostream>

#include <signal.h>

#define WIDTH 43

#define HEIGHT 21

#define PADLX 1

#define PADRX WIDTH - 2




typedef uint32_t socklen_t;




// Global variables recording the state of the game

typedef struct {

	int ballX;

	int ballY;

	int dx;

	int dy;

	int padLY;

	int padRY;

	int scoreL;

	int scoreR;

	int roundsPlayed;

} GameState;




GameState global_state = {0,0,0,0,0,0,0,0,0};




pthread_t pth, pth1, pth2;

int rounds = 0;

bool gameOver = false;




// ncurses window

WINDOW *win;




// Global networking vars

// Host

int h_sockfd;

struct addrinfo h_hints, * h_servinfo, * h_p;

int h_rv;




struct sockaddr_storage theirinfo;

struct sockaddr_storage clientinfo;

socklen_t addr_len = sizeof(clientinfo);




int refresh_global;

// Client

int c_sockfd;

struct addrinfo c_hints, * c_servinfo, * c_p;

int c_rv;




bool isHost;




pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;




void printLog(char * message){




	if(isHost){

		FILE * f = fopen("hlog", "a");

		fprintf(f, "%s\n", message);

		fclose(f);

	}

	else{

		FILE * f = fopen("clog", "a");

		fprintf(f, "%s\n", message);

		fclose(f);

	}

}







/* Draw the current game state to the screen

 * ballX: X position of the ball

 * ballY: Y position of the ball

 * padLY: Y position of the left paddle

 * padRY: Y position of the right paddle

 * scoreL: Score of the left player

 * scoreR: Score of the right player

 */

void draw(int ballX, int ballY, int padLY, int padRY, int scoreL, int scoreR) {

    // Center line

    int y;

    for(y = 1; y < HEIGHT-1; y++) {

        mvwaddch(win, y, WIDTH / 2, ACS_VLINE);

    }

    // Score

    mvwprintw(win, 1, WIDTH / 2 - 3, "%2d", scoreL);

    mvwprintw(win, 1, WIDTH / 2 + 2, "%d", scoreR);

    // Ball

    mvwaddch(win, ballY, ballX, ACS_BLOCK);

    // Left paddle

    for(y = 1; y < HEIGHT - 1; y++) {

        int ch = (y >= padLY - 2 && y <= padLY + 2)? ACS_BLOCK : ' ';

        mvwaddch(win, y, PADLX, ch);

    }

    // Right paddle

    for(y = 1; y < HEIGHT - 1; y++) {

        int ch = (y >= padRY - 2 && y <= padRY + 2)? ACS_BLOCK : ' ';

        mvwaddch(win, y, PADRX, ch);

    }

    // Print the virtual window (win) to the screen

    wrefresh(win);

    // Finally erase ball for next time (allows ball to move before next refresh)

    mvwaddch(win, ballY, ballX, ' ');

}




/* Return ball and paddles to starting positions

 * Horizontal direction of the ball is randomized

 */

void reset() {

    global_state.ballX = WIDTH / 2;

    global_state.padLY = global_state.padRY = global_state.ballY = HEIGHT / 2;

    // dx is randomly either -1 or 1

    global_state.dx = (rand() % 2) * 2 - 1;

    global_state.dy = 0;

    // Draw to reset everything visually

    draw(global_state.ballX, global_state.ballY, global_state.padLY, global_state.padRY, global_state.scoreL, global_state.scoreR);

}




/* Display a message with a 3 second countdown

 * This method blocks for the duration of the countdown

 * message: The text to display during the countdown

 */

void countdown(const char *message) {

    int h = 4;

    int w = strlen(message) + 4;

    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);

    box(popup, 0, 0);

    mvwprintw(popup, 1, 2, message);

    int countdown;

    for(countdown = 3; countdown > 0; countdown--) {

        mvwprintw(popup, 2, w / 2, "%d", countdown);

        wrefresh(popup);

        sleep(1);

    }

    wclear(popup);

    wrefresh(popup);

    delwin(popup);

    global_state.padLY = global_state.padRY = HEIGHT / 2; // Wipe out any input that accumulated during the delay

}




// Game state network functions




GameState recvH(){




	int numbytes;

	GameState temp;




	if((numbytes = recvfrom(h_sockfd, &temp, sizeof(GameState), 0, (struct sockaddr *) & clientinfo, & addr_len)) < 0){

		perror("recv");

		exit(41);

	}

			

	return temp;	

}




GameState recvC(){




	int numbytes;

	GameState temp;




	if((numbytes = recvfrom(c_sockfd, &temp, sizeof(GameState), 0, c_p->ai_addr, & addr_len)) < 0){

		perror("recv");	

		exit(42);

	}




	return temp;

}




void sendH(){




	GameState temp = global_state;




	int numbytes;




	if((numbytes = sendto(h_sockfd, &temp, sizeof(GameState), 0, (struct sockaddr *) &clientinfo, addr_len)) < 0){

		perror("send");

		exit(43);

	}	

}




void sendC(){




	GameState temp = global_state;




	int numbytes;




	if((numbytes = sendto(c_sockfd, &temp, sizeof(GameState), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

		perror("send");	

		exit(44);

	}

}




/* Perform periodic game functions:

 * 1. Move the ball

 * 2. Detect collisions

 * 3. Detect scored points and react accordingly

 * 4. Draw updated game state to the screen

 */

void tock() {

    // Move the ball

    global_state.ballX += global_state.dx;

    global_state.ballY += global_state.dy;

    

    // Check for paddle collisions

    // padY is y value of closest paddle to ball

    int padY = (global_state.ballX < WIDTH / 2) ? global_state.padLY : global_state.padRY;

    // colX is x value of ball for a paddle collision

    int colX = (global_state.ballX < WIDTH / 2) ? PADLX + 1 : PADRX - 1;

    if(global_state.ballX == colX && abs(global_state.ballY - padY) <= 2) {

        // Collision detected!

        global_state.dx *= -1;

        // Determine bounce angle

        if(global_state.ballY < padY) global_state.dy = -1;

        else if(global_state.ballY > padY) global_state.dy = 1;

        else global_state.dy = 0;

    }




    // Check for top/bottom boundary collisions

    if(global_state.ballY == 1) global_state.dy = 1;

    else if(global_state.ballY == HEIGHT - 2) global_state.dy = -1;

    

    // Score points

	int numbytes;

    if(global_state.ballX <= 1 && isHost) {

		char message[BUFSIZ] = "reset";

			if((numbytes = sendto(h_sockfd, message, strlen(message), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

				perror("send");

				exit(51);

			}

	

        global_state.scoreR += 1;

        if(global_state.scoreR == 2){

			global_state.scoreR = 0;

			global_state.roundsPlayed++;

		}




		reset();

		sendH();

        countdown("SCORE -->");

    } 




	else if(global_state.ballX >= WIDTH - 2 && !isHost) {

        char message[BUFSIZ] = "reset";

		if((numbytes = sendto(c_sockfd, message, strlen(message), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

			perror("send");

			exit(25);

		}

		

		global_state.scoreL += 1;

		if(global_state.scoreL == 2){

			global_state.scoreL = 0;

			global_state.roundsPlayed++;

		}

        

		reset();

		sendC();

        countdown("<-- SCORE");

    }

	

    // Finally, redraw the current state

    draw(global_state.ballX, global_state.ballY, global_state.padLY, global_state.padRY, global_state.scoreL, global_state.scoreR);

}




// sends game state periodically

void *sendNetwork(void * args){

	int numbytes;




	// are locks needed for the sends?

	while(global_state.roundsPlayed < rounds){

		if(!isHost){

			char message[BUFSIZ] = "game_state";

			if((numbytes = sendto(c_sockfd, message, strlen(message), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

				perror("send");

				exit(25);

			}

		

			sendC();

		}

		else{

		

			theirinfo.ss_family = AF_UNSPEC;

			char message[BUFSIZ] = "game_state";

			if((numbytes = sendto(h_sockfd, message, strlen(message), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

				perror("send");

				exit(51);

			}

		

			sendH();

		}

		usleep(120); //can maybe be lowered

	}

}




/* Listen to keyboard input

 * Updates global pad positions

 */

void *listenInput(void *args) {

    while(global_state.roundsPlayed < rounds && !gameOver) {

        switch(getch()) {

            case KEY_UP:

				if(isHost){

					pthread_mutex_lock(& lock);

					global_state.padRY--;

					pthread_mutex_unlock(& lock);

				}

        	    break;

            case KEY_DOWN:	

				if(isHost){

					pthread_mutex_lock(& lock);

					global_state.padRY++;

					pthread_mutex_unlock(& lock);

        		}

		   		break;

            case 'w':

				if(!isHost){	

					pthread_mutex_lock(& lock);

					global_state.padLY--;

					pthread_mutex_unlock(& lock);

            	}

				break;

            case 's': 

				if(!isHost){

					pthread_mutex_lock(& lock);

					global_state.padLY++;

					pthread_mutex_unlock(& lock);

        		}	

				break;

          default: break;

      	}	

    }       

    return NULL;

}




void initNcurses() {

    initscr();

    cbreak();

    noecho();

	timeout(100);

    keypad(stdscr, TRUE);

    curs_set(0);

    refresh();

    win = newwin(HEIGHT, WIDTH, (LINES - HEIGHT) / 2, (COLS - WIDTH) / 2);

    box(win, 0, 0);

    mvwaddch(win, 0, WIDTH / 2, ACS_TTEE);

    mvwaddch(win, HEIGHT-1, WIDTH / 2, ACS_BTEE);

}




void setupHost(char * port){




	memset(& h_hints, 0, sizeof(h_hints));

	h_hints.ai_family = AF_UNSPEC;

	h_hints.ai_socktype = SOCK_DGRAM;

	h_hints.ai_flags = AI_PASSIVE;




	if((h_rv = getaddrinfo(NULL, port, & h_hints, & h_servinfo)) != 0)

		exit(1);




	for(h_p = h_servinfo; h_p != NULL; h_p = h_p->ai_next){

		

		if((h_sockfd = socket(h_p->ai_family, h_p->ai_socktype, h_p->ai_protocol)) == -1)

			continue;




		if(bind(h_sockfd, h_p->ai_addr, h_p->ai_addrlen) == -1)

			continue;




		break;

	}

	

	if(!h_p)

		exit(4);




	int on  =1;

	setsockopt(h_sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));




	freeaddrinfo(h_servinfo);

}




void setupC(char * host, char * port){




	memset(& c_hints, 0, sizeof(c_hints));

	c_hints.ai_family = AF_UNSPEC;

	c_hints.ai_socktype = SOCK_DGRAM;




	if((c_rv = getaddrinfo(host, port, & c_hints, & c_servinfo)) != 0)

		exit(1);

	

	for(c_p = c_servinfo; c_p != NULL; c_p = c_p->ai_next){




		if((c_sockfd = socket(c_p->ai_family, c_p->ai_socktype, c_p->ai_protocol)) == -1)

			continue;




		break;

	}




	if(!c_p)

		exit(3);




	int on = 1;

	setsockopt(c_sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

}	




void * listenNetworkH(void * args){




	char recvbuffer[BUFSIZ] = {0};

	int numbytes;




	while(1 && !gameOver){

		

		bzero(recvbuffer, BUFSIZ);

		

		char mess[BUFSIZ] = "here";

		//printLog(mess);




		if((numbytes = recvfrom(h_sockfd, recvbuffer, BUFSIZ, 0, (struct sockaddr *) & clientinfo, & addr_len)) < 0){

			perror("recv");

			exit(91);

		}




		char mess2[BUFSIZ] = "recieved";

		//printLog(mess2);




		if(!strcmp(recvbuffer, "game_state")){

			pthread_mutex_lock(&lock);

			GameState temp = recvH();




			if(global_state.ballX > WIDTH/2){

				global_state.dx = temp.dx;

				global_state.dy = temp.dy;

				global_state.ballX = temp.ballX;

				global_state.ballY = temp.ballY;

			}




			global_state.padLY = temp.padLY;

			global_state.scoreL = temp.scoreL;

			pthread_mutex_unlock(&lock);

		}

		

		else if(!strcmp(recvbuffer, "reset")){

			pthread_mutex_lock(&lock);




			GameState temp = recvH();

        	reset();		

			if(global_state.roundsPlayed < temp.roundsPlayed)

				global_state.scoreR = 0;

		

			global_state.roundsPlayed = temp.roundsPlayed;




			global_state.dx = temp.dx;

        	countdown("<-- SCORE");




    		struct timeval tv;

    	    gettimeofday(&tv,NULL);

    	    unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;

			

    	    gettimeofday(&tv,NULL);

    	    unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;

    	    unsigned long toSleep = refresh_global - (after - before);

    	    // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()

    	    // In that case it's MUCH bigger because of overflow!

    	    if(toSleep > refresh_global) toSleep = refresh_global;

   		    usleep(toSleep); // Sleep exactly as much as is necessary		




			pthread_mutex_unlock(&lock);

		}




		else if(!strcmp(recvbuffer, "close")){

			char message2[BUFSIZ] = "close";

			if((numbytes = sendto(h_sockfd, message2, strlen(message2), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

					perror("send");

					exit(123);

			}

			break;

		}

	}

	return NULL;

}




void * listenNetworkC(void * args){




	char recvbuffer[BUFSIZ] = {0};

	int numbytes;




	while(!gameOver){

		

		bzero(recvbuffer, BUFSIZ);

	

		char mess[BUFSIZ] = "waiting to receive";

		//printLog(mess);




		if((numbytes = recvfrom(c_sockfd, recvbuffer, BUFSIZ, 0, c_p->ai_addr, & addr_len)) < 0){

			perror("recv");

			exit(93);

		}




		char mess2[BUFSIZ] = "recieved";

		//printLog(mess2);




		if(!strcmp(recvbuffer, "game_state")){

			pthread_mutex_lock(& lock);

			GameState temp = recvC();




			if(global_state.ballX < WIDTH/2){

				global_state.dx = temp.dx;

				global_state.dy = temp.dy;

				global_state.ballX = temp.ballX;

				global_state.ballY = temp.ballY;

			}

			

			global_state.padRY = temp.padRY;

			global_state.scoreR = temp.scoreR;

			pthread_mutex_unlock(& lock);

		}

	

		else if(!strcmp(recvbuffer, "reset")){

			pthread_mutex_lock(&lock);




        	GameState temp = recvC();

        	reset();




			if(global_state.roundsPlayed < temp.roundsPlayed)

				global_state.scoreL = 0;




			global_state.roundsPlayed = temp.roundsPlayed;

			global_state.dx = temp.dx;

        	countdown("SCORE -->");




    		struct timeval tv;

    	    gettimeofday(&tv,NULL);

    	    unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;

			

    	    gettimeofday(&tv,NULL);

    	    unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;

    	    unsigned long toSleep = refresh_global - (after - before);

    	    // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()

    	    // In that case it's MUCH bigger because of overflow!

    	    if(toSleep > refresh_global) toSleep = refresh_global;

   		    usleep(toSleep); // Sleep exactly as much as is necessary

			

			pthread_mutex_unlock(&lock);




		}




		else if(!strcmp(recvbuffer, "close")){

			char message2[BUFSIZ] = "close";	

			if((numbytes = sendto(c_sockfd, message2, strlen(message2), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

					perror("send");

					exit(124);

			}

			break;

		}

	}




	return NULL;

}







// not 100% functional with sending message for client to exit

void intHandler(int signal){

	int numbytes;

	char message2[BUFSIZ] = "close";

		if(isHost){

			if((numbytes = sendto(h_sockfd, message2, strlen(message2), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

					perror("send");

					exit(123);

			}

		}

 

		else{

			if((numbytes = sendto(c_sockfd, message2, strlen(message2), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

					perror("send");

					exit(124);

			}

		}




	// Clean up

   	gameOver = true;

   	endwin();

	exit(0);

}




void usage(){




	printf("***TO CONNECT AS HOST***\n./netpong --host [PORT]\n\n");

	printf("***TO CONNECT AS OTHER PLAYER***\n./netpong [HOSTNAME] [PORT]\n\n");

	exit(0);

}




int main(int argc, char *argv[]) {

    // Process args

    // refresh is clock rate in microseconds

    // This corresponds to the movement speed of the ball

    

	if(argc != 3)

		usage();




    int refresh;

	char h_connection[BUFSIZ] = {0};

	char c_connection[BUFSIZ] = {0};

	int numbytes;




	if(!(strcmp(argv[1], "--host"))){




		isHost = true;




    	char difficulty[10]; 

    	printf("Please select the difficulty level (easy, medium or hard): ");

    	scanf("%s", &difficulty);

    	if(strcmp(difficulty, "easy") == 0) refresh = 80000;

    	else if(strcmp(difficulty, "medium") == 0) refresh = 40000;

    	else if(strcmp(difficulty, "hard") == 0) refresh = 20000;




    	printf("Please enter the maximum number of rounds to play: ");

    	scanf("%d", &rounds);




		setupHost(argv[2]);




		std::cout<<"Waiting for challengers on port " << argv[2] <<std::endl;




		if((numbytes = recvfrom(h_sockfd, h_connection, BUFSIZ, 0, (struct sockaddr *) & clientinfo, & addr_len)) < 0){

			perror("recv");

			exit(23);

		}




		char* message = difficulty;

		if((numbytes = sendto(h_sockfd, message, strlen(message), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

			perror("send");

			exit(24);

		}




		int roundsToSend = htonl(rounds);

		if((numbytes = sendto(h_sockfd, & roundsToSend, sizeof(roundsToSend), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

			perror("send");

			exit(27);

		}

	}




    else{

		isHost = false;

		setupC(argv[1], argv[2]);

		

		char message[BUFSIZ] = "ok";

		if((numbytes = sendto(c_sockfd, message, strlen(message), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

			perror("send");

			exit(25);

		}




		bzero(message, BUFSIZ);

		if((numbytes = recvfrom(c_sockfd, c_connection, BUFSIZ, 0, c_p->ai_addr, & addr_len)) < 0){

			perror("recv");	

			exit(26);

		}

		




		bzero(message, BUFSIZ);

		if((numbytes = recvfrom(c_sockfd, & rounds, sizeof(rounds), 0, c_p->ai_addr, & addr_len)) < 0){

			perror("recv");

			exit(28);

		}

		

		rounds = ntohl(rounds);




		if(strcmp(c_connection, "easy") == 0) refresh = 80000;

    	else if(strcmp(c_connection, "medium") == 0) refresh = 40000;

    	else if(strcmp(c_connection, "hard") == 0) refresh = 20000;

		

		refresh_global = refresh;

	}




    // Set up ncurses environment 

 	if((isHost && !strcmp(h_connection, "ok")) || (!isHost && (!strcmp(c_connection, "easy") || !strcmp(c_connection, "medium") || !strcmp(c_connection, "hard")))){

    	initNcurses();

   		signal(SIGINT, intHandler);

    	

		// Set starting game state and display a countdown

    	reset();

    	countdown("Starting Game");

    

    	// Listen to keyboard input in a background thread

    	pthread_create(&pth, NULL, listenInput, NULL);




		// send game state periodically in network thread

		pthread_create(&pth1, NULL, sendNetwork, NULL);




		if(isHost)

			pthread_create(&pth2, NULL, listenNetworkH, NULL);

		else

			pthread_create(&pth2, NULL, listenNetworkC, NULL);




    	// Main game loop executes tock() method every REFRESH microse

    	struct timeval tv;

    	while(global_state.roundsPlayed < rounds){

    	    gettimeofday(&tv,NULL);

    	    unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;

			

			pthread_mutex_lock(& lock);

    	    tock(); // Update game state

			pthread_mutex_unlock(& lock);




    	    gettimeofday(&tv,NULL);

    	    unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;

    	    unsigned long toSleep = refresh - (after - before);

    	    // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()

    	    // In that case it's MUCH bigger because of overflow!

    	    if(toSleep > refresh) toSleep = refresh;

   		    usleep(toSleep); // Sleep exactly as much as is necessary

   		}

		char message2[BUFSIZ] = "close";

		if(isHost){

			if((numbytes = sendto(h_sockfd, message2, strlen(message2), 0, (struct sockaddr *) & clientinfo, addr_len)) < 0){

					perror("send");

					exit(123);

			}

		}

 

		else{

			if((numbytes = sendto(c_sockfd, message2, strlen(message2), 0, c_p->ai_addr, c_p->ai_addrlen)) < 0){

					perror("send");

					exit(124);

			}

		}




		// Clean up

   		gameOver = true;

		pthread_join(pth1, NULL);

		pthread_join(pth2, NULL);

		pthread_join(pth, NULL);

   		endwin();

	}

	

    return 0;

}
