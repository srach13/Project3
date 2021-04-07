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

int main() {
    printf("Hello, World!\n");
    return 0;
}
