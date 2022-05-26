#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include "../include/utilities.h"
#include "../include/client.h"

fileQueue* queue;
pthread_t* worker_threads;

int main(int argc, char **argv) {
    int port, sock, newsock, thread_pool_size, queue_size, block_size;
    struct sockaddr_in server, client;
    struct sockaddr * serverptr = (struct sockaddr *)&server;
    struct sockaddr *clientptr= (struct sockaddr *)&client;
    socklen_t clientlen;

    if (argc < 5) {
    	printf("Please give port, thread pool size, queue size and block size\n");
       	exit(1);
    }

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[i+1]);
        }

        if (strcmp(argv[i], "-s") == 0) {
            thread_pool_size = atoi(argv[i+1]);
            // worker_threads = new pthread_t[thread_pool_size];
        }

        if (strcmp(argv[i], "-q") == 0) {
            queue_size = atoi(argv[i+1]);
        }

        if (strcmp(argv[i], "-b") == 0) {
            block_size = atoi(argv[i+1]);
        }
    
    }
    printf("Server's parameters are:\n");
    printf("port: %d\n", port);
    printf("thread_pool_size: %d\n", thread_pool_size);
    printf("queue_size: %d\n", queue_size);
    printf("block_size: %d\n", block_size);
    printf("Server was successfully initialized...\n");

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror_exit("socket");
    }
    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if (bind(sock, serverptr, sizeof(server)) < 0) {
        perror_exit("bind") ;
    }
    if (listen(sock, 5) < 0) {
        perror_exit ("listen");
    }
    printf("Listening for connections on port %d\n" , port);
    int i = 0;
    while (1) {
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0) {
            perror_exit("accept");
        }
    	printf("Accepted connection\n");

        pthread_t* communication_thread;
        pthread_create(&communication_thread, NULL, &read_directory, NULL);

        // pthread_create(&worker_threads[i], NULL, &execute_all_jobs, NULL);

        close(newsock); 
        i++;
    // }

}


void* read_directory(void *arg) {
    // recvfrom(int socket, void * buff, size_t length, int flags, struct sockaddr * address, socklen_t * address_len);
    // printf("About to scan directory %s\n", )
    // int status;
    // char *args[2];

    // args[0] = "/bin/ls";        // first arg is the full path to the executable
    // args[1] = NULL;             // list of args must be NULL terminated

    // if ( fork() == 0 )
    //     execv( args[0], args ); // child: call execv with the path and the args
    // else
    //     wait( &status );        // parent: wait for the child (not really necessary)


    // queue.push();

}

// void *execute_all_jobs(void *arg)
// {
//     jobNode *job = NULL;
//     while (1)
//     {
//         pthread_mutex_lock(&queueLock);
//         while (scheduler->getQueue()->isEmpty()) // Wait while JobQueue is empty
//         {
//             if (globalExit) // If program exited while this thread was waiting, exit
//             {
//                 pthread_mutex_unlock(&queueLock);
//                 return NULL;
//             }
//             pthread_cond_wait(&queueEmptyCond, &queueLock); // wait until queue is no more empty to pop
//         }
//         job = scheduler->getQueue()->pop(); // Execute Job
//         pthread_mutex_unlock(&queueLock);
//         job->getJob()->getFunc()(job->getJob()->getArgs());

//         delete job;
//     }
//     return NULL;
// }