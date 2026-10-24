#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "../../common/include/protocol.h"

int resolve_address(struct addrinfo **servinfo ){
    int return_value;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((return_value = getaddrinfo(NULL, PORT, &hints, servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(return_value));
        return -1;
    }
    return 0;
}


int server_start(int argc, char* argv[]){
    int sockfd = -1; // файловый дескриптор сокета
    int numbytes = 0; // точное количество байтов, которое пришло или ушло
    char buffer[MAX_BUF_SIZE]; // буффер для складывания полученных значений

    struct addrinfo  *servinfo = NULL;// вспомогательные переменные для разрешения адреса
    
    int return_value;
    if (resolve_address(&servinfo) != 0){
        return 2;
    }
    
    return 0;
}