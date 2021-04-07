//HEADER FILES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>

//DECLARATIONS
#define BUFFER_SIZE 256
#define READ 0
#define WRITE 1
#define TRUE 1
#define FALSE 0
#define BACKLOG 20
#define DEFAULT_PORT_STR "9999"
#define DICT "dictionary.txt"
#define DICT_SIZE 99172
#define EXIT_USAGE_ERROR 1
#define EXIT_GET_ADDRESS_INFO_ERROR 2
#define EXIT_BIND_FAILURE 3
#define EXIT_LISTEN_FAILURE 4
#define NUM_WORKERS 5
#define MAX_LINE 64

//STRUCTS
typedef struct addrinfo addrinfo;
typedef struct sockaddr_storage sockaddr_storage;
typedef struct sockaddr sockaddr;

typedef struct {
    int *buffer_array; //buffer array
    int max_capacity; //max
    int first; //first
    int last; //last
    int size; //size of queue
    sem_t mutex; //semaphore binary
    sem_t slots; //semaphore
    sem_t items; //semaphore
} queue;

typedef struct threadArg {
    queue *q;
    char **dictionary_words;
} threadArg;

//PROTOTYPES
void instantiateQueue(queue *, int); //instantiate queue
void deInstantiate(queue *); //deInstantiate queue
void addToSocket(queue *, int); //add to socket
int removeFromSocket(queue *); //remove from socket
int getListenFD(char *); //listen fd
void *serviceClient(void *); //services the client with the thread
ssize_t readLine(int , void *, size_t); //read line
char **makeDictionary(char *); //makes the dictionary into pointer

int main (int argc, char **argv) {
    pthread_t threads[NUM_WORKERS];
    threadArg threadArgument; //thread
    int newSocket, welcomeSocket; //listening and connected socket descriptor
    int i;
    sockaddr client_address; //client address
    socklen_t client_address_size; //size of client address
    char client_name[MAX_LINE]; //client name
    char client_port[MAX_LINE]; //client port
    char *port; //port
    char *dictionaryName; //dictionary name
    char **dictionaryWords; //list of dictionary words
    void *ret;
    queue q; //queue of socket descriptors

    if (argc < 2) { //neither given
        port = DEFAULT_PORT_STR;
        dictionaryName = DICT;
    } else if (argc < 3) { //only dictionary name
        port = DEFAULT_PORT_STR;
        dictionaryName = argv[2];
    } else { //port and dictionary given
        port = argv[1];
        dictionaryName = argv[2];
    }

    newSocket = getListenFD(port); //socket descriptor

    instantiateQueue(&q, 20); //queue made

    if ((dictionaryWords = makeDictionary(dictionaryName)) == NULL) { //error opening dictionary
        fprintf(stderr, "Error: Couldn't open dictionary.\n"); //error message
        return EXIT_FAILURE; //exit failure notice
    }

    threadArgument.q = &q; //worker thread queue
    threadArgument.dictionary_words = dictionaryWords; //worker thread dictionary

    //create worker threads
    for (i = 0; i < NUM_WORKERS; i++) {
        if (pthread_create(&threads[i], NULL, serviceClient, &threadArgument) != 0) {
            fprintf(stderr, "error creating thread.\n");
            return EXIT_FAILURE;
        }
    }

    while (1) {
        client_address_size = sizeof(client_address);
        if ((welcomeSocket = accept(newSocket, (sockaddr *) &client_address, &client_address_size)) == -1) { //accept connection
            fprintf(stderr, "accept error\n");
            continue;
        }

        if (getnameinfo((sockaddr *) & client_address, client_address_size, client_name, MAX_LINE, client_port, MAX_LINE, 0) != 0) { //name info of connection
            fprintf(stderr, "error getting name information about client\n");
        } else {
            printf("accepted connection from %s: %s\n", client_name, client_port);
        }

        addToSocket(&q, welcomeSocket); //add client to socket
    }

    // join threads
    for (i = 0; i < NUM_WORKERS; i++) {
        if (pthread_join(threads[i], &ret) != 0) {
            fprintf(stderr, "join error\n");
            return EXIT_FAILURE;
        }
    }

    free(dictionaryWords);

    return EXIT_SUCCESS;
}

/* given a port number or service as string,
returns a descriptor to pass on to accept() */
int getListenFD(char *port) {
    int listenFD, status;
    addrinfo hints, *res, *p;

    memset (&hints, 0, sizeof(hints));
    // TCP
    hints.ai_socktype = SOCK_STREAM;
    // IPv4
    hints.ai_family = AF_INET;

    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error %s\n", gai_strerror(status));
        exit(EXIT_GET_ADDRESS_INFO_ERROR);
    }

    /* try to bind to the first available address/port in the list
    if we fail, try the next one */
    for (p = res; p != NULL; p = p->ai_next) {
        if ((listenFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;

        if (bind(listenFD, p->ai_addr, p->ai_addrlen) == 0)
            break;
    }

    // free up memory
    freeaddrinfo(res);

    if (p == NULL)
        exit(EXIT_BIND_FAILURE);

    if (listen(listenFD, BACKLOG) < 0) {
        close(listenFD);
        exit(EXIT_LISTEN_FAILURE);
    }

    return listenFD;
}


