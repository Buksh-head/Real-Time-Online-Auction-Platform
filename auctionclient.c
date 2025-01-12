/*
 * Auction Client
 * Client that connecst to auction server
 * Created by: Adnaan Buksh
 * Student number: 47435568
 */

// includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <csse2310a3.h>
#include <csse2310a4.h>
#include <signal.h>

// constants
#define USAGE_ERR "Usage: auctionclient portno\n"
#define USAGE_ERR_CODE 20
#define CONNECTION_ERR "auctionclient: cannot connect to port %s\n"
#define CONNECTION_ERR_CODE 13
#define CONNECTION_CLOSE "auctionclient: server connection closed\n"
#define CONNECTION_CLOSE_CODE 18
#define AUCTION_EXIT "Exiting with auction still in progress\n"
#define AUCTION_EXIT_CODE 9
#define AUCTION_PROG "Auction(s) in progress - can not exit yet\n"
#define LOCALHOST "localhost"
#define BID ":bid"
#define LISTED ":listed"
#define OUTBID ":outbid"
#define WON ":won"
#define UNSOLD ":unsold"
#define SOLD ":sold"
#define QUIT "quit"
#define HASH "#"
#define EMPTY "\0"
#define SMALL_BUFFER 4
#define BUFFER 5
#define LARGE_BUFFER 7

// Structure that holds all the data for the client to connect to the server
typedef struct {
    const char* portName;
    int socket;
    FILE* to;
    FILE* from;
    volatile int liveAuction;
} ClientData;

// functions

/* usage_err()
* −----------------
* Throws usage error
*/
void usage_err() {
    fprintf(stderr, USAGE_ERR);
    exit(USAGE_ERR_CODE);
}

/* command_line_check()
* −−−−−−−−−−−−−−−
* Checks the command line arguments, sets up the client data for the 
* auction client and connects to the port provided.
* 
* data: The client data struct to be set up.
* argc: The number of command line arguments.
* argv: The array of command line arguments.
* 
* Returns: The client data struct with the socket and file descriptors set up.
* Errors: if insufficient or too many arguments are given or not a valid port 
* number is given.
*/
ClientData command_line_check(ClientData data, int argc, char* argv[]) {
    if (argc != 2) {
        usage_err();
    }
    data.portName = argv[1];
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM; //TCP
    int err;
    if ((err = getaddrinfo(LOCALHOST, data.portName, &hints, &ai))) {
        fprintf(stderr, CONNECTION_ERR, data.portName);
        exit(CONNECTION_ERR_CODE);
    }

    // if valid address then connect
    data.socket = socket(AF_INET, SOCK_STREAM, 0); // 0 use default protocol 
    if (connect(data.socket, ai->ai_addr, sizeof(struct sockaddr))) { 
        fprintf(stderr, CONNECTION_ERR, data.portName);
        exit(CONNECTION_ERR_CODE);
    }
    data.to = fdopen(data.socket, "w");    
    data.from = fdopen(dup(data.socket), "r");

    freeaddrinfo(ai);
    return data;
}

/* to_server()
* −−−−−−−−−−−−−−−
* This function is responsible for reading the server response and counting 
* live auctions, protected by mutex.
* 
* arg: A void pointer to the client data.
* Return: void* Returns NULL.
* Errors: if server conection is disrupted
 */
void* to_server(void* arg) {
    ClientData* data = (ClientData*) arg;
    char* serverResponse;
    pthread_mutex_t liveAuctionMutex = PTHREAD_MUTEX_INITIALIZER;
    while ((serverResponse = read_line(data->from))) {
        pthread_mutex_lock(&liveAuctionMutex);
        if ((strncmp(serverResponse, BID, SMALL_BUFFER) == 0) ||
                (strncmp(serverResponse, LISTED, LARGE_BUFFER) == 0)) {
            data->liveAuction++;
        } else if ((strncmp(serverResponse, OUTBID, LARGE_BUFFER) == 0) ||
                (strncmp(serverResponse, WON, SMALL_BUFFER) == 0) ||
                (strncmp(serverResponse, UNSOLD, LARGE_BUFFER) == 0) ||
                (strncmp(serverResponse, SOLD, BUFFER) == 0)) {
            data->liveAuction--;
        }
        pthread_mutex_unlock(&liveAuctionMutex);
        fprintf(stdout, "%s\n", serverResponse);
        fflush(stdout);
        free(serverResponse);
    }
    if (serverResponse == NULL) {
        fprintf(stderr, CONNECTION_CLOSE);
        exit(CONNECTION_CLOSE_CODE);
    }
    return NULL;
}

/* sigpipe_handler()
* −----------------
* Handling SIGPIPE signal
* Prints error message and exits
* 
* sig : signal number 
*
*/
void sigpipe_handler(int signum) {
    fprintf(stderr, CONNECTION_CLOSE);
    exit(CONNECTION_CLOSE_CODE);
}

/* main()
* −----------------
* Main function of the program
* Sets up the signal handler for SIGPIPE
* init the ClientData struct,
* Creates a thread to read from the server, while main reads from stdin and 
* handles inputs.
*
* argc: The number of command line arguments.
* argv: The array of command line arguments.
*/
int main(int argc, char* argv[]) {
    signal(SIGPIPE, sigpipe_handler);
    ClientData data = {.portName = NULL, .liveAuction = 0};
    data = command_line_check(data, argc, argv);

    pthread_t toServer;
    pthread_create(&toServer, NULL, to_server, &data);

    char* line;
    while ((line = read_line(stdin))) {
        if (strcmp(line, QUIT) == 0) {
            if (data.liveAuction > 0) {
                fprintf(stdout, AUCTION_PROG);
                fflush(stdout);
            } else {
                exit(0);
            }
        } else if ((strncmp(line, EMPTY, 1) == 0) ||
                (strncmp(line, HASH, 1) == 0)) {
        } else {
            fprintf(data.to, "%s\n", line);
            fflush(data.to);
        }
        free(line);
    }
    
    if (feof(stdin)) {
        if (data.liveAuction > 0) {
            fprintf(stderr, AUCTION_EXIT);
            exit(AUCTION_EXIT_CODE);
        } else {
            exit(0);
        }
    }

    pthread_join(toServer, NULL);
    fclose(data.from);
    fclose(data.to);
}