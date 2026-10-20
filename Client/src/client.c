#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include "../../common/include/protocol.h"


// парсинг входных параметров. Должке быть один
int parse_argv(int argc, char* argv[], const char** out_host){
    if(argc != 2){
        fprintf(stderr, "Bad init argumens");
        return -1;
    }
    *out_host = argv[1];
    return 0;
}

//цель функции - привести человекочитаемый ip и port в понятный сокетам и машине набор адресов
int resolve_address(const char* IP_adress, struct addrinfo **serfinfo ){
    int return_value;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((return_value = getaddrinfo(IP_adress, PORT, &hints, serfinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(return_value));
        return -1;
    }

    return 0;
}

// в argc должен передать IP адресс  
int client_start(int argc, char* argv[]){
    int sockfd; // файловый дескриптор сокета
    int numbytes; // точное количество байтов, которое пришло или ушло
    char buffer[MAX_BUF_SIZE]; // буффер для складывания полученных значений

    struct addrinfo  *serfinfo = NULL;
    struct addrinfo *p = NULL; // вспомогательные переменные для разрешения адреса
    int return_value;

    // считываем IP адресс в переменную 
    const char* IP_adress = NULL;
    if(parse_argv(argc, argv, &IP_adress) != 0){
        return -1;
    }

    if (resolve_address(IP_adress, &serfinfo) != 0){
        return -2;
    }

    


    return 0;
}