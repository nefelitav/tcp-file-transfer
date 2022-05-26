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
#include <netdb.h>	        
#include "../include/utilities.h"

int main(int argc, char **argv) {
    int server_port, sock;
    char* directory;
    char* server_ip;

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;

    if (argc < 4) {
    	printf("Please give server ip, server port and directory\n");
       	exit(1);
    }
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            server_ip = malloc(strlen(argv[i+1]) + 1);
            strcpy(server_ip, argv[i+1]);
        }

        if (strcmp(argv[i], "-p") == 0) {
            server_port = atoi(argv[i+1]);
        }

        if (strcmp(argv[i], "-d") == 0) {
            directory = malloc(strlen(argv[i+1]) + 1);
            strcpy(directory, argv[i+1]);
        }
    }

    printf("Client's parameters are:\n");
    printf("serverIP: %s\n", server_ip);
    printf("port: %d\n", server_port);
    printf("directory: %s\n", directory);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    	perror_exit("socket");
    }

    if ((rem = gethostbyname(server_ip)) == NULL) {	
	   herror("gethostbyname"); 
       exit(1);
    }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(server_port);

    if (connect(sock, serverptr, sizeof(server)) < 0) {
	   perror_exit("connect");
    }
    
    printf("Connecting to %s on port %d\n", server_ip, server_port);

    if (sendto(sock, directory, strlen(directory), 0, serverptr, sizeof(server)) < 0) {
	   perror_exit("sendto");
    }

    // strtok filepath -> fname, path

    // pid_t pid;
    // if (access(fname, F_OK) == 0) {
        // file exists
        // if ((pid = fork()) == -1) {
        //     perror("Failed to fork\n");
        //     exit(1);
        // }
        // if (pid == 0) {
        //     if (execl("/bin/rm", "rm", filepath, (char *)0) == -1) { 
        //         perror("Execl Failed\n");
        //         exit(1);
        //     }
        // }
    // } else {
        // file doesn't exist
        // if ((pid = fork()) == -1) {
        //     perror("Failed to fork\n");
        //     exit(1);
        // }
        // if (pid == 0) {
        //     if (execl("/bin/mkdir", "mkdir", "-p", path, (char *)0) == -1) { 
        //         perror("Execl Failed\n");
        //         exit(1);
        //     }
        // }
        // write

    // }



    close(sock); 
    free(directory);
    free(server_ip);
}