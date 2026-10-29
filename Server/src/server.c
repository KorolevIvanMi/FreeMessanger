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

#define BACKLOG 10

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

int try_to_bind(struct addrinfo* servinfo, int* sockfd){
    struct addrinfo *p = NULL; 
    int yes = 1;
    for(p = servinfo; p != NULL; p = p->ai_next){
        if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }
        if(setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            close(*sockfd);
            perror("server: setsockopt");
            return -1;
        }
        if(bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(*sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    if(p == NULL){
        fprintf(stderr, "server: failed to bind \n");
        return 2;
    }

    return 0;
}

void* get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_client_messages_cycle(int new_fd){
    char buf[MAX_BUF_SIZE];
        ssize_t n;
        while ((n = recv(new_fd, buf, sizeof buf - 1, 0)) > 0) {
            buf[n] = '\0';
            buf[strcspn(buf, "\n")] = '\0';
            if (buf[0] == '\0') continue; 
            printf("client: %s\n", buf);
        }

        if (n == 0){
            printf("client disconnected\n");
            return 0;
        } 
        else {
            perror("recv");
            return 1;
        }
}

int find_client_connection_cycle(int* sockfd){
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int new_fd;
    char s[INET6_ADDRSTRLEN];
    while(1){
        sin_size = sizeof(their_addr);
        new_fd = accept(*sockfd, (struct sockaddr*)&their_addr, &sin_size);
        if(new_fd == -1){
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (get_client_messages_cycle(new_fd) != 0){
            close(new_fd);
            continue;
        }

        close(new_fd);
    }
    return 0;
}



int server_start(int argc, char* argv[]){
    int sockfd = -1; // файловый дескриптор сокета
    // int numbytes = 0; // точное количество байтов, которое пришло или ушло
    // char buffer[MAX_BUF_SIZE]; // буффер для складывания полученных значений
    struct addrinfo  *servinfo = NULL;// вспомогательные переменные для разрешения адреса
    
    if (resolve_address(&servinfo) != 0){
        return 2;
    }

    if(try_to_bind(servinfo, &sockfd)){
        freeaddrinfo(servinfo);
        return 3;
    }

    freeaddrinfo(servinfo);
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        close(sockfd);
        return 4;
    }

    printf("server: waiting for connections...\n");
    find_client_connection_cycle(&sockfd);
    
    return 0;
}