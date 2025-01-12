/*
 * Auctioneer
 * Tests for Word Ladder
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <csse2310a3.h>
#include <csse2310a4.h>

// constants
#define USAGE_ERR "Usage: auctioneer [--max connections] [--listenon portno]\n"
#define USAGE_ERR_CODE 8
#define INVALID_PORT "auctioneer: socket can't be listened on\n"
#define INVALID_PORT_CODE 11
#define MIN_ARGS 2
#define MAX_ARGS 5
#define MAX_PORT 65535
#define MIN_PORT 1024
#define LISTEN_ON "--listenon"
#define MAX "--max"
#define DEFAULT_PORT "0"
#define SELL_ARGS_NO 4
#define RESERVE 2
#define DURATION 3
#define SELL_NAME 1
#define REJECTED ":rejected"
#define BUFFER_LISTED 9
#define LISTED ":listed %s"
#define INVALID ":invalid"
#define SPACE " "
#define BID_ARGS 3
#define BID_ARGS_NO 2
#define BID_NAME_ARGS_NO 1
#define NAME_BUFFER 6
#define BREAKER "|"
#define MAX_INPUT 4
#define BUFFER_LEN 5
#define BUFFER_LARGE 8
#define BLANK ' '

// Structure that holds all the data for each item
typedef struct {
    int highestBid;
    int highestBidder;
    int owner;
    int reserve;
    double duration;
    char* itemName;
    bool removed;
    int charLen;
    int reserveLen;
} Item;

// Structure that holds items in auction
typedef struct {
    int numItems;
    Item* items;
    pthread_mutex_t lock;
} Auction;

// Structure that keeps track of stats
typedef struct {
    unsigned int sellRequest;
    unsigned int sellAccepted;
    unsigned int bidReceived;
    unsigned int bidAccepted;
} Stat;

// Structure that keeps track of client connected
typedef struct {
    int id;
    bool active;
} ActiveClient;

// Structure that holds all the data
typedef struct {
    int maxConnections;
    char* portNumber;
    int fdServer;
    int numCon;
    int fdptr;
    int totalCon;
    pthread_mutex_t lock;
    ActiveClient* clients;
    Auction* auction;
    Stat* stats;
} AuctionData;

// Structure that holds all the data for the client to connect 
typedef struct {
    int fdptr;
    int* curCon;
    int* totalCon;
    pthread_mutex_t* lock;
    Auction* auction;
    Stat* stats;
    ActiveClient* clients;
} ThreadArgs;

// functions

/* usage_err()
* −----------------
* Throws usage error
*/
void usage_err() {
    fprintf(stderr, USAGE_ERR);
    exit(USAGE_ERR_CODE);
}

/* check_digits()
* −−−−−−−−−−−−−−−
* Checks to see if all characters are digits 
*
* number: The given pointer to the string.
*
* Returns: boolean
*/
bool check_digits(const char* number) {
    while (*number) {
        if (*number < '0' || *number > '9') { 
            return false;
        }
        number++; //points to next char
    }
    return true;
}

/* check_command_line()
* −−−−−−−−−−−−−−−
* Checks the validity of command line arguments 
* 
* data: Pointer to the AuctionData struct that holds the server's data.
* argc: The number of arguments passed in the command line.
* argv: The array of strings containing the command line arguments.
* 
* Errors: if insufficient or too many arguments are given or not a valid port 
* number is given or port number is not a digit.
*/
void check_command_line(AuctionData* data, int argc, char* argv[]) {
    bool setMax = false;
    bool setPort = false;
    if (argc == MIN_ARGS || argc > MAX_ARGS) {
        usage_err();
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], LISTEN_ON) == 0 && i + 1 < argc && !setPort &&
                check_digits(argv[i + 1])) {
    
            int port = atoi(argv[i + 1]);
            if (port > MAX_PORT || port < MIN_PORT) {
                if (port != 0) {
                    usage_err();
                }
            }
            data->portNumber = argv[i + 1];
            setPort = true;
            i++; 
        } else if (strcmp(argv[i], MAX) == 0 && i + 1 < argc && !setMax &&
                check_digits(argv[i + 1])) {

            if (atoi(argv[i + 1]) < 0) {
                usage_err();
            }
            data->maxConnections = atoi(argv[i + 1]);
            setMax = true;
            i++;
        } else {
            usage_err();
        }
    }

    if (!setMax) {
        data->maxConnections = 0;
    }

    if (!setPort) {
        data->portNumber = DEFAULT_PORT;
    }
}

/* connect_port()
* −−−−−−−−−−−−−−−
* Creates a socket and binds it to a port number specified.
* Listens for incoming connections
* Prints port number to stderr
* 
* param: data A pointer to the AuctionData struct
* 
* Errors: if the socket cant be listened on
*/
void connect_port(AuctionData* data) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;   // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // listen on all IP addresses  

    int err;
    if ((err = getaddrinfo(NULL, data->portNumber, &hints, &ai))) {
        freeaddrinfo(ai);
        fprintf(stderr, INVALID_PORT);
        exit(INVALID_PORT_CODE);
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0); // 0=default protocol (TCP)
    data->fdServer = listenfd;

     // Allow address (port number) to be reused immediately
    int optVal = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int));
    if (bind(listenfd, ai->ai_addr, sizeof(struct sockaddr)) < 0) {
        fprintf(stderr, INVALID_PORT);
        exit(INVALID_PORT_CODE);
    }

    if (listen(listenfd, SOMAXCONN) < 0) { 
        fprintf(stderr, INVALID_PORT);
        exit(INVALID_PORT_CODE);
    }

    struct sockaddr_in sockin;
    socklen_t len = sizeof(sockin);
    if (getsockname(listenfd, (struct sockaddr *)&sockin, &len) == -1) {
        perror("getsockname");
    } else {
        // Getting the number of the port
        fprintf(stderr, "%d\n", ntohs(sockin.sin_port));
    }
    fflush(stderr);
}

/* check_active()
* −−−−−−−−−−−−−−−
* Checks if a client with the given file descriptor is active.
* 
* fd: The file descriptor of the client to check.
* clients: An array of ActiveClient structs representing all clients.
* totalCon: The total number of connected clients.
* 
* Return: true if the client is active, false otherwise.
*/
bool check_active(int fd, ActiveClient* clients, int totalCon) {
    for (int i = 0; i < totalCon; i++) {
        if (clients[i].id == fd) {
            return clients[i].active;
        }
    }
    return false;
}

/* process_sell()
* −−−−−−−−−−−−−−−
* Processes a sell request and adds item if it meets the requirements.
* 
* line: The client input line
* params: The ThreadArgs struct
* numArgs: The number of arguments in the sell request.
* fields: The array of fields in the sell request.
* curFd: The file descriptor of the client making the request.
* response: The response message to be sent back to the client.
* 
* Return: a response message whether the sell request was valid or not
*/
char* process_sell(char* line, ThreadArgs* params, int numArgs, char** fields, 
        int curFd, char* response) {
    params->stats->sellRequest++;
    if (numArgs == SELL_ARGS_NO) {
        Auction* auction = params->auction;
        if (check_digits(fields[RESERVE]) && check_digits(fields[DURATION])) {
            for (int i = 0; i < auction->numItems; i++) {
                if (!auction->items[i].removed && 
                        strcmp(auction->items[i].itemName, 
                        fields[SELL_NAME]) == 0) {
                    return REJECTED; 
                }
            }
            int reserve = atoi(fields[RESERVE]);
            double duration = atoi(fields[DURATION]) + get_time_ms();
            int charLen = strlen(line) - 1; //-4 sell +3 for space0|
            if (reserve > 0 && atoi(fields[DURATION]) >= 1) {
                params->stats->sellAccepted++;
                Item item = {.owner = curFd, .highestBidder = 0, 
                    .duration = duration, .removed = false, 
                    .itemName = strdup(fields[SELL_NAME]), .highestBid = 0,
                    .reserve = reserve, .charLen = charLen,
                    .reserveLen = strlen(fields[RESERVE])};
                response = malloc(strlen(item.itemName) + BUFFER_LISTED);
                sprintf(response, LISTED, item.itemName);
                auction->items = realloc(auction->
                        items, (auction->numItems + 1) *
                        sizeof(Item));
                auction->items[auction->numItems] = item;
                auction->numItems++;
                return response;
            } else {
                return INVALID;
            }
        } else {
            return INVALID;
        }
    } else {
        return INVALID;
    }
}

/* process_bid()
* −−−−−−−−−−−−−−−
* Processes a bid request
* 
* line: The client input line
* params: The ThreadArgs struct
* numArgs: The number of arguments in the sell request.
* fields: The array of fields in the sell request.
* curFd: The file descriptor of the client making the request.
* response: The response message to be sent back to the client.
* 
* Return: a response message whether the sell request was valid or not
*/
char* process_bid(char* line, ThreadArgs* params, int numArgs, char** fields, 
        int curFd, char* response) {
    params->stats->bidReceived++;
    if (numArgs == BID_ARGS) {
        if (!check_digits(fields[BID_ARGS_NO])) {
            return INVALID;
        }
        int bid = atoi(fields[BID_ARGS_NO]);
        if (bid < 1) {
            return INVALID;
        }
        for (int i = 0; i < params->auction->numItems; i++) {
            Item* item = &(params->auction->items[i]);
            if (strcmp(item->itemName, fields[BID_NAME_ARGS_NO]) == 0 &&
                    !item->removed) {

                if (bid >= item->reserve && item->owner != curFd && 
                        item->highestBidder != curFd && 
                        bid > item->highestBid) {
                    if (item->highestBidder != 0 && check_active(item->
                            highestBidder, params->clients, *params->
                            totalCon)) {
                        char* outBid = malloc(strlen(item->
                                itemName) + BUFFER_LISTED);
                        sprintf(outBid, ":outbid %s %d", item->
                                itemName, bid);
                        FILE* to = fdopen(item->highestBidder, "w");
                        fprintf(to, "%s\n", outBid);
                        fflush(to);
                    }
                    params->stats->bidAccepted++;
                    item->highestBid = bid;
                    item->highestBidder = curFd;
                    item->charLen = item->charLen + strlen(fields[2]) -
                            item->reserveLen;
                    response = malloc(strlen(fields[BID_NAME_ARGS_NO]) +
                            NAME_BUFFER);
                    sprintf(response, ":bid %s", fields[BID_NAME_ARGS_NO]);
                    return response;
                } else {
                    return REJECTED;
                }
            } 
        }
        return REJECTED;
    } else {
        return INVALID;
    }
}

/* make_list()
* −−−−−−−−−−−−−−−
* Iterates through the auction's items and creates a list of live items 
* 
* param: The ThreadArgs struct
* response: The string to store the list of items.
* responseLen: The maximum length of the response string.
*/
void make_list(ThreadArgs* params, char* response, int responseLen) {
    for (int i = 0; i < params->auction->numItems; i++) {
        Item* item = &(params->auction->items[i]);
        if (item->removed) {
            continue;
        } 
        strcat(response, item->itemName);
        strcat(response, SPACE);
        char reserveStr[responseLen]; //Max length
        snprintf(reserveStr, sizeof(reserveStr), "%d", item->reserve);
        strcat(response, reserveStr);
        strcat(response, SPACE);
        char maxBidStr[responseLen];  
        snprintf(maxBidStr, sizeof(maxBidStr), "%d", item->highestBid);
        strcat(response, maxBidStr);
        strcat(response, SPACE);
        char remainStr[responseLen];  
        double remainTime = (item->duration - get_time_ms());
        if (remainTime < 1) {
            remainTime = 0;
        }
        snprintf(remainStr, sizeof(remainStr), "%d", (int)remainTime);
        strcat(response, remainStr);
        strcat(response, BREAKER);
    }
}

/* process_line()
* −−−−−−−−−−−−−−−
* Processes a line of input from a client and returns a response.
* 
* line: The line of input to process.
* params: The ThreadArgs struct.
* curFd: The file descriptor of the current client.
*
* Return A response to the client's input.
*/
char* process_line(char* line, ThreadArgs* params, int curFd) {
    char** fields = split_by_char(line, BLANK, 0);
    int numArgs = 0;
    while (fields[numArgs] != NULL) {
            numArgs++;
        }
    char* command = fields[0];
    char* response = NULL;
    if (numArgs > MAX_INPUT) {
        return INVALID;
    } else {
        if (strcmp(command, "sell") == 0) {
            pthread_mutex_lock(&params->auction->lock);
            response = process_sell(line, params, numArgs, fields, curFd, 
                    response);
            pthread_mutex_unlock(&params->auction->lock);
        } else if (strcmp(command, "bid") == 0) {
            pthread_mutex_lock(&params->auction->lock);
            response = process_bid(line, params, numArgs, fields, curFd, 
                    response);
            pthread_mutex_unlock(&params->auction->lock);
        } else if (strcmp(command, "list") == 0 && numArgs == 1) {
            bool allRemoved = true;
            response = ":list";
            pthread_mutex_lock(&params->auction->lock);
            if (params->auction->numItems == 0) {
                pthread_mutex_unlock(&params->auction->lock);
                return response;
            }
            int responseLen = BUFFER_LEN;  // For ":list"
            for (int i = 0; i < params->auction->numItems; i++) {
                responseLen += params->auction->items[i].charLen;
                if (!params->auction->items[i].removed) {
                    allRemoved = false;
                }
            }
            pthread_mutex_unlock(&params->auction->lock);
            if (allRemoved) {
                return response;
            }
            char* response = (char*)malloc(responseLen * sizeof(char));
            strcpy(response, ":list ");
            make_list(params, response, responseLen);
            return response;
        } else {
            return INVALID;
        }
    }
    return response;
}

/* expiry_thread()
* −−−−−−−−−−−−−−−
* Thread that checks when auctoins expire
* If expired, removes item from auction and notifies highest bidder and owner 
* If no bids, it notifies only the owner.
* 
* arg: a pointer to the AuctionData struct 
* 
* Return NULL
*/
void* expiry_thread(void* arg) {
    AuctionData* data = (AuctionData*)arg;
    while (1) {
        pthread_mutex_lock(&data->auction->lock);
        double currentTime = get_time_ms();
        for (int i = 0; i < data->auction->numItems; i++) {
            Item* item = &data->auction->items[i];
            if (!item->removed && currentTime >= item->duration) {
                item->removed = true;
                if (item->highestBidder != 0) {
                    if (check_active(item->owner, data->clients,
                            data->totalCon)) {
                        char* sold = malloc(item->charLen + NAME_BUFFER);
                        sprintf(sold, ":sold %s %d", item->itemName, 
                                item->highestBid);
                        FILE* to = fdopen(item->owner, "w");
                        fprintf(to, "%s\n", sold);
                        fflush(to);
                    }
                    if (check_active(item->highestBidder, data->clients, 
                            data->totalCon)) {
                        char* won = malloc(item->charLen + BUFFER_LEN);
                        sprintf(won, ":won %s %d", item->itemName, 
                                item->highestBid);
                        FILE* to2 = fdopen(item->highestBidder, "w");
                        fprintf(to2, "%s\n", won);
                        fflush(to2);
                    }
                    
                } else {
                    if (check_active(item->owner, data->clients, 
                            data->totalCon)) {
                        char* unsold = malloc(strlen(item->itemName) + 
                                BUFFER_LARGE);
                        sprintf(unsold, ":unsold %s", item->itemName);
                        FILE* to = fdopen(item->owner, "w");
                        fprintf(to, "%s\n", unsold);
                        fflush(to);
                    }
                }
            }
        }
        pthread_mutex_unlock(&data->auction->lock);
        usleep(100000); //100ms
    }

    return NULL;
}

/* client_thread()
* −−−−−−−−−−−−−−−
* Thread that handles communication with a client.
* Reads input from client and processes it.
* 
* arg: A void pointer to a ThreadArgs struct 
* 
* Return NULL
*/
void* client_thread(void* arg) {
    ThreadArgs* params = (ThreadArgs*)arg;
    int fd = params->fdptr;
    FILE* to = fdopen(fd, "w");
    FILE* from = fdopen(dup(fd), "r");
    char* currentIn;
    while ((currentIn = read_line(from)) != NULL) {
        char* response;
        response = process_line(currentIn, params, fd);
        fprintf(to, "%s\n", response);
        fflush(to);
        free(currentIn);
    }
    pthread_mutex_lock(params->lock);
    for (int i = 0; i < *params->curCon; i++) {
        if (params->clients[i].id == fd) {
            params->clients[i].active = false;
        }
    }
    (*params->curCon)--;
    pthread_mutex_unlock(params->lock);

    close(fd);
    fclose(to);
    fclose(from);

    return NULL;
}

/* client_thread()
* −−−−−−−−−−−−−−−
* Processes incoming connections from clients and creates a new thread to
* handle each client.
* If max connections is set, it will wait in a while loop until a client 
* disconnects before accepting a new connection.
* 
* data: A pointer to the AuctionData struct
*
* Errors: if the socket cant be accepted
*/
void process_connections(AuctionData* data) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    int maxCon = data->maxConnections;

    while (1) {
        fromAddrSize = sizeof(struct sockaddr_in);
        fd = accept(data->fdServer, (struct sockaddr*)&fromAddr, 
                &fromAddrSize);
        data->fdptr = fd;
        if (fd < 0) {
            perror("Error accepting connection");
            exit(1);
        }
        pthread_mutex_lock(&data->lock);
        if (maxCon != 0) {
            while (data->numCon >= maxCon) {
                pthread_mutex_unlock(&data->lock);
                pthread_mutex_lock(&data->lock);
            }
        }
        data->clients = realloc(data->clients, (data->totalCon + 1) * 
                sizeof(ActiveClient));
        data->clients[data->totalCon].id = fd;
        data->clients[data->totalCon].active = true;
        (data->numCon)++;
        (data->totalCon)++;
        
        pthread_mutex_unlock(&data->lock);
        ThreadArgs threadArgs = {.fdptr = fd, .curCon = &data->numCon, 
                .lock = &data->lock, .auction = data->auction, 
                .stats = data->stats, .clients = data->clients,
                .totalCon = &data->totalCon};

        pthread_t threadId;
        pthread_create(&threadId, NULL, client_thread, &threadArgs);
        pthread_detach(threadId);

    }
}

/* init_stat()
* −----------------
* Initializes the given Stat struct with default values.
* 
* stats: The Stat struct to initialize.
*/
void init_stat(Stat* stats) {
    stats->sellRequest = 0;
    stats->sellAccepted = 0;
    stats->bidReceived = 0;
    stats->bidAccepted = 0;
}

/* signal_thread()
* −----------------
* Thread that handles SIGHUP signal
* Prints out the stats of the server
*
* arg: A void pointer to a AuctionData struct 
* 
* Return NULL
* REF: Following was constructed by ChatGPT, was modified to print nessesary 
* REF: message and removed unnesesary code.
* REF: added comments to show understanding
*/
void* signal_thread(void* arg) {
    AuctionData* data = (AuctionData*)arg;
    sigset_t set;
    int sig;

    // Block SIGHUP in this thread
    // So can only be handeled in this thread
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    while (1) {
        // Wait for SIGHUP to occur
        sigwait(&set, &sig); 
        fprintf(stderr, "Connected clients: %u\n", data->numCon);
        fprintf(stderr, "Completed clients: %u\n", data->totalCon - 
                data->numCon);
        int activeAuctions = 0;
        for (int i = 0; i < data->auction->numItems; i++) {
            if (!data->auction->items[i].removed) {
                activeAuctions++;
            }
        }
        fprintf(stderr, "Active auctions: %u\n", activeAuctions);
        fprintf(stderr, "Total sell requests: %u\n", data->stats->sellRequest);
        fprintf(stderr, "Successful sell requests: %u\n", data->stats->
                sellAccepted);
        fprintf(stderr, "Total bid requests: %u\n", data->stats->bidReceived);
        fprintf(stderr, "Successful bid requests: %u\n", data->stats->
                bidAccepted);
    }

    return NULL;
}

/* main()
* −----------------
* Main function of the program
* Calls all the functions to run the Auction server
*
* argc: Number of command line arguments
* argv: Array of command line arguments
*/
int main(int argc, char* argv[]) {
    AuctionData* data = malloc(sizeof(AuctionData));
    pthread_mutex_init(&data->lock, NULL);
    data->auction = malloc(sizeof(Auction));
    data->stats = malloc(sizeof(Stat));
    data->clients = malloc(sizeof(ActiveClient));
    init_stat(data->stats);
    pthread_mutex_init(&data->auction->lock, NULL);

    check_command_line(data, argc, argv);

    pthread_t tid;
    pthread_create(&tid, NULL, signal_thread, data);
    // GPT produced code
    // Block SIGHUP in this thread
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    // End GPT produced code
    pthread_t expiryThread;
    pthread_create(&expiryThread, NULL, expiry_thread, data);
    connect_port(data);
    process_connections(data);
    pthread_join(expiryThread, NULL);
}


