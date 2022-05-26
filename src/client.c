#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>	
#include "../include/utilities.h"


int main(int argc, char **argv) {
    int server_ip, server_port, sock;
    char* directory;

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;

    if (argc < 4) {
    	printf("Please give server ip, server port and directory\n");
       	exit(1);
    }
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            server_ip = atoi(argv[i+1]);
        }

        if (strcmp(argv[i], "-p") == 0) {
            server_port = atoi(argv[i+1]);
        }

        if (strcmp(argv[i], "-d") == 0) {
            directory = malloc(strlen(argv[i+1]) + 1);
            strcpy(directory, argv[i+1]);
        }
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    	perror_exit("socket");
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);

    if (connect(sock, serverptr, sizeof(server)) < 0) {
	   perror_exit("connect");
    }
    printf("Connecting to port %d\n", server_port);

    // sendto(int sock, void * buff, size_t length, int flags, struct sockaddr * dest_addr, socklen_t dest_len);

    close(sock); 
    free(directory);
}