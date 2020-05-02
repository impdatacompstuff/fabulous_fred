/*
    Server program for Fabulous Fred, looping for clients, using sockets
    
    Sandra Lippert

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Sockets libraries
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
// Custom libraries
#include "sockets.h"
#include "fatal_error.h"
//Thread library
#include <pthread.h>
//game/player state enums
#include "Game_Codes.h"

#define BUFFER_SIZE 1024
#define MAX_QUEUE 5
#define PLAYERS 3

//Mutex for the playersConnected variable
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
//Mutex for the thread synchronization variable
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
//Condition variable for mutex2
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

//The struct to be sent to the client
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

//Player struct
typedef struct player_info_struct
{
    //Boolean, correct if player already lost and is disconnected
    int isOut;
    int client_fd;
    socketCommunication_t *clientData;
} player_t;

// Structure to hold all the data that will be shared between threads for the server
typedef struct thread_data_struct
{
    int server_fd;
    //The number of players that are already connected
    int playersConnected;
    //The numbers of expected players for this game
    int playersExpected;
    int playerID;
    int gameState;
    //The color sequence to remember so far
    int *colorSequence;
    int sequenceSize;
    //The least remembered color by current player
    int color;
    int wrongColor;
    int playerTurn;
    int turnCounter;
    int sentTo;
    int losers;
    int newColor;
    int newRound;
    //Player info array
    player_t **playerArray;
} thread_data_t;

///// FUNCTION DECLARATIONS
void usage(char *program);
void waitForConnections(int server_fd);
void *attendClient(void *arg);
void startGame(thread_data_t *sharedData);
void setupGame(thread_data_t *sharedData);
void compareColors(thread_data_t *sharedData, int playerID, int index);
void whoseTurn(thread_data_t *sharedData, int playerID);
int checkIfWinner(thread_data_t *sharedData, int playerID);
void addColor(thread_data_t *sharedData, int playerID, int index);
int checkColor(thread_data_t *sharedData, int index);
void freeAll(thread_data_t *sharedData);


///// MAIN FUNCTION
int main(int argc, char *argv[])
{
    int server_fd;

    printf("\n=== FABULOUS FRED SERVER STARTING ===\n");

    // Check the correct arguments
    if (argc != 2)
    {
        usage(argv[0]);
    }

    // Show the IPs assigned to this computer
    printLocalIPs();

    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);

    //setupHandlers();

    // Listen for connections from the clients
    waitForConnections(server_fd);

    // Close the socket
    close(server_fd);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char *program)
{
    printf("Usage:\n");
    printf("\t%s {port_number}\n", program);
    exit(EXIT_FAILURE);
}

/*
    Main loop to wait for incomming connections
*/

void waitForConnections(int server_fd)
{

    //Initialize struct for sharing between threads
    thread_data_t *sharedData = NULL;
    sharedData = malloc(sizeof(thread_data_t));
    sharedData->server_fd = server_fd;
    sharedData->playersExpected = 1;
    sharedData->playersConnected = 0;
    sharedData->playerTurn = 0;
    sharedData->playerID = 0;
    sharedData->turnCounter = 0;
    sharedData->gameState = GWAIT;
    sharedData->wrongColor = 0;
    sharedData->sequenceSize = 0;
    sharedData->color = 0;
    sharedData->losers = 0;
    sharedData->newColor = 0;
    sharedData->newRound = 0;
    //Allocate space for one player
    sharedData->playerArray = malloc(__SIZEOF_POINTER__);

    //Array of threads
    pthread_t tid[1];

    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    // Get the size of the structure to store client information
    client_address_size = sizeof client_address;

    //Server waits for first player to  connect
    //Allocate player struct
    sharedData->playerArray[sharedData->playersConnected] = malloc(sizeof(player_t));

    //Connect with the first client
    sharedData->playerArray[sharedData->playersConnected]->client_fd = accept(sharedData->server_fd, (struct sockaddr *)&client_address, &client_address_size);
    if (sharedData->playerArray[sharedData->playersConnected]->client_fd == -1)
    {
        fatalError("accept");
    }

    // Get the data from the client
    inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
    printf("Received incomming connection from %s on port %d\n", client_presentation, client_address.sin_port);

    //Communication for the first player to set up the game
    setupGame(sharedData);

    printf("playersexpected: %d\n", sharedData->playersExpected);

    sharedData->playersConnected++;

    //Server loops for the other expected players.
    while (sharedData->playersConnected < sharedData->playersExpected)
    {
        //Allocate player struct
        sharedData->playerArray[sharedData->playersConnected] = malloc(sizeof(player_t));

        sharedData->playerArray[sharedData->playersConnected]->client_fd = accept(sharedData->server_fd, (struct sockaddr *)&client_address, &client_address_size);
        if (sharedData->playerArray[sharedData->playersConnected]->client_fd == -1)
        {
            fatalError("accept");
        }

        sharedData->playersConnected++;

        // Get the data from the client
        inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
        printf("Received incomming connection from %s on port %d\n", client_presentation, client_address.sin_port);
    }

    // Create threads for the server connection
    for (int i = 0; i < sharedData->playersExpected; i++)
    {
        if (pthread_create(&tid[i], NULL, &attendClient, sharedData) == -1)
        {
            fprintf(stderr, "ERROR: pthread_create\n");
            exit(EXIT_FAILURE);
        }
    }

    //Wait for threads to finish
    for (int i = 0; i < sharedData->playersExpected; i++)
    {
        pthread_join(tid[i], NULL);
    }
    
    //Free Memory
    freeAll(sharedData);
}

/*
    Thread with game logic
*/
void *attendClient(void *arg)
{
    thread_data_t *sharedData = (thread_data_t *)arg;

    //Assign an individual client to the thread
    pthread_mutex_lock(&mutex1);
    int playerID = sharedData->playerID;
    sharedData->playerID++;
    pthread_mutex_unlock(&mutex1);

    //Allocate the structure to send to the client
    sharedData->playerArray[playerID]->clientData = malloc(sizeof(socketCommunication_t));


    //Prepare data for first send()
    sharedData->playerArray[playerID]->clientData->playerState = PWAIT;
    sharedData->playerArray[sharedData->playerTurn]->clientData->playerState = PACTIVE; 
    sharedData->playerArray[playerID]->clientData->gameState = GACTIVE;
    sharedData->playerArray[playerID]->clientData->newColor = 0;
    sharedData->playerArray[playerID]->clientData->color = 0;
    sharedData->gameState = GACTIVE;
    //Player is not yet marked "kicked out" and the beginning of the game
    sharedData->playerArray[playerID]->isOut = 1;
    //Default is: not ready to send
    sharedData->sentTo = -1;
    //Initialize color array
    sharedData->colorSequence = malloc(sizeof(int));

    //Initial sending, the game begins
    send(sharedData->playerArray[playerID]->client_fd, sharedData->playerArray[playerID]->clientData, sizeof(socketCommunication_t), 0);

    //Variable to iterate through the colorSequence array
    int index = 0;

    //START GAME LOOP
    while (sharedData->gameState == GACTIVE && (checkIfWinner(sharedData, playerID) != 0))
    {
        //For the active player
        if (sharedData->playerArray[playerID]->clientData->playerState == PACTIVE)
        {   
            //Checks if next send() will come with a new round
            if(index == sharedData->sequenceSize)
            {
                sharedData->newRound = 1;
            }
            else
            {
                sharedData->newRound = 0;
            }

            //If it's the first color to add or a new color for the array, we will enter here
            if (sharedData->sequenceSize == index)
            {
                addColor(sharedData, playerID, index);
                //Now it's another player's turn
                whoseTurn(sharedData, playerID);

                //Reset colorSequence index counter
                index = 0;
                //Reset newColor variable
                sharedData->newColor = 0;
            }
            else
            {
                //If the index already has a color, enter here
                compareColors(sharedData, playerID, index);
                index++;   
                //Check if player is about to enter last color to be compared
                if (index == sharedData->sequenceSize)
                {
                    sharedData->newColor = 1;
                }
            }

            //Now ready to prepare the results of this round
            pthread_mutex_lock(&mutex2);
            sharedData->sentTo = 0;
            //Send signal to waiting clients
            for(int i = 0; i < (sharedData->playersConnected - 1); i++)
            {
                pthread_cond_signal(&cond);
            }
            pthread_mutex_unlock(&mutex2);
        }

        //Make clients wait for the readiness of the data to be sent
        pthread_mutex_lock(&mutex2);
        while (sharedData->sentTo < 0)
        {
            //Block while until signal comes from active player thread
            pthread_cond_wait(&cond, &mutex2);
        }
        
        //Prepare data for the client
        sharedData->playerArray[playerID]->clientData->color = sharedData->color;
        sharedData->playerArray[playerID]->clientData->wrongColor = sharedData->wrongColor;
        sharedData->playerArray[playerID]->clientData->newColor = sharedData->newColor;
        sharedData->playerArray[playerID]->clientData->newRound = sharedData->newRound;
        pthread_mutex_unlock(&mutex2);

        //Check if player is Winner!
        if (checkIfWinner(sharedData, playerID) == 0)
        {
            printf("Nr %d: WIN!\n", playerID);
        }

        //Data is sent to all clients
        send(sharedData->playerArray[playerID]->client_fd, sharedData->playerArray[playerID]->clientData, sizeof(socketCommunication_t), 0);
        
        //sentTo variable controls that everyone has got an update
        pthread_mutex_lock(&mutex2);
        sharedData->sentTo++;
        pthread_mutex_unlock(&mutex2);

        printf("Update sent to player %d , sentTo = %d\n", playerID, sharedData->sentTo);

        //Last thread updates the checking variable and informs other threads to go on
        pthread_mutex_lock(&mutex2);
        if (sharedData->sentTo == sharedData->playersConnected)
        {
            //Reset synchronization variable
            sharedData->sentTo = -1;
            //Signal to all threads that the synchronization variable has been changed
            for (int i = 0; i < (sharedData->playersConnected - 1); i++)
            {
                pthread_cond_signal(&cond);
            }

            printf("sentTo resettet!\n");
        }
        pthread_mutex_unlock(&mutex2);

        //Check if sentTo variable was resetted and if threads can go on with the playing loop
        while (sharedData->sentTo > 0)
        {
            //Block thread until signal was received from last thread
            pthread_mutex_lock(&mutex2);
            pthread_cond_wait(&cond, &mutex2);
            pthread_mutex_unlock(&mutex2);
        }

        //Kick out the loser
        if (sharedData->playerArray[playerID]->clientData->playerState == LOSER)
        {
            pthread_mutex_lock(&mutex1);
            sharedData->playersConnected--;
            pthread_mutex_unlock(&mutex1);
            //pthread_exit(NULL);
            break;
        }
    }

    if (sharedData->gameState == END)
    {
        printf("Game ended!\n");
    }

    pthread_exit(NULL);
}

/*
    Communication with first client to setup the number of players
*/
void setupGame(thread_data_t *sharedData)
{
    socketCommunication_t clientData;
    clientData.playerState = FIRST;
    clientData.gameState = GWAIT;

    send(sharedData->playerArray[0]->client_fd, &clientData, sizeof(socketCommunication_t), 0);

    recv(sharedData->playerArray[0]->client_fd, &clientData, sizeof(socketCommunication_t), 0);
    sharedData->playersExpected = clientData.playersExpected;
}

/*
    Compares the player's color with the colorsequence at given index
*/
int checkColor(thread_data_t *sharedData, int index)
{
    int result = 1;

    if (sharedData->color == sharedData->colorSequence[index])
    {
        sharedData->wrongColor = 0;
        result = 1;
    }
    else if (sharedData->color != sharedData->colorSequence[index])
    {
        sharedData->wrongColor = 1;
        result = 0;
    }
    return result;

}

/*
    Increase the counter of turns taken and calculate the new active player, update status
*/
void whoseTurn(thread_data_t *sharedData, int playerID)
{    
    int i = 0;

    while (i < sharedData->playersExpected)
    {
        sharedData->turnCounter++;
        sharedData->playerTurn = sharedData->turnCounter % sharedData->playersExpected;

        //If the player is still in the game
        if (sharedData->playerArray[sharedData->playerTurn]->isOut == 1)
        {
            //Desactivate status of current player
            sharedData->playerArray[playerID]->clientData->playerState = PWAIT;
            //Activate status of next player
            sharedData->playerArray[sharedData->playerTurn]->clientData->playerState = PACTIVE;

            break;
        }

        i++;
    }
}

/*
    Checks if player is last one in the game and thus the winner
*/
int checkIfWinner(thread_data_t *sharedData, int playerID)
{
    //Player wins when he is the last one standing, except when he plays alone, then he cant win
    if (sharedData->playersExpected != 1 && (sharedData->playersExpected - sharedData->losers == 1) && sharedData->playerArray[playerID]->clientData->playerState != LOSER)
    {
        sharedData->playerArray[playerID]->clientData->playerState = WINNER;
        sharedData->gameState = END;
        sharedData->playerArray[playerID]->clientData->gameState = END;
        return 0;
    }

    return 1;
}

/*
    Add a new color to the colorSequence array and update status and tags
*/
void addColor(thread_data_t *sharedData, int playerID, int index)
{
    recv(sharedData->playerArray[playerID]->client_fd, sharedData->playerArray[playerID]->clientData, sizeof(socketCommunication_t), 0);
    sharedData->color = sharedData->playerArray[playerID]->clientData->color;

    //If it is not the first color, reallocate the memory of colorSequence[]
    if (index != 0)
    {
        sharedData->colorSequence = realloc(sharedData->colorSequence, sizeof(int));
    }

    sharedData->colorSequence[index] = sharedData->color;
    sharedData->sequenceSize++;
    sharedData->wrongColor = 0;
    sharedData->playerArray[playerID]->clientData->wrongColor = 0;
    sharedData->playerArray[playerID]->clientData->playerState = PWAIT;
}

/*
    Receives color from active player, and evaluates its comparison
*/
void compareColors(thread_data_t *sharedData, int playerID, int index)
{
    recv(sharedData->playerArray[playerID]->client_fd, sharedData->playerArray[playerID]->clientData, sizeof(socketCommunication_t), 0);
    sharedData->color = sharedData->playerArray[playerID]->clientData->color;

    //If the return value of checkColor is 0, the player had remembered the wrong color
    if (checkColor(sharedData, index) == 0)
    {
        //Calculate whose' turn is it next
        whoseTurn(sharedData, playerID);
        sharedData->playerArray[playerID]->clientData->playerState = LOSER;
        sharedData->playerArray[playerID]->isOut = 0;
        sharedData->losers++;
        sharedData->newColor = 0;
        sharedData->newRound = 1;
    }
}

/*
    Free structs
*/
void freeAll(thread_data_t *sharedData)
{
    for(int i = 0; i < sharedData->playersExpected; i++)
    {
        free(sharedData->playerArray[i]->clientData);
    }
    
    free(sharedData->colorSequence);

    free(sharedData);
}