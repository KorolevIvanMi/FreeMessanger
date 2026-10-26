#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include "../include/client.h"
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
int resolve_address(const char* IP_adress, struct addrinfo **servinfo ){
    int return_value;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((return_value = getaddrinfo(IP_adress, PORT, &hints, servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(return_value));
        return -1;
    }
    return 0;
}

// пытаемся создать соединение с сервером, перебирая все адреса 
int try_to_connect(struct addrinfo* serfinfo, int* sockfd){
    struct addrinfo *p = NULL; 
    for(p = serfinfo; p != NULL; p = p->ai_next){
        if((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }
        if(connect(*sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(*sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if(p == NULL){
        fprintf(stderr, "client: failed to connect \n");
        return -1;
    }
    return 0;
}

// простой цикл бесконечной отправки сообщений
int send_cycle(int* sockfd){
    char message_buf[MAX_BUF_SIZE];
    while(1){
        if(fgets(message_buf, sizeof(message_buf),stdin )== NULL){
            break;
        } 
        message_buf[strcspn(message_buf, "\n")] = 0;
        if(send(*sockfd, message_buf, sizeof(message_buf), 0) == -1){
            perror("send");
            return -1;
        }
    }
    return 0;
}

// в argc должен передать IP адресс  
int client_start(int argc, char* argv[]){
    int sockfd = -1; // файловый дескриптор сокета
    int numbytes = 0; // точное количество байтов, которое пришло или ушло
    char buffer[MAX_BUF_SIZE]; // буффер для складывания полученных значений

    struct addrinfo  *servinfo = NULL;// вспомогательные переменные для разрешения адреса
    
    int return_value;

    // считываем IP адресс в переменную 
    const char* IP_adress = NULL;
    if(parse_argv(argc, argv, &IP_adress) != 0){
        return 1;
    }

    if (resolve_address(IP_adress, &servinfo) != 0){
        return 2;
    }

    if (try_to_connect(servinfo, &sockfd) != 0){
        freeaddrinfo(servinfo);
        return 3;
    }

    freeaddrinfo(servinfo);

    if(send_cycle(&sockfd) != 0){
        close(sockfd);
        return 4;
    }

    close(sockfd);

    return 0;
}