#ifndef UTILITIES
#define UTILITIES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdbool.h>

void child_server(int newsock);
void sigchld_handler(int sig);
void perror_exit(char *message);

typedef struct fileNode fileNode;

struct fileNode
{
    char *file;
    char *directory;
    int socket;
    struct sockaddr *address;
    socklen_t address_len;
    fileNode *next;
};

typedef struct fileQueue fileQueue;

struct fileQueue
{
    fileNode *first;
    fileNode *last;
    unsigned int currSize;
    unsigned int maxSize;
};

void createFileQueue(unsigned int maxSize);
void deleteFileQueue();
bool stillServingClient(int socket);
bool isEmpty();
bool isFull();
bool push(char *newfile, char *fileDir, int socket, struct sockaddr *address, socklen_t address_len);

fileNode *pop();
void printQueue();

extern fileQueue *queue;

typedef struct assignment assignment;

struct assignment
{
    int socket;
    pthread_t thread;
    assignment *next;
};

typedef struct assignmentQueue assignmentQueue;

struct assignmentQueue
{
    assignment *first;
};

extern assignmentQueue *assignments;

void createAssignmentQueue();
void deleteAssignmentQueue();
void pushAssignment(int socket, pthread_t thread);
void popAssignment();
bool isLast(int socket);

////////////////////////////////

typedef struct socketMutex socketMutex;

struct socketMutex
{
    int socket;
    pthread_mutex_t mutex;
    socketMutex *next;
};

typedef struct socketMutexQueue socketMutexQueue;

struct socketMutexQueue
{
    socketMutex *first;
};

extern socketMutexQueue *socketMutexes;

void createMutexQueue();
void deleteMutexQueue();
void pushMutex(int socket);
void lock(int socket);
void unlock(int socket);

#endif