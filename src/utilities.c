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
#include "../include/server.h"

// file queue
fileQueue *queue;
// thread-socket relationship
assignmentQueue *assignments;

// each socket and its mutex
socketMutexQueue *socketMutexes;

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

//////////////////////////////////
// FileQueue
/////////////////////////////////

void createFileQueue(unsigned int maxSize)
{
    queue = malloc(sizeof(fileQueue));
    queue->first = NULL;
    queue->last = NULL;
    queue->currSize = 0;
    queue->maxSize = maxSize;
}

bool isEmpty()
{
    return (queue->currSize == 0);
}

bool isFull()
{
    return (queue->currSize == queue->maxSize);
}

bool stillServingClient(int socket)
{
    fileNode *curr = queue->first;
    fileNode *next;
    while (curr != NULL)
    {
        next = curr->next;
        // still in list
        if (curr->socket == socket)
        {
            return true;
        }
        curr = next;
    }
    return false;
}

bool push(char *newfile, char *fileDir, int socket, struct sockaddr *address, socklen_t address_len)
{
    if (isFull())
    {
        return false;
    }
    if (queue->last == NULL)
    {
        // empty queue
        queue->last = (fileNode *)malloc(sizeof(fileNode));
        queue->last->file = malloc(sizeof(char) * strlen(newfile) + 1);
        queue->last->directory = malloc(sizeof(char) * strlen(fileDir) + 1);
        strcpy(queue->last->file, newfile);
        strcpy(queue->last->directory, fileDir);
        queue->last->address = address;
        queue->last->address_len = address_len;
        queue->last->socket = socket;
        queue->last->next = NULL;
        queue->first = queue->last;
    }
    else
    {
        queue->last->next = (fileNode *)malloc(sizeof(fileNode));
        queue->last = queue->last->next;
        queue->last->file = malloc(sizeof(char) * strlen(newfile) + 1);
        queue->last->directory = malloc(sizeof(char) * strlen(fileDir) + 1);
        strcpy(queue->last->file, newfile);
        strcpy(queue->last->directory, fileDir);
        queue->last->address = address;
        queue->last->address_len = address_len;
        queue->last->socket = socket;
        queue->last->next = NULL;
    }
    queue->currSize++;
    return true;
}

fileNode *pop()
{
    if (queue->currSize == 0)
    {
        return NULL;
    }
    fileNode *toReturn = queue->first;
    if (queue->first == queue->last)
    {
        queue->last = NULL;
    }
    queue->first = queue->first->next;
    queue->currSize--;
    return toReturn;
}

void printQueue()
{
    fileNode *curr = queue->first;
    printf("\n############################################\n");
    while (curr != NULL)
    {
        printf("file = %s/%s in socket %d\n", curr->directory, curr->file, curr->socket);
        curr = curr->next;
    }
    printf("############################################\n");
}

void deleteFileQueue()
{
    fileNode *curr = queue->first;
    fileNode *next;
    while (curr != NULL)
    {
        next = curr->next;
        free(curr->file);
        free(curr->directory);
        free(curr);
        curr = next;
    }
    free(queue);
}

//////////////////////////////////
// AssignmentQueue
/////////////////////////////////

void createAssignmentQueue()
{
    assignments = malloc(sizeof(assignmentQueue));
    assignments->first = NULL;
}
void deleteAssignmentQueue()
{
    assignment *curr = assignments->first;
    assignment *next;
    while (curr != NULL)
    {
        next = curr->next;
        free(curr);
        curr = next;
    }
    free(assignments);
}
void pushAssignment(int socket, pthread_t thread)
{
    assignment *head = assignments->first;
    assignment *newHead = (assignment *)malloc(sizeof(assignment));
    newHead->thread = thread;
    newHead->socket = socket;
    newHead->next = head;
    assignments->first = newHead;
}

// worker thread done with this client
void popAssignment(int socket, pthread_t thread)
{
    assignment *curr = assignments->first;
    assignment *next;
    assignment *prev = assignments->first;
    while (curr != NULL)
    {
        next = curr->next;
        if (curr->socket == socket && curr->thread == thread)
        {
            if (curr == assignments->first)
            {
                assignments->first = curr->next;
            }
            else
            {
                prev->next = next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = next;
    }
}

// last worker thread that handles this client
bool isLast(int socket)
{
    assignment *curr = assignments->first;
    assignment *next;
    while (curr != NULL)
    {
        next = curr->next;
        if (curr->socket == socket)
        {
            return false;
        }
        curr = next;
    }
    return true;
}

//////////////////////////////////
// MutexQueue
/////////////////////////////////

void createMutexQueue()
{
    socketMutexes = malloc(sizeof(socketMutexQueue));
    socketMutexes->first = NULL;
}
void deleteMutexQueue()
{
    socketMutex *curr = socketMutexes->first;
    socketMutex *next;
    while (curr != NULL)
    {
        next = curr->next;
        free(curr);
        curr = next;
    }
    free(socketMutexes);
}
void pushMutex(int socket)
{
    socketMutex *head = socketMutexes->first;
    socketMutex *newHead = (socketMutex *)malloc(sizeof(socketMutex));
    newHead->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    newHead->socket = socket;
    newHead->next = head;
    socketMutexes->first = newHead;
}

void lock(int socket)
{
    socketMutex *curr = socketMutexes->first;
    socketMutex *next;
    while (curr != NULL)
    {
        next = curr->next;
        if (curr->socket == socket)
        {
            pthread_mutex_lock(&(curr->mutex));
            return;
        }
        curr = next;
    }
}
void unlock(int socket)
{
    socketMutex *curr = socketMutexes->first;
    socketMutex *next;
    while (curr != NULL)
    {
        next = curr->next;
        if (curr->socket == socket)
        {
            pthread_mutex_unlock(&(curr->mutex));
            return;
        }
        curr = next;
    }
}