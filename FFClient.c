/*
    Client program for Fabulous Fred
    This program connects to the server using sockets

    Sandra Lippert
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
// Sockets libraries
#include <netdb.h>
#include <arpa/inet.h>
// Custom libraries
#include "sockets.h"
#include "fatal_error.h"
//Ncurses library
#include <ncurses.h>
//Thread library
#include <pthread.h>
//game/player state enums
#include "Game_Codes.h"

#define BUFFER_SIZE 1024
#define COLORNUM 7

//Mutex for the thread synchronization variable
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//Condition variable for mutex2
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Structure to hold all the data that will be shared between threads
typedef struct thread_data_struct
{
    char *address;
    char *port;
    //The id of the player connected
    int connection_fd;
    int gameState;
    int playerState;
    char buffer[BUFFER_SIZE];
    int playersExpected;
    int color;
    int wrongColor;
    int newColor;
    int newRound;
} thread_data_t;

//Client structure - what the visualizing thread needs to know from the server
typedef struct socket_Communication
{
    int playersExpected;
    int playerState;
    int gameState;
    int color;
    int wrongColor;
    int newColor;
    int newRound;
} socketCommunication_t;


///// FUNCTION DECLARATIONS
void usage(char *program);
void quit();
void initBoard(int colorNum);
int positionColor(int color);
void youLose();
int startGame(thread_data_t *sharedData);
void *communicationThread(void *arg);
void drawBoard(thread_data_t *sharedData);
void playingLoop(thread_data_t *sharedData);

///// MAIN FUNCTION
int main(int argc, char *argv[])
{
    // Check the correct arguments
    if (argc != 3)
    {
        usage(argv[0]);
    }

    //Initialize thread structure
    thread_data_t *sharedData = NULL;
    sharedData = malloc(sizeof(thread_data_t));
    sharedData->address = argv[1];
    sharedData->port = argv[2];
    sharedData->gameState = GWAIT;
    sharedData->playerState = PWAIT;
    sharedData->color = 0;
    sharedData->wrongColor = 0;
    sharedData->newColor = 0;
    sharedData->playersExpected = 0;
    sharedData->newRound = 1;
    bzero(sharedData->buffer, BUFFER_SIZE);

    //Starts a thread for server communication
    startGame(sharedData);

    //Visual playfield with ncurses
    drawBoard(sharedData);

    pthread_exit(NULL);

    free(sharedData);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char *program)
{
    printf("Usage:\n");
    printf("\t%s {server_address} {port_number}\n", program);
    exit(EXIT_FAILURE);
}

/*
    End ncurses properly when quitting
*/
void quit()
{
    endwin();
}

/*
    Calculate the color blocks for the playing board
*/
void initBoard(int colorNum)
{
    for (int i = 1; i <= colorNum; i++)
    {
        //Color pairs (background, foreground) have to be initialized
        init_pair(i, 0, i);
        color_set(i, 0);

        //Calculate the x position of the color square
        int x = positionColor(i);
        //The color square
        char colorBox[3];
        sprintf(colorBox, "%c %d %c", 32, i, 32);
        mvaddstr(10, x, colorBox);
    }

    //Reset window color to white on black
    init_pair(0, 7, 0);
    color_set(0, 0);
}

/*
    Calculate the x position of the color blocks
*/
int positionColor(int color)
{
    //Padding to the left side of the window
    int offset = 2;
    int distance = 6;
    int x;

    x = offset + (distance * color);

    return x;
}

/*
    Initialize ncurses window as the user playing interface. 
    Get user input and show game updates.
*/
void drawBoard(thread_data_t *sharedData)
{
    initscr();
    cbreak();
    //Exit of the window will also close ncurses mode
    atexit(quit);
    //For the use of colors
    start_color();
    clear();
    curs_set(0);
    keypad(stdscr, TRUE);

    //Title
    mvaddstr(5, 5, "FABULOUS FRED");

    //Color buttons
    initBoard(COLORNUM);

    strcpy(sharedData->buffer, "Waiting for players to connect...");
    mvaddstr(17, 5, sharedData->buffer);
    refresh();
    
    //Wait for the game to begin
    while (sharedData->gameState == GWAIT)
    {
        if(sharedData->playerState == PWAIT)
        {
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&cond, &mutex);
            pthread_mutex_unlock(&mutex);
        }

        //The first player has to setup the game
        if(sharedData->playerState == FIRST)
        {
            pthread_mutex_lock(&mutex);
            strcpy(sharedData->buffer, "Setup the game! Number of players: ");
            curs_set(1);
            mvaddstr(17, 5, sharedData->buffer);
            scanw("%d", &sharedData->playersExpected);
            
            //Delete last line
            move(17, 5);
            deleteln();
            insertln();
            refresh();

            bzero(sharedData->buffer, BUFFER_SIZE);
            sprintf(sharedData->buffer, "Waiting for %d players to connect.", sharedData->playersExpected);
            curs_set(0);
            mvaddstr(17,5, sharedData->buffer);
            refresh();
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mutex);

            //Wait for an update from the server that the actual game can begin
            bzero(sharedData->buffer, BUFFER_SIZE);
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&cond, &mutex);
            refresh();
            pthread_mutex_unlock(&mutex);
            
            move(17, 5);
            deleteln();
            insertln();
            refresh();
        }
    }

    //Playing loop
    while (sharedData->gameState == GACTIVE)
    {   
        //Messages for the waiting players
        if (sharedData->playerState == PWAIT)
        {
            move(17, 5);
            deleteln();
            insertln();
            refresh();
            bzero(sharedData->buffer, BUFFER_SIZE);
            strcpy(sharedData->buffer, "Wait and remember!");
            mvaddstr(17, 5, sharedData->buffer);
            refresh();

            //Indicate the player that the following color is a new one
            if (sharedData->newColor == 1)
            {
                move(17, 5);
                deleteln();
                insertln();
                refresh();
                bzero(sharedData->buffer, BUFFER_SIZE);
                strcpy(sharedData->buffer, "New Color!");
                mvaddstr(14, 5, sharedData->buffer);
                refresh();
                sleep(1);
                deleteln();
                insertln();
                refresh();
            }

            //Indicate the player that the sequence begins from the start again
            if (sharedData->newRound == 1)
            {
                bzero(sharedData->buffer, BUFFER_SIZE);
                strcpy(sharedData->buffer, "New Round!"); 
                mvaddstr(14, 5, sharedData->buffer);
                refresh();
                sleep(1);
                deleteln();
                insertln();
                refresh();
            }
        }

        //Interaction with the active player
        if (sharedData->playerState == PACTIVE)
        {
            curs_set(1);
            bzero(sharedData->buffer, BUFFER_SIZE);
            move(17, 5);
            deleteln();
            insertln();
            refresh();

            //Indicate to remember a color from the sequence
            if (sharedData->newColor == 0)
            {
                strcpy(sharedData->buffer, "Your Turn! Pick a color: ");
                mvaddstr(17, 5, sharedData->buffer);
                pthread_mutex_lock(&mutex);
                scanw("%d", &sharedData->color);
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
                
                move(17, 5);
                deleteln();
                refresh();
            }
            //Indicate to add a new color
            if (sharedData->newColor == 1)
            {
                strcpy(sharedData->buffer, "Add a new color: ");
                mvaddstr(17, 5, sharedData->buffer);
                pthread_mutex_lock(&mutex);
                scanw("%d", &sharedData->color);
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
                move(17, 5);
                deleteln();
                insertln();
                refresh();
            }
        }

        curs_set(0);
        bzero(sharedData->buffer, BUFFER_SIZE);

        //All clients wait for game update from server
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        pthread_mutex_unlock(&mutex);

        //Show selected color to all players
        int x = positionColor(sharedData->color);
        color_set(sharedData->color, 0);

        //Indicate if color was remembered wrong or right
        if(sharedData->wrongColor == 0)
        {
            mvaddstr(12, x, "  :) ");
        }
        else if(sharedData->wrongColor == 1)
        {
            mvaddstr(12, x, "  X  ");
        }

        refresh();
        sleep(1);
        deleteln();
        insertln();
        refresh();

        //Reset colors for messages
        color_set(0, 0);

        bzero(sharedData->buffer, BUFFER_SIZE);
 
        //Check and print player's status
        if (sharedData->playerState == LOSER)
        {

            strcpy(sharedData->buffer, "You Lose!");
            mvaddstr(17, 5, sharedData->buffer);
            refresh();
            curs_set(1);
            getch();
            pthread_exit(NULL);
        }

        if (sharedData->playerState == WINNER)
        {
            move(17, 5);
            deleteln();
            insertln();
            refresh();
            strcpy(sharedData->buffer, "You Win!");
            mvaddstr(17, 5, sharedData->buffer);
            refresh();
            curs_set(1);
            getch();
        }
    }
}

/*
    Create thread for communication with server
*/
int startGame(thread_data_t *sharedData)
{
    pthread_t tid;

    // Create the thread for the server connection
    if (pthread_create(&tid, NULL, communicationThread, sharedData) == -1)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    return 0;
}

/*
    Thread for server communication
*/
void *communicationThread(void *arg)
{
    thread_data_t *sharedData = (thread_data_t *)arg;

    socketCommunication_t communication;

    //Connect to the server
    sharedData->connection_fd = connectSocket(sharedData->address, sharedData->port);

    //Get first update about game status and player status
    recv(sharedData->connection_fd, &communication, sizeof(socketCommunication_t), 0);
    pthread_mutex_lock(&mutex);
    sharedData->gameState = communication.gameState;
    sharedData->playerState = communication.playerState;
    sharedData->color = communication.color;
    sharedData->newColor = communication.newColor;
    sharedData->wrongColor = communication.wrongColor;
    sharedData->newRound = communication.newRound;
    
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    
    
    //Communication for first player
    if (communication.playerState == FIRST)
    {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        communication.playersExpected = sharedData->playersExpected;
        pthread_mutex_unlock(&mutex);
        //Send amount of players
        send(sharedData->connection_fd, &communication, sizeof(socketCommunication_t), 0);

        //receive following message, which changes the game state flag and the player state
        pthread_mutex_lock(&mutex);
        recv(sharedData->connection_fd, &communication, sizeof(socketCommunication_t), 0);
        sharedData->gameState = communication.gameState;
        sharedData->playerState = communication.playerState;
        sharedData->color = communication.color;
        sharedData->newColor = communication.newColor;
        sharedData->wrongColor = communication.wrongColor;
        sharedData->newRound = communication.newRound;

        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    //Actual playing loop
    while (sharedData->gameState == GACTIVE)
    {
        //Playing loop for active player
        if (communication.playerState == PACTIVE)
        {
            //Wait for visualizing thread to get user input
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&cond, &mutex);
            communication.color = sharedData->color;
            pthread_mutex_unlock(&mutex);

            send(sharedData->connection_fd, &communication, sizeof(socketCommunication_t), 0);
        }

        //Receives the Update
        recv(sharedData->connection_fd, &communication, sizeof(socketCommunication_t), 0);
        
        pthread_mutex_lock(&mutex);
        sharedData->gameState = communication.gameState;
        sharedData->playerState = communication.playerState;
        sharedData->color = communication.color;
        sharedData->newColor = communication.newColor;
        sharedData->wrongColor = communication.wrongColor;
        sharedData->newRound = communication.newRound;
        //Signal the visualizing thread, that the game info was updated
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        //Quit thread if player lost
        if (communication.playerState == LOSER)
        {
            pthread_exit(NULL);
        }

        //Quit thread if player won
        if (communication.playerState == WINNER)
        {
            pthread_exit(NULL);
        }
    }

    // Close the socket
    close(sharedData->connection_fd);

    pthread_exit(EXIT_SUCCESS);
}

