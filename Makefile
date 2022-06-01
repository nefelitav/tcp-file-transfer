OBJS1 = server.o utilities.o
OBJS2 = client.o utilities.o
OUT1 = dataServer
OUT2 = remoteClient
CC = gcc
FLAGS = -g -c -Wall

dataServer : $(OBJS1)
	$(CC) -g -pthread -Wall -o $(OUT1) $(OBJS1)

remoteClient : $(OBJS2)
	$(CC) -g -pthread -Wall -o $(OUT2) $(OBJS2)

server.o : ./src/server.c
	$(CC) $(FLAGS) ./src/server.c

client.o : ./src/client.c
	$(CC) $(FLAGS) ./src/client.c

utilities.o : ./src/utilities.c
	$(CC) $(FLAGS) ./src/utilities.c

valgrind_server : $(OUT1)
	valgrind --leak-check=full --show-leak-kinds=all  --track-origins=yes ./dataServer -p 8012 -s 2 -q 2 -b 100

valgrind_client : $(OUT2)
	valgrind --leak-check=full --show-leak-kinds=all  --track-origins=yes ./remoteClient -i localhost -p 8012 -d /mnt/c/Users/ntavoula/Desktop/test

helgrind: $(OUT1)
	valgrind --tool=helgrind ./$(OUT1) -p 8012 -s 2 -q 2 -b 100

drd: $(OUT1)
	valgrind --tool=drd ./$(OUT1) -p 8012 -s 2 -q 2 -b 100

clean :
	rm -f $(OBJS1) $(OBJS2) $(OUT1) $(OUT2)