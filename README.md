# TCP File Transfer

[![C CI Actions Status](https://github.com/nefelitav/tcp-file-transfer/workflows/C%20CI/badge.svg)](https://github.com/nefelitav/tcp-file-transfer/actions)


This program simulates the socket communication between a server and many clients. 
- The client sends the name of a directory from the server's filesystem and the server through a communication thread, that uses fork, exec and a pipe with ls, pushes to a queue every file of this directory (ls is used with the -R and -a flags, so that all files, even hidden ones are included). The result of ls is being processed, so that we get the useful information. When the queue stops being empty, worker threads pop the files from the queue and send their content in blocks to the client. For this task, the first message sent from the server to the client is the block size, so that the client knows how many bytes to expect. The second message is the name of the file. Then the file content is sent and an EOF message when finishing. Finally, some file metadata are transfered and then a CONT or END message depending on whether all the files of the directory have been sent to the client and the connection should be terminated. I check this, using a list that stores the socket-thread relationships. When a thread starts working on a client, this information is pushed to the list and when a thread finishes with this client, it is removed from the list. Thus, in order to check if the client has been fully served, I check that no worker thread is still working for this client. Plus, some useful file metadata are sent to the client, such as the permissions, the file size etc. 
- On the other hand, the client receives all that data, accumulates the file content and finally checks if the same file exists on their own filesystem. If it exists, they delete it, using fork and exec with rm, else they create the corresponding folder structure, using fork and exec with mkdir (mkdir is used with the -p flag, so that all the intermediate directories are created). Then, they clone the file from the server's filesystem. 
- Mutexes are used whenever global resources are accesed to protect the data and condition variables are used to avoid busy waiting. For example, some used mutexes correspond to the file queue, the sockets and the struct that stores the thread-client relationship. Moreover, I used condition variables to wait as a worker thread when the file queue is empty and as a communication thread when the file queue is full. Thread synchronization has been tested with the helgrind and drd tools. Finally, the server is terminated on receival of SIGINT signal where a handler frees all the allocated resources.

## Compile & Run Server

```
$ make dataServer && ./dataServer -p <port> -s <thread pool size> -q <queue size> -b <block size>
```

## Compile & Run Client

```
$ make remoteClient && ./remoteClient -i <server ip> -p <port> -d <directory>
```

## Memcheck

```
$ make valgrind_server
$ make valgrind_client
```

## Data race detection

```
$ make helgrind
$ make drd
```
