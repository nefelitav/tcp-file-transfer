#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include "../include/utilities.h"
#include "../include/server.h"

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void child_server(int newsock) {
    char buf[1];
    while(read(newsock, buf, 1) > 0) {  
    	putchar(buf[0]);           
    	if (write(newsock, buf, 1) < 0)
    	    perror_exit("write");
    }
    printf("Closing connection.\n");
    close(newsock);	 
}

void sigchld_handler (int sig) {
	while (waitpid(-1, NULL, WNOHANG) > 0);
}


//////////////////////////////////
// FileQueue    
/////////////////////////////////

void createFileQueue() {
    queue = malloc(sizeof(fileQueue));
    queue->first = NULL;
    queue->last = NULL;
    queue->currSize = 0;
}

bool isEmpty() {
    return (queue->currSize == 0);
}

void push(char* newfile) {
    if (queue->last == NULL) {
        // empty queue
        queue->last = (fileNode*)malloc(sizeof(fileNode));
        queue->last->file = malloc(sizeof(char) * strlen(newfile) + 1);
        strcpy(queue->last->file, newfile);
        queue->last->next = NULL;
        queue->first = queue->last;
    } else {
        queue->last->next = (fileNode*)malloc(sizeof(fileNode));
        queue->last = queue->last->next;
        queue->last->file = malloc(sizeof(char) * strlen(newfile) + 1);
        strcpy(queue->last->file, newfile);
        queue->last->next = NULL;
    }
    queue->currSize++;
}

fileNode *pop() {
    if (queue->currSize == 0) {
        return NULL;
    }
    fileNode *toReturn = queue->first;
    if (queue->first == queue->last) {
        queue->last = NULL;
    }
    queue->first = queue->first->next;
    queue->currSize--;
    return toReturn;
}

void printQueue() {
    fileNode *curr = queue->first;
    printf("\n############################################\n");
    while (curr != NULL) {
        printf("file = %s\n", curr->file);
        curr = curr->next;
    }
    printf("############################################\n");
}

void deleteFileQueue() {
    fileNode *curr = queue->first;
    fileNode *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
    free(queue);
}

