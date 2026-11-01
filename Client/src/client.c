#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdlib.h>

#include "../include/client.h"
#include "../../common/include/protocol.h"

pthread_mutex_t lock;


typedef struct{
    int fd;
} server_args_t;


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

int read_server_message(int* sockfd){
    char buf[MAX_BUF_SIZE];
    ssize_t n;
    while ((n = recv(*sockfd, buf, sizeof buf - 1, 0)) > 0) {
        buf[n] = '\0';
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0'){
            continue;
        }
        pthread_mutex_lock(&lock);
        printf("other_client: %s\n", buf); 
        pthread_mutex_unlock(&lock);
    }

    return 0;
}

void* server_thread(void* arg){
    server_args_t* args = arg;
    int fd = args->fd;
    free(args);
    read_server_message(&fd);
    return NULL;
}

// пытаемся создать соединение с сервером, перебирая все адреса 
int try_to_connect(struct addrinfo* serfinfo, int* sockfd){
    struct addrinfo *p = NULL; 
    
    for(p = serfinfo; p != NULL; p = p->ai_next){
        if((*sockfd = socket(p->ai_family, p->ai_socktype , p->ai_protocol)) == -1){
            // perror("client: socket");
            continue;
        }

        if(connect(*sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(*sockfd);
            // perror("client: connect");
            continue;
        }
        break;
    }
    if(p == NULL){
        fprintf(stderr, "client: failed to connect \n");
        return -1;
    }

    server_args_t* args = malloc(sizeof *args);
    args->fd = *sockfd;

    pthread_t tid;
    int err = pthread_create(&tid, NULL, server_thread, args);
    if (err != 0) {
        free(args);
        return -1;
    }
    pthread_detach(tid);
    return 0;
}

// простой цикл бесконечной отправки сообщений
int send_cycle(int* sockfd){
    char message_buf[MAX_BUF_SIZE];
    char server_message[MAX_BUF_SIZE];
    ssize_t n;
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
    pthread_mutex_init(&lock,NULL);
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