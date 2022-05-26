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
void sigchld_handler (int sig);
void perror_exit(char *message);


typedef struct fileNode fileNode;

struct fileNode {
    char* file;
    fileNode *next;
};

typedef struct fileQueue fileQueue;

struct fileQueue {
    fileNode *first;
    fileNode *last;
    unsigned int currSize;
};

void createFileQueue();
void deleteFileQueue();
bool isEmpty();
void push(char* newfile);
fileNode *pop();
void printQueue();


#endif