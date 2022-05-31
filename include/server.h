#ifndef SERVER
#define SERVER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include "../include/utilities.h"
#include "../include/client.h"

void *read_directory(void *arg);
void *worker_job(void *arg);
int isDirectory(const char *dir, const char *path);
char *getMetadata(char *filepath);

typedef struct
{
    int socket;
    struct sockaddr *address;
    socklen_t address_len;
} communication_thread_args;

extern pthread_t *worker_threads;
extern pthread_mutex_t queueLock;
extern pthread_mutex_t assignLock;
extern pthread_mutex_t socketLock;
extern pthread_cond_t queueFullCond;
extern pthread_cond_t queueEmptyCond;
extern struct sockaddr_in server;
extern struct sockaddr *serverptr;
extern socklen_t serverlen;

#endif