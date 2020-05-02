/*
    Helper methods to use with sockets
    The basic functions that should be done on any client / server program
    - Creation of the socket on the server and binding
    - Printing the local addresses
    - Creation of a socket on a client
    - Error validation when sending or receiving messages

    Gilberto Echeverria
    gilecheverria@yahoo.com
    31/03/2018
*/

#ifndef SOCKETS_H
#define SOCKETS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Socket libraries
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "fatal_error.h"

/*
	Show the local IP addresses, to allow testing
	Based on code from:
		https://stackoverflow.com/questions/20800319/how-do-i-get-my-ip-address-in-c-on-linux
*/
void printLocalIPs();

/*
    Prepare and open the listening socket
    Returns the file descriptor for the socket
    Remember to close the socket when finished
*/
int initServer(char * port, int max_queue);

/*
    Open and connect the socket to the server
    Returns the file descriptor for the socket
    Remember to close the socket when finished
*/
int connectSocket(char * address, char * port);

/*
    Send a string with error validation
    Receive the file descriptor, a string to store the message and the max string size
    Returns 1 on successful receipt, or 0 if the connection has finished
*/
int recvString(int connection_fd, char * buffer, int size);

/*
    Send a message with error validation
    Receive the file descriptor and the string pointer
*/
void sendString(int connection_fd, char * buffer);




#endif
