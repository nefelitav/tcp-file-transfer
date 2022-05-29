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

pthread_t *worker_threads;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueFullCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueEmptyCond = PTHREAD_COND_INITIALIZER;

int main(int argc, char **argv)
{
    int port, sock, newsock, thread_pool_size, queue_size, block_size;
    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct sockaddr *clientptr = (struct sockaddr *)&client;
    socklen_t clientlen = 0;

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

    createFileQueue(queue_size);
    worker_threads = malloc(sizeof(pthread_t) * thread_pool_size);

    for (int i = 0; i < thread_pool_size; i++)
    {
        pthread_create(&worker_threads[i], NULL, &worker_job, (void *)&block_size);
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    if (bind(sock, serverptr, sizeof(server)) < 0)
    {
        perror_exit("bind");
    }

    if (listen(sock, 10) < 0)
    {
        perror_exit("listen");
    }

    printf("Listening for connections on port %d\n", port);
    while (1)
    {
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0)
        {
            perror_exit("accept");
        }
        printf("Accepted connection\n\n");
        pthread_t communication_thread;
        communication_thread_args *args = malloc(sizeof(communication_thread_args));
        args->socket = newsock;
        args->address = clientptr;
        args->address_len = clientlen;
        pthread_create(&communication_thread, NULL, &read_directory, (void *)args);
        pthread_join(communication_thread, NULL);
        free(args);
    }
    deleteFileQueue();
    for (int i = 0; i < thread_pool_size; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }
    free(worker_threads);
    close(newsock);
}

void *read_directory(void *arg)
{
    char buff[256];
    memset(buff, 0, 256);
    communication_thread_args *args = (communication_thread_args *)arg;
    int n;
    unsigned int addr_len = args->address_len;
    if ((n = recvfrom(args->socket, buff, sizeof(buff), 0, args->address, &addr_len) < 0))
    {
        perror_exit("recvfrom");
    }
    printf("[Thread: %ld] : About to scan directory %s\n", pthread_self(), buff);

    DIR *dir = opendir(buff);
    if (dir)
    {
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        printf("Directory does not exist\n");
        // TODO free resources
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
        if (execl("/bin/ls", "ls", "-Ra", buff, (char *)0) == -1)
        {
            perror("Execl Failed\n");
            exit(1);
        }
    }
    else
    {
        close(p[1]);
        char inbuf[256];
        memset(inbuf, 0, 256);
        char *files = malloc(sizeof(char) * 1000 + 1);
        memset(files, 0, 1000 + 1);
        int nbytes;
        while (1)
        {
            nbytes = read(p[0], inbuf, 256);
            if (nbytes == 0)
            {
                break;
            }
            files = (char *)realloc(files, strlen(files) + 256 + 1);
            strncat(files, inbuf, 256);
            memset(inbuf, 0, 256);
        }
        // printf("%s\n", files);
        char *line, *temp;
        char *dir;
        dir = malloc(sizeof(char) * strlen(files) + 1);
        line = strtok_r(files, "\n", &temp);
        do
        {
            // printf("current line = %s\n", line);
            if (strchr(line, ':') != NULL)
            {
                line[strlen(line) - 1] = '\0';
                strcpy(dir, line);
                // printf("Directory = %s\n", dir);
            }
            else
            {
                if (isDirectory(line))
                {
                    continue;
                }
                pthread_mutex_lock(&queueLock);
                if (push(line, dir, args->socket, args->address, args->address_len) == false)
                {
                    pthread_cond_wait(&queueFullCond, &queueLock);
                }
                else
                {
                    printf("[Thread: %ld] : Adding file %s to the queue...\n", pthread_self(), line);
                    pthread_cond_signal(&queueEmptyCond);
                }
                pthread_mutex_unlock(&queueLock);
            }
        } while ((line = strtok_r(NULL, "\n", &temp)) != NULL);
        free(files);
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
            // if (globalExit) {
            //     pthread_mutex_unlock(&queueLock);
            //     return NULL;
            // }
            // printf("Waiting\n");
            pthread_cond_wait(&queueEmptyCond, &queueLock);
        }
        // printf("Done Waiting\n");
        fn = pop();
        printf("[Thread: %ld]: Received task: <%s/%s, %d>\n", pthread_self(), fn->directory, fn->file, fn->socket);
        // file content
        char *filepath = malloc(strlen(fn->directory) + strlen(fn->file) + 2);
        memset(filepath, 0, strlen(fn->directory) + strlen(fn->file) + 2);
        strcat(filepath, fn->directory);
        strcat(filepath, "/");
        strcat(filepath, fn->file);
        printf("[Thread: %ld]: About to read file %s\n", pthread_self(), filepath);
        int readFile;
        if ((readFile = open(filepath, O_RDONLY, PERMS)) == -1)
        {
            perror("Failed to open file\n");
            exit(1);
        }
        // printf("%s\n", filepath);
        int block_size = *(int *)arg;
        char block[block_size];
        memset(block, 0, block_size);
        int nbytes;

        // send filename
        if ((sendto(fn->socket, fn->file, strlen(fn->file), 0, fn->address, fn->address_len)) < 0)
        {
            perror_exit("sendto");
        }

        while (1)
        {
            if (((nbytes = read(readFile, block, block_size)) < 0))
            {
                perror_exit("read");
            }
            // printf("Sending -%s- %d\n", block, nbytes);
            if (nbytes == 0)
            {
                char *metadata = getMetadata(filepath);

                if ((sendto(fn->socket, "EOF", 5, 0, fn->address, fn->address_len)) < 0)
                {
                    perror_exit("sendto");
                }
                if ((sendto(fn->socket, metadata, strlen(metadata), 0, fn->address, fn->address_len)) < 0)
                {
                    perror_exit("sendto");
                }
                // if ((sendto(fn->socket, "EOF", 5, 0, fn->address, fn->address_len)) < 0)
                // {
                //     perror_exit("sendto");
                // }
                free(metadata);
                break;
            }
            if ((sendto(fn->socket, block, block_size, 0, fn->address, fn->address_len)) < 0)
            {
                perror_exit("sendto");
            }
            memset(block, 0, block_size);
        }

        if (close(readFile) == -1)
        {
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

int isDirectory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

char *getMetadata(char *filepath)
{
    struct stat statbuf;
    if (stat(filepath, &statbuf) < 0)
    {
        perror("stat\n");
    }

    char *metadata = malloc(sizeof(char) * 1000);
    memset(metadata, 0, 1000);

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
    sprintf(tm_mon_str, "%d", dt.tm_mon);
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
    sprintf(tm_mon_str, "%d", dt.tm_mon);
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
    sprintf(tm_mon_str, "%d", dt.tm_mon);
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