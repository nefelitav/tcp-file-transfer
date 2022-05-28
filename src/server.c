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
#include "../include/server.h"

pthread_t* worker_threads;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueFullCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueEmptyCond = PTHREAD_COND_INITIALIZER;

int main(int argc, char **argv) {
    int port, sock, newsock, thread_pool_size, queue_size, block_size;
    struct sockaddr_in server, client;
    struct sockaddr * serverptr = (struct sockaddr *)&server;
    struct sockaddr *clientptr = (struct sockaddr *)&client;
    clientptr = NULL;
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

    createFileQueue(queue_size);
    worker_threads = malloc(sizeof(pthread_t) * thread_pool_size);

    for (int i = 0; i < thread_pool_size; i++) {
        pthread_create(&worker_threads[i], NULL, &worker_job, (void*)&block_size);
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror_exit("socket");
    }

    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    if (bind(sock, serverptr, sizeof(server)) < 0) {
        perror_exit("bind") ;
    }

    if (listen(sock, 10) < 0) {
        perror_exit("listen");
    }

    printf("Listening for connections on port %d\n" , port);
    while (1) {

        if ((newsock = accept(sock, clientptr, &clientlen)) < 0) {
            perror_exit("accept");
        }

    	printf("Accepted connection\n\n");

        pthread_t communication_thread;
        read_dir_args_struct *args = malloc(sizeof(read_dir_args_struct));
        args->socket = newsock;
        args->address = clientptr;
        args->address_len = &clientlen;
        pthread_create(&communication_thread, NULL, &read_directory, (void*)args);
        pthread_join(communication_thread, NULL);
        free(args);
        close(newsock); 
    }
    deleteFileQueue();
    for (int i = 0; i < thread_pool_size; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    free(worker_threads);
}


void* read_directory(void *arg) {
    char buff[256];
    memset(buff, 0, 256);
    read_dir_args_struct* args = (read_dir_args_struct*)arg;
    int n;
    if ((n = recvfrom(args->socket, buff, sizeof(buff), 0, args->address, args->address_len) < 0)) {
        perror_exit("recvfrom");
    }
    printf("[Thread: %ld] : About to scan directory %s\n", pthread_self(), buff);

    // create pipe
    int p[2];
    if (pipe(p) < 0) {
        perror("Failed to open pipe\n");
        exit(1);
    }
    pid_t pid;
    if ((pid = fork()) == -1) {
        perror("Failed to fork\n");
        exit(1);
    }
    if (pid == 0) {
        close(p[0]);
        // set pipe to stdout
        dup2(p[1], 1); 
        close(p[1]);
        if (execl("/bin/ls", "ls", "-Ra", buff, (char *)0) == -1) { 
            perror("Execl Failed\n");
            exit(1);
        }
    } else {
        close(p[1]);
        char inbuf[256];
        memset(inbuf, 0, 256);
        char* files = malloc(sizeof(char) * 1000 + 1);
        memset(files, 0, 1000 + 1);
        int nbytes;
        while (1) {
            nbytes = read(p[0], inbuf, 256);
            if (nbytes == 0) {
                break;
            }
            files = (char *)realloc(files, strlen(files) + 256 + 1);
            strncat(files, inbuf, 256);
            memset(inbuf, 0, 256);
        }
        // printf("%s\n", files);
        char *line, *temp;
        char* dir;
        dir = malloc(sizeof(char) * strlen(files) + 1);
        line = strtok_r(files, "\n", &temp);
        do {
            // printf("current line = %s\n", line);
            if (strchr(line, ':') != NULL) {
                line[strlen(line)-1] = '\0';
                strcpy(dir, line);
                // printf("Directory = %s\n", dir);
            } else {
                if (isDirectory(line)) {
                    continue;
                }
                pthread_mutex_lock(&queueLock);
                if (push(line, dir, args->socket) == false) {
                    pthread_cond_wait(&queueFullCond, &queueLock);
                } else {
                    printf("[Thread: %ld] : Adding file %s to the queue...\n",  pthread_self(),  line);
                    pthread_cond_signal(&queueEmptyCond);
                }
                pthread_mutex_unlock(&queueLock);
            }
        } while ((line = strtok_r(NULL, "\n", &temp)) != NULL);
        free(files);
    }
    return NULL;
}

void *worker_job(void *arg) {
    fileNode *fn = NULL;
    while (1) {
        pthread_mutex_lock(&queueLock);
        while (isEmpty()) {
            // if (globalExit) {
            //     pthread_mutex_unlock(&queueLock);
            //     return NULL;
            // }
            printf("Waiting\n");
            pthread_cond_wait(&queueEmptyCond, &queueLock); 
        }
        printf("Done Waiting\n");

        fileNode* fn = pop(); 
        printf("[Thread: %ld]: Received task: <%s/%s, %d>\n", pthread_self(), fn->directory, fn->file, fn->socket);
        printf("[Thread: %ld]: About to read file %s/%s\n", pthread_self(), fn->directory, fn->file);
        
        // file content
        char* filepath = malloc(strlen(fn->directory) + strlen(fn->file) + 2);
        memset(filepath, 0, strlen(fn->directory) + strlen(fn->file) + 3);
        strcat(filepath, fn->directory);
        strcat(filepath, "/");
        strcat(filepath, fn->file);
        int readFile;
        if ((readFile = open(filepath, O_RDONLY)) == -1) {
            perror("Failed to open file\n");
            exit(1);
        }

        int block_size = *(int*)arg;

        char block[block_size];
        memset(block, 0, block_size);
        // int nbytes;
        // while (1) {
        //     nbytes = read(readFile, block, block_size);
        //     if (nbytes == 0) {
        //         break;
        //     }
        //     memset(block, 0, 256);
        // }

        // int stat(char *path, struct stat *buf);

        if (close(readFile) == -1) {  
            perror("Failed to close file\n");
            exit(1);
        } 
        free(filepath);
        pthread_cond_signal(&queueFullCond);
        pthread_mutex_unlock(&queueLock);
        free(fn);
    }
    // return NULL;
}

int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}