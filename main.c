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
int getListenFileDescriptor(char *); //listen fd
void *serviceClient(void *); //services the client with the thread
ssize_t readLine(int , void *, size_t); //read line
char **makeDictionary(char *); //makes the dictionary into pointer

int main(int argc, char **argv) {
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

    if(argc < 2) { //neither given
        port = DEFAULT_PORT_STR;
        dictionaryName = DICT;
    } else if(argc < 3) { //only dictionary name
        port = DEFAULT_PORT_STR;
        dictionaryName = argv[2];
    } else { //port and dictionary given
        port = argv[1];
        dictionaryName = argv[2];
    }

    newSocket = getListenFileDescriptor(port); //socket descriptor

    instantiateQueue(&q, 20); //queue made

    if((dictionaryWords = makeDictionary(dictionaryName)) == NULL) { //error opening dictionary
        fprintf(stderr, "Error: Couldn't open dictionary.\n"); //error message
        return EXIT_FAILURE; //exit failure notice
    }

    threadArgument.q = &q; //worker thread queue
    threadArgument.dictionary_words = dictionaryWords; //worker thread dictionary

    //create worker threads
    for(i = 0; i < NUM_WORKERS; i++) {
        if(pthread_create(&threads[i], NULL, serviceClient, &threadArgument) != 0) {
            fprintf(stderr, "error creating thread.\n");
            return EXIT_FAILURE;
        }
    }

    while (1) {
        client_address_size = sizeof(client_address);
        if((welcomeSocket = accept(newSocket, (sockaddr *) &client_address, &client_address_size)) == -1) { //accept connection
            fprintf(stderr, "accept error\n");
            continue;
        }

        if(getnameinfo((sockaddr *) & client_address, client_address_size, client_name, MAX_LINE, client_port, MAX_LINE, 0) != 0) { //name info of connection
            fprintf(stderr, "error getting name information about client\n");
        } else {
            printf("accepted connection from %s: %s\n", client_name, client_port);
        }

        addToSocket(&q, welcomeSocket); //add client to socket
    }

    // join threads
    for(i = 0; i < NUM_WORKERS; i++) {
        if(pthread_join(threads[i], &ret) != 0) {
            fprintf(stderr, "join error\n");
            return EXIT_FAILURE;
        }
    }

    free(dictionaryWords);

    return EXIT_SUCCESS;
}

/* given a port number or service as string,
returns a descriptor to pass on to accept() */
int getListenFileDescriptor(char *port) {
    int listenFileDescriptor, status;
    addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));
    // TCP
    hints.ai_socktype = SOCK_STREAM;
    // IPv4
    hints.ai_family = AF_INET;

    if((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "Get address info error %s\n", gai_strerror(status));
        exit(EXIT_GET_ADDRESS_INFO_ERROR);
    }

    /* try to bind to the first available address/port in the list
    if we fail, try the next one */
    for(p = res; p != NULL; p = p->ai_next) {
        if((listenFileDescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;

        if(bind(listenFileDescriptor, p->ai_addr, p->ai_addrlen) == 0)
            break;
    }

    // free up memory
    freeaddrinfo(res);

    if(p == NULL)
        exit(EXIT_BIND_FAILURE);

    if(listen(listenFileDescriptor, BACKLOG) < 0) {
        close(listenFileDescriptor);
        exit(EXIT_LISTEN_FAILURE);
    }

    return listenFileDescriptor;
}

/*worker thread function*/
void *serviceClient(void *arguments) {
    int welcomeSocket;
    ssize_t bytes_read;
    char line[MAX_LINE];
    char result[MAX_LINE];
    int ok = FALSE;

    threadArg *args = arguments;
    queue *qu = args->q;
    char **dictionaryWords = args->dictionary_words;

    while (1) {
        welcomeSocket = removeFromSocket(qu);
        while((bytes_read = readLine(welcomeSocket, line, MAX_LINE - 1)) > 0) {
            memset(result, 0, sizeof(result)); //empty the string
            int i;
            for(i = 0; dictionaryWords[i] != NULL; i++) {
                if(strncmp(dictionaryWords[i], line, strlen(line) - 2) == 0) { //checks to see if words match
                    strncpy(result, line, strlen(line) - 2); //copies result to line
                    ok = TRUE;
                    break;
                }
            }

            if(ok) {
                strcat(result, " OK\n"); //spelled correctly
            } else {
                strncpy(result, line, strlen(line) - 2);
                strcat(result, " MISSPELLED\n"); //misspelled
            }
            write(welcomeSocket, result, sizeof(result)); //sends result back to client
            ok = FALSE;
        }

        printf("connection closed\n");
        close(welcomeSocket);
    }
}

/* Read characters from 'fd' until a newline is encountered. If a newline
character is not encountered in the first (n - 1) bytes, then the excess
characters are discarded. The returned string placed in 'buf' is
null-terminated and includes the newline character if it was read in the
first (n - 1) bytes. The function return value is the number of bytes
placed in buffer (which includes the newline character if encountered,
but excludes the terminating null byte). */

ssize_t readLine(int fd, void *buffer, size_t n) {
    ssize_t numRead; //# of bytes fetched by last read()
    size_t totRead; //Total bytes read so far
    char *buf;
    char ch;

    if(n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = buffer; //No pointer arithmetic on "void *"

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if(numRead == -1) {
            if(errno == EINTR) //Interrupted --> restart read()
                continue;
            else
                return -1; //Some other error

        } else if(numRead == 0) { //EOF
            if(totRead == 0) //No bytes read; return 0
                return 0;
            else //Some bytes read; add '\0'
                break;

        } else { //numRead' must be 1 if we get here
            if(totRead < n - 1) { //discard > (n - 1) bytes */
                totRead++;
                *buf++ = ch;
            }

            if(ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
}

//makes the dictionary that'll be shared by all threads
char **makeDictionary(char *dictionaryName) {
    FILE *fp;
    char line[MAX_LINE];
    char **dict;
    int i = 0;

    if((dict = malloc(DICT_SIZE * sizeof(char *))) == NULL) { //allocating array
        fprintf(stderr, "error allocating dictionary array.\n");
        return NULL;
    }

    fp = fopen(dictionaryName, "r"); //opens file
    while(fgets(line, sizeof(line), fp)) {
        if((dict[i] = malloc(strlen(line) * sizeof(char *) + 1)) == NULL)//puts word into array
        {
            fprintf(stderr, "error loading word into dict array.\n");
            return NULL;
        }

        strncpy(dict[i++], line, strlen(line) - 1); //takes away new line character
    }
    fclose(fp);
    dict[i] = NULL;
    return dict;
}