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
#include "../include/client.h"
#define PERMS 0666

int main(int argc, char **argv)
{
    int server_port, sock;
    char *directory;
    char *server_ip;

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct hostent *rem;

    if (argc < 4)
    {
        printf("Please give server ip, server port and directory\n");
        exit(1);
    }
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            server_ip = malloc(strlen(argv[i + 1]) + 1);
            strcpy(server_ip, argv[i + 1]);
        }

        if (strcmp(argv[i], "-p") == 0)
        {
            server_port = atoi(argv[i + 1]);
        }

        if (strcmp(argv[i], "-d") == 0)
        {
            directory = malloc(strlen(argv[i + 1]) + 1);
            strcpy(directory, argv[i + 1]);
        }
    }

    printf("Client's parameters are:\n");
    printf("serverIP: %s\n", server_ip);
    printf("port: %d\n", server_port);
    printf("directory: %s\n", directory);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("socket");
    }

    if ((rem = gethostbyname(server_ip)) == NULL)
    {
        herror("gethostbyname");
        exit(1);
    }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(server_port);

    if (connect(sock, serverptr, sizeof(server)) < 0)
    {
        perror_exit("connect");
    }

    printf("Connecting to %s on port %d\n", server_ip, server_port);

    if (sendto(sock, directory, strlen(directory), 0, serverptr, sizeof(server)) < 0)
    {
        perror_exit("sendto");
    }
    unsigned int serverlen = sizeof(server);
    char block[100];
    memset(block, 0, 100);
    char *filecontent = malloc(sizeof(char) * 1000 + 1);
    memset(filecontent, 0, 1000 + 1);
    int n;
    char filename[256];
    memset(filename, 0, 256);

    int i = 0;
    while (1)
    {
        if (i == 0)
        {
            if (((n = recvfrom(sock, filename, sizeof(filename), 0, serverptr, &serverlen)) < 0))
            {
                perror_exit("recvfrom");
            }
            printf("-----%ld %s-----\n", sizeof(filename), filename);
            i++;
            continue;
        }
        if (((n = recvfrom(sock, block, sizeof(block), 0, serverptr, &serverlen)) < 0))
        {
            perror_exit("recvfrom");
        }
        // printf("-----%d %s\n", n, block);

        // end of this file
        if (n == 0 || strcmp(block, "EOF") == 0)
        {
            // printf("%s\n", filecontent);
            // write_file(directory, filename, filecontent);
            i = 0;
            memset(filename, 0, 256);
            memset(filecontent, 0, strlen(filecontent));
            memset(block, 0, 100);
            // char metadataLength[10];
            // memset(metadataLength, 0, 10);
            // if (((n = recvfrom(sock, metadataLength, 10, 0, serverptr, &serverlen)) < 0))
            // {
            //     perror_exit("recvfrom");
            // }
            char metadata[1000];
            memset(metadata, 0, 1000);
            if (((n = recvfrom(sock, metadata, 1000, 0, serverptr, &serverlen)) < 0))
            {
                perror_exit("recvfrom");
            }
            printf("\nReceived file metadata:");
            printf("\n-%s-\n", metadata);
            memset(metadata, 0, 1000);
            // memset(metadataLength, 0, 10);
            char finishLine[5];
            memset(finishLine, 0, 5);
            if (((n = recvfrom(sock, finishLine, sizeof(finishLine), 0, serverptr, &serverlen)) < 0))
            {
                perror_exit("recvfrom");
            }
            if (strcmp(finishLine, "END") == 0)
            {
                printf("finishing\n");
                break;
            }
            else
            {
                printf("continuing\n");
            }
            memset(finishLine, 0, 5);
            continue;
        }
        filecontent = (char *)realloc(filecontent, strlen(filecontent) + 256 + 1);
        strncat(filecontent, block, 256);
        memset(block, 0, 100);
        i++;
    }
    free(filecontent);
    close(sock);
    free(directory);
    free(server_ip);
}

void write_file(char *directory, char *filename, char *filecontent)
{

    printf("Creating file %s in directory %s\n", filename, directory);

    char *filepath = malloc(strlen(directory) + strlen(filename) + 2);
    memset(filepath, 0, strlen(directory) + strlen(filename) + 2);
    strcat(filepath, directory);
    strcat(filepath, "/");
    strcat(filepath, filename);

    pid_t pid;
    if (access(filepath, F_OK) == 0)
    {
        printf("File exists\n");
        if ((pid = fork()) == -1)
        {
            perror("Failed to fork\n");
            exit(1);
        }
        if (pid == 0)
        {
            printf("Deleting file\n");
            if (execl("/bin/rm", "rm", filepath, (char *)0) == -1)
            {
                perror("Execl Failed\n");
                exit(1);
            }
        }
    }
    else
    {
        // file doesn't exist
        if ((pid = fork()) == -1)
        {
            perror("Failed to fork\n");
            exit(1);
        }
        if (pid == 0)
        {
            // create folders
            if (execl("/bin/mkdir", "mkdir", "-p", directory, (char *)0) == -1)
            {
                perror("Execl Failed\n");
                exit(1);
            }
        }
    }
    while (access(filepath, F_OK) == 0)
        ;
    create_file(filepath, filecontent);
    free(filepath);
}

void create_file(char *filepath, char *filecontent)
{
    printf("Write to new file\n");
    int writeFile;
    if ((writeFile = open(filepath, O_CREAT | O_RDWR, PERMS)) == -1)
    {
        perror("Failed to open file\n");
        exit(1);
    }

    if (write(writeFile, filecontent, strlen(filecontent)) == -1)
    {
        perror("Failed to write to file\n");
        exit(1);
    }

    if (close(writeFile) == -1)
    {
        perror("Failed to close file\n");
        exit(1);
    }
}