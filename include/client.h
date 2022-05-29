#ifndef CLIENT
#define CLIENT

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

void write_file(char *directory, char *filename, char *filecontent);
void create_file(char *filepath, char *filecontent);

#endif