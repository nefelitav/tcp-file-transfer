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
#include <dirent.h>
#include <errno.h>
#include "../include/utilities.h"
#include "../include/client.h"
#include "../include/server.h"
#define PERMS 0666

bool exitStatus = false;
pthread_t *worker_threads;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t socketLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t assignLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t printLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueFullCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueEmptyCond = PTHREAD_COND_INITIALIZER;
struct sockaddr_in server;
struct sockaddr *serverptr = (struct sockaddr *)&server;
socklen_t serverlen = 0;
int thread_num;

int main(int argc, char **argv)
{
    int port, sock, newsock, thread_pool_size, queue_size, block_size;
    struct sockaddr_in client;
    struct sockaddr *clientptr = (struct sockaddr *)&client;
    socklen_t clientlen = 0;

    // stop when i receive ctrl + c
    if (signal(SIGINT, sigint_handler) < 0)
    {
        perror("SIGINT");
    }

    // get arguments
    if (argc < 5)
    {
        printf("Please give port, thread pool size, queue size and block size\n");
        exit(1);
    }
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            port = atoi(argv[i + 1]);
        }

        if (strcmp(argv[i], "-s") == 0)
        {
            thread_pool_size = atoi(argv[i + 1]);
        }

        if (strcmp(argv[i], "-q") == 0)
        {
            queue_size = atoi(argv[i + 1]);
        }

        if (strcmp(argv[i], "-b") == 0)
        {
            block_size = atoi(argv[i + 1]);
        }
    }
    printf("Server's parameters are:\n");
    printf("port: %d\n", port);
    printf("thread_pool_size: %d\n", thread_pool_size);
    printf("queue_size: %d\n", queue_size);
    printf("block_size: %d\n", block_size);
    printf("Server was successfully initialized...\n");

    // initiliaze some structs
    createFileQueue(queue_size);
    createAssignmentQueue();
    createMutexQueue();
    worker_threads = malloc(sizeof(pthread_t) * thread_pool_size);

    for (int i = 0; i < thread_pool_size; i++)
    {
        pthread_create(&(worker_threads[i]), NULL, &worker_job, (void *)&block_size);
    }
    thread_num = thread_pool_size;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    serverlen = sizeof(server);

    if (bind(sock, serverptr, sizeof(server)) < 0)
    {
        perror_exit("bind");
    }

    if (listen(sock, 10) < 0)
    {
        perror_exit("listen");
    }

    // accepting connections
    printf("Listening for connections on port %d\n", port);
    while (1)
    {
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0)
        {
            perror_exit("accept");
        }
        pthread_mutex_lock(&socketLock);
        pushMutex(newsock);
        pthread_mutex_unlock(&socketLock);
        printf("Accepted connection\n\n");
        pthread_t communication_thread;
        // store args in a struct to send them to the thread function
        communication_thread_args *args = malloc(sizeof(communication_thread_args));
        args->socket = newsock;
        args->address = clientptr;
        args->address_len = clientlen;
        pthread_create(&communication_thread, NULL, &read_directory, (void *)args);
        pthread_join(communication_thread, NULL);
        free(args);
    }
    for (int i = 0; i < thread_pool_size; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }
    deleteAssignmentQueue();
    deleteFileQueue();
    deleteMutexQueue();
    free(worker_threads);
    close(newsock);
}

// communication thread function
void *read_directory(void *arg)
{
    char buff[4096];
    memset(buff, 0, 4096);
    communication_thread_args *args = (communication_thread_args *)arg;
    int n;
    unsigned int addr_len = args->address_len;

    // receive requested directory from client
    if ((n = recvfrom(args->socket, buff, sizeof(buff), 0, args->address, &addr_len) < 0))
    {
        perror_exit("recvfrom");
    }
    printf("[Thread: %ld] : About to scan directory %s\n", pthread_self(), buff);

    // check if it exists
    DIR *dir = opendir(buff);
    if (dir)
    {
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        printf("Directory does not exist\n");
        exit(1);
    }
    else
    {
        perror("opendir");
    }

    // create pipe
    int p[2];
    if (pipe(p) < 0)
    {
        perror("Failed to open pipe\n");
        exit(1);
    }
    pid_t pid;
    if ((pid = fork()) == -1)
    {
        perror("Failed to fork\n");
        exit(1);
    }
    if (pid == 0)
    {
        close(p[0]);
        // set pipe to stdout
        dup2(p[1], 1);
        close(p[1]);
        // -R -> all files recursively, -a -> hidden files also
        if (execl("/bin/ls", "ls", "-Ra", buff, (char *)0) == -1)
        {
            perror("Execl Failed\n");
            exit(1);
        }
    }
    else
    {
        // get output of ls
        close(p[1]);
        char inbuf[4096];
        memset(inbuf, 0, 4096);
        char *files = malloc(sizeof(char) * 1000 + 1);
        memset(files, 0, 1000 + 1);
        int nbytes;

        while (1)
        {
            nbytes = read(p[0], inbuf, 4096);
            if (nbytes == 0)
            {
                break;
            }
            files = (char *)realloc(files, strlen(files) + 4096 + 1);
            strncat(files, inbuf, 4096);
            memset(inbuf, 0, 4096);
        }

        // clean output of ls
        char *line, *temp;
        char *dir = malloc(sizeof(char) * strlen(files) + 1);
        line = strtok_r(files, "\n", &temp);
        do
        {
            if (strchr(line, ':') != NULL)
            {
                line[strlen(line) - 1] = '\0';
                strcpy(dir, line);
            }
            else
            {
                if (isDirectory(dir, line))
                {
                    continue;
                }
                pthread_mutex_lock(&queueLock);
                // push file to queue
                if (push(line, dir, args->socket, args->address, args->address_len) == false)
                {
                    // wait if queue is full
                    pthread_cond_wait(&queueFullCond, &queueLock);
                }
                else
                {
                    pthread_mutex_lock(&printLock);
                    printf("[Thread: %ld] : Adding file %s to the queue...\n", pthread_self(), line);
                    pthread_mutex_unlock(&printLock);
                    pthread_cond_signal(&queueEmptyCond);
                }
                pthread_mutex_unlock(&queueLock);
            }
        } while ((line = strtok_r(NULL, "\n", &temp)) != NULL);
        free(files);
        free(dir);
    }
    return NULL;
}

void *worker_job(void *arg)
{
    fileNode *fn = NULL;
    while (1)
    {
        pthread_mutex_lock(&queueLock);
        while (isEmpty())
        {
            if (exitStatus)
            {
                printf("Dead\n");
                pthread_mutex_unlock(&queueLock);
                return NULL;
            }
            // printf("Waiting\n");
            pthread_cond_wait(&queueEmptyCond, &queueLock);
        }
        // printf("Done Waiting\n");
        fn = pop();
        pthread_cond_signal(&queueFullCond);
        pthread_mutex_unlock(&queueLock);

        // start working on it
        pthread_mutex_lock(&assignLock);
        pushAssignment(fn->socket, pthread_self());
        pthread_mutex_unlock(&assignLock);
        pthread_mutex_lock(&printLock);
        printf("[Thread: %ld] : Received task: <%s/%s, %d>\n", pthread_self(), fn->directory, fn->file, fn->socket);
        pthread_mutex_unlock(&printLock);
        // file content
        char *filepath = malloc(4096);
        memset(filepath, 0, 4096);
        strcat(filepath, fn->directory);
        strcat(filepath, "/");
        strcat(filepath, fn->file);
        pthread_mutex_lock(&printLock);
        printf("[Thread: %ld] : About to read file %s\n", pthread_self(), filepath);
        pthread_mutex_unlock(&printLock);
        int readFile;
        if ((readFile = open(filepath, O_RDONLY, PERMS)) == -1)
        {
            perror("Failed to open file\n");
            exit(1);
        }
        // printf("%s\n", filepath);
        int block_size = *(int *)arg;
        char block[block_size + 1];
        memset(block, 0, block_size + 1);
        int nbytes;
        lock(fn->socket);

        // send block size
        char blockLength[10];
        memset(blockLength, 0, 10);
        sprintf(blockLength, "%d", block_size);
        if ((sendto(fn->socket, blockLength, 10, 0, fn->address, fn->address_len)) < 0)
        {
            perror_exit("sendto");
        }
        memset(blockLength, 0, block_size);

        // send filename
        if ((sendto(fn->socket, filepath, 4096, 0, fn->address, fn->address_len)) < 0)
        {
            perror_exit("sendto");
        }

        while (1)
        {
            // read from file
            if (((nbytes = read(readFile, block, block_size)) < 0))
            {
                perror_exit("read");
            }

            // have reached end of file
            if (nbytes == 0)
            {
                // inform client that we have reached end of file
                if ((sendto(fn->socket, "EOF", 4, 0, fn->address, fn->address_len)) < 0)
                {
                    perror_exit("sendto");
                }

                // wait for a little
                struct timeval read_timeout;
                read_timeout.tv_sec = 0;
                read_timeout.tv_usec = 10;
                if (setsockopt(fn->socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
                {
                    perror_exit("setsockopt");
                }

                // receive my own message so that ensure that the client doesnt receive EOF and metadata all at once
                char sentMessage[1000];
                int n;
                if (((n = recvfrom(fn->socket, sentMessage, 4, MSG_PEEK, serverptr, &serverlen)) < 0))
                {
                    if ((EAGAIN != errno) && (EWOULDBLOCK != errno))
                    {
                        perror_exit("recvfrom");
                    }
                }
                // wait until it's received
                while (strlen(sentMessage) > 0)
                    ;

                // send metadata
                char *metadata = getMetadata(filepath);
                if ((sendto(fn->socket, metadata, 500, 0, fn->address, fn->address_len)) < 0)
                {
                    perror_exit("sendto");
                }
                free(metadata);

                // finish working on it
                pthread_mutex_lock(&assignLock);
                popAssignment(fn->socket, pthread_self());

                // the client has been fully served
                if (isLast(fn->socket) && !stillServingClient(fn->socket))
                {
                    if ((sendto(fn->socket, "END", 5, 0, fn->address, fn->address_len)) < 0)
                    {
                        perror_exit("sendto");
                    }
                }
                else
                {
                    if ((sendto(fn->socket, "CONT", 5, 0, fn->address, fn->address_len)) < 0)
                    {
                        perror_exit("sendto");
                    }
                }
                pthread_mutex_unlock(&assignLock);
                break;
            }
            // send block to client
            if ((sendto(fn->socket, block, block_size, 0, fn->address, fn->address_len)) < 0)
            {
                perror_exit("sendto");
            }
            memset(block, 0, block_size + 1);
        }
        unlock(fn->socket);

        if (close(readFile) == -1)
        {
            perror("Failed to close file\n");
            exit(1);
        }

        free(filepath);
        free(fn->directory);
        free(fn->file);
        free(fn);
    }
    // return NULL;
}

int isDirectory(const char *dir, const char *path)
{
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0)
    {
        return 1;
    }
    char *filepath = malloc(strlen(dir) + strlen(path) + 2);
    memset(filepath, 0, strlen(dir) + strlen(path) + 2);
    strcat(filepath, dir);
    strcat(filepath, "/");
    strcat(filepath, path);
    struct stat statbuf;
    if (stat(filepath, &statbuf) != 0)
        return 0;
    free(filepath);
    return S_ISDIR(statbuf.st_mode);
}

char *getMetadata(char *filepath)
{
    struct stat statbuf;
    if (stat(filepath, &statbuf) < 0)
    {
        perror("stat\n");
    }

    char *metadata = malloc(sizeof(char) * 500);
    memset(metadata, 0, 500);

    strcat(metadata, "access: ");
    if (statbuf.st_mode & R_OK)
    {
        strcat(metadata, "read");
    }
    if (statbuf.st_mode & W_OK)
    {
        strcat(metadata, " write");
    }
    if (statbuf.st_mode & X_OK)
    {
        strcat(metadata, " execute");
    }
    strcat(metadata, "\n");

    strcat(metadata, "size: ");
    char st_size_str[20];
    sprintf(st_size_str, "%ld", statbuf.st_size);
    strcat(metadata, st_size_str);
    strcat(metadata, "\n");

    struct tm dt;
    dt = *(gmtime(&statbuf.st_ctime));
    strcat(metadata, "created on: ");
    char tm_mday_str[20];
    char tm_mon_str[20];
    char tm_year_str[20];
    char tm_hour_str[20];
    char tm_min_str[20];
    char tm_sec_str[20];
    sprintf(tm_mday_str, "%d", dt.tm_mday);
    sprintf(tm_mon_str, "%d", dt.tm_mon + 1);
    sprintf(tm_year_str, "%d", dt.tm_year + 1900);
    sprintf(tm_hour_str, "%d", dt.tm_hour);
    sprintf(tm_min_str, "%d", dt.tm_min);
    sprintf(tm_sec_str, "%d", dt.tm_sec);
    strcat(metadata, tm_mday_str);
    strcat(metadata, "-");
    strcat(metadata, tm_mon_str);
    strcat(metadata, "-");
    strcat(metadata, tm_year_str);
    strcat(metadata, " ");
    strcat(metadata, tm_hour_str);
    strcat(metadata, ":");
    strcat(metadata, tm_min_str);
    strcat(metadata, ":");
    strcat(metadata, tm_sec_str);
    strcat(metadata, "\n");

    dt = *(gmtime(&statbuf.st_mtime));
    strcat(metadata, "modified on: ");
    sprintf(tm_mday_str, "%d", dt.tm_mday);
    sprintf(tm_mon_str, "%d", dt.tm_mon + 1);
    sprintf(tm_year_str, "%d", dt.tm_year + 1900);
    sprintf(tm_hour_str, "%d", dt.tm_hour);
    sprintf(tm_min_str, "%d", dt.tm_min);
    sprintf(tm_sec_str, "%d", dt.tm_sec);
    strcat(metadata, tm_mday_str);
    strcat(metadata, "-");
    strcat(metadata, tm_mon_str);
    strcat(metadata, "-");
    strcat(metadata, tm_year_str);
    strcat(metadata, " ");
    strcat(metadata, tm_hour_str);
    strcat(metadata, ":");
    strcat(metadata, tm_min_str);
    strcat(metadata, ":");
    strcat(metadata, tm_sec_str);
    strcat(metadata, "\n");

    dt = *(gmtime(&statbuf.st_atime));
    strcat(metadata, "last accessed on: ");
    sprintf(tm_mday_str, "%d", dt.tm_mday);
    sprintf(tm_mon_str, "%d", dt.tm_mon + 1);
    sprintf(tm_year_str, "%d", dt.tm_year + 1900);
    sprintf(tm_hour_str, "%d", dt.tm_hour);
    sprintf(tm_min_str, "%d", dt.tm_min);
    sprintf(tm_sec_str, "%d", dt.tm_sec);
    strcat(metadata, tm_mday_str);
    strcat(metadata, "-");
    strcat(metadata, tm_mon_str);
    strcat(metadata, "-");
    strcat(metadata, tm_year_str);
    strcat(metadata, " ");
    strcat(metadata, tm_hour_str);
    strcat(metadata, ":");
    strcat(metadata, tm_min_str);
    strcat(metadata, ":");
    strcat(metadata, tm_sec_str);
    strcat(metadata, "\n");
    return metadata;
}

void sigint_handler(int signum)
{
    pthread_mutex_lock(&queueLock);
    printf("Dying\n");
    exitStatus = true;
    pthread_cond_broadcast(&queueEmptyCond);
    pthread_mutex_unlock(&queueLock);
    for (int i = 0; i < thread_num; i++)
    {
        if (pthread_join(worker_threads[i], NULL) != 0)
        {
            perror("pthread_join");
        }
    }
    deleteAssignmentQueue();
    deleteFileQueue();
    deleteMutexQueue();
    free(worker_threads);
    exit(0);
}