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


void* read_directory(void *arg);
void *worker_job(void *arg);
int isDirectory(const char *path);

typedef struct {
    int socket;
    struct sockaddr * address;
    socklen_t * address_len;
} read_dir_args_struct;

extern pthread_t* worker_threads;
extern pthread_mutex_t queueLock;
extern pthread_cond_t queueFullCond;
extern pthread_cond_t queueEmptyCond;

#endif