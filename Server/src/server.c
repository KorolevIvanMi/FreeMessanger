#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdlib.h>

#include <pthread.h>


#include "../../common/include/protocol.h"

#define BACKLOG 10
pthread_mutex_t lock;
static int clients[BACKLOG] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static char clients_name[BACKLOG][50];

typedef struct{
    int fd;
    int client_id;
} client_args_t;

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
// находит свободный слот для клиента
int find_free_client_slot(){
    for(int i = 0 ; i <= 9; i++){
        if(clients[i] == -1){
            return i;
        }
    }
    return -1;
}

// попытка забиндиться на указанный адрес
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

// получение адреса, как IPv4 так и IPv6
void* get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_message_to_clients(int sender_id, const char buf[MAX_BUF_SIZE]){
    char out[MAX_BUF_SIZE + 64];
    int len = snprintf(out, sizeof out, "%s: %s\n",
                       clients_name[sender_id], buf);   // ← имя отправителя
    if (len < 0) return;
    if ((size_t)len >= sizeof out) len = sizeof out - 1; // обрезали — шлём сколько есть

    for (int i = 0; i < BACKLOG; i++) {
        if (clients[i] != -1 && i != sender_id) {
            if (send(clients[i], out, len, 0) == -1) {
                perror("send_to_clients");
            }
        }
    }
}

// цикл приёма сообщений от пользователя
int get_client_messages_cycle(int new_fd, int client_id){
    char buf[MAX_BUF_SIZE];
    char nameBuf[50];
    ssize_t n;
    if ((n = recv(new_fd, nameBuf, sizeof nameBuf - 1, 0)) > 0) {
        nameBuf[n] = '\0';
        nameBuf[strcspn(nameBuf, "\n")] = '\0';  
        if (nameBuf[0] != '\0' && nameBuf[0] != ' ') {
            strncpy(clients_name[client_id], nameBuf, sizeof clients_name[client_id] - 1);
            clients_name[client_id][sizeof clients_name[client_id] - 1] = '\0';
        }
    }

    while ((n = recv(new_fd, buf, sizeof buf - 1, 0)) > 0) {
        buf[n] = '\0';
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0'){
            continue;
        } 
        pthread_mutex_lock(&lock);
        printf("%s: %s\n",clients_name[client_id], buf); 
        send_message_to_clients(client_id, buf);
        pthread_mutex_unlock(&lock);
            
    }

    if (n == 0){
        printf("client disconnected\n");
        clients[client_id] = -1;
        return 0;
    } 
    else {
        perror("recv");
        return 1;
    }
}

// поток клиента в которой запускается цикл приёма сообщений 
void* client_thread(void* arg){
    client_args_t* args = arg;
    int fd = args->fd;
    int client_id = args->client_id;
    free(args);

    get_client_messages_cycle(fd, client_id);
    close(fd);
    return NULL;
}

// цикл поиска клиентов и установка соединения с ними
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

        // создание отдельного потока с функцией клиента
        client_args_t* args = malloc(sizeof *args);
        if (!args) { close(new_fd); continue; }
        args->fd = new_fd;
        int client_id = find_free_client_slot();
        args->client_id = client_id;
        if(client_id == -1){
            perror("Server overloaded");
            break;
        }
        clients[args->client_id] = new_fd;
        strcpy(clients_name[args->client_id], "АБОБА");
        pthread_t tid;
        int err = pthread_create(&tid, NULL, client_thread, args);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            free(args);
            close(new_fd);
            continue;
        }

        pthread_detach(tid);
    }
    return 0;
}

int server_start(int argc, char* argv[]){
    pthread_mutex_init(&lock, NULL);
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
    
    pthread_mutex_destroy(&lock);
    return 0;
}