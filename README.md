# Project3: Networked Spell Checker

This project is a multithreaded networked spell checker. 

DESIGN

Using this program, a client can connect to the server and send words. The server then responds to each word with either `OK` or `MISSPELLED`. 
The main thread handles the program setup. It parses the command line options and sets up the dictionary. 
The main thread also establishes `N` worker threads to service client requests and a single logger thread to handle logging activities. 
The main thread waits for incoming connections and places the respective sockets in a buffer of fixed size. 
If the buffer is full, the main thread waits until the buffer is no longer full to continue. 
When the buffer is no longer full, some worker thread signals this to the main thread. 
Additionally, the main thread will signal the workers to consume from the buffer if the buffer is not empty. 
Access to the socket buffer is protected by a mutex lock and condition variables, since N+1 threads are working on it.
Each worker thread continuously checks the socket buffer for new entries. 
If the socket buffer has sockets, a worker will remove the first socket and service that connection. 
After it removes a socket, the worker signals that the buffer has space to add another socket. 
Also, a mutex lock and condition variables are used here to ensure only a single worker can modify the socket buffer at once. 
This way, only one worker will ever service a given connection.
In servicing a connection, the worker waits for an incoming message, spell checks the words of the message, and sends the result back to the client. 
This continues until the client disconnects, at which point the worker goes to fetch another socket, or wait until there is a socket to fetch. 
Whenever a worker receives a message from a client, it sends the spell check results to a log queue, to be handled by the logger. 
The log queue is also protected by a mutex lock and condition variables, since N+1 threads are working on it.
The logger thread continuously checks the log queue for new entries, and writes them to a log file. 
As mentioned above, log queue is protected by a mutex lock and condition variables.

HOW TO USE

After building the program in a Linux environment, run the "spellchecker" executable file to begin the program. 
Once the program has executed successfully, the server will begin to run on the current machine and begin to accept any connection to the spell check server.
Once connected, the server will now begin to spell check any words you enter. NOTE: the server will only handle one word at a time.

TESTING

To test my spellchecker, I used the netcat program as a client. When testing, I used multiple instances of clients requesting spell check services at the same
time, i.e. multiple clients. These clients are running from multiple computer system simultaneously. I connected to the client using the command Nc 127.0.0.1 8888.
Then, I was able to run my program using ./spellchecker 8888.
