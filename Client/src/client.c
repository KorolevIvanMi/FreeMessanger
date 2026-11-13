#include <stdbool.h>
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
#include <ncurses.h>
#include <locale.h>

#include "../include/client.h"
#include "../../common/include/protocol.h"

pthread_mutex_t lock;
WINDOW *win_chat;
WINDOW *win_input;

const char* client_name = NULL;

typedef struct{
    int fd;
} server_args_t;

void chat_append(const char* prefix, const char* msg ){
    if(prefix[0] == '\0'){
        pthread_mutex_lock(&lock);
        wprintw(win_chat, "%s\n", msg);
        wrefresh(win_chat);
        pthread_mutex_unlock(&lock);
    } else {
        pthread_mutex_lock(&lock);
        wprintw(win_chat, "%s: %s\n", prefix, msg);
        wrefresh(win_chat);
        pthread_mutex_unlock(&lock);
    }
    
}

static void input_draw(const char* line, int len){
    werase(win_input);
    mvwprintw(win_input, 1, 2, "> %.*s", getmaxx(win_input) - 4, line);
    wmove(win_input, 1, 4 + len);
    wrefresh(win_input);
}

// парсинг входных параметров. Должке быть один
int parse_argv(int argc, char* argv[], const char** out_host, const char** client_name){
    if(argc != 3){
        fprintf(stderr, "Bad init argumens");
        return -1;
    }
    *out_host = argv[1];
    *client_name = argv[2];
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

// чтение сообщений от сервера в отдельном потоке 
int read_server_message(int* sockfd){
    char buf[MAX_BUF_SIZE + 64];
    ssize_t n;
    while ((n = recv(*sockfd, buf, sizeof buf - 1, 0)) > 0) {
        buf[n] = '\0';
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0'){
            continue;
        }
        chat_append("", buf);
    }

    return 0;
}

// тот самый отдельный поток, который запускает функцию чтения
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

    // запуск отдельного потока
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
    int  n = 0;                       
    message_buf[0] = '\0';
    // первым сообщением отправляем имя пользователя 
    if (client_name != NULL) {
        char name_buf[MAX_BUF_SIZE];
        int name_len = snprintf(name_buf, sizeof name_buf, "%s\n", client_name);
        if (send(*sockfd, name_buf, name_len, 0) == -1) {
            return -1;
        }
    }
    while(1){
        input_draw(message_buf, n);
        int ch = wgetch(win_input);
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (n > 0) {
                if(send(*sockfd, message_buf, n + 1, 0) == -1){
                    return -1;
                }
                chat_append("me", message_buf);
            }
            n = 0;
            message_buf[0] = '\0';

        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (n > 0) message_buf[--n] = '\0';
        
        } else {
            if (n < (int)sizeof(message_buf) - 1) {
                message_buf[n++] = (char)ch;
                message_buf[n]   = '\0';
            }
        }
    }
    return 0;
}

// в argc должен передать IP адресс  
int client_start(int argc, char* argv[]){
    setlocale(LC_ALL, "");  
    initscr();
    cbreak();
    noecho();
    
    int row, col;
    getmaxyx(stdscr, row, col);

    int chat_h = row * 9/ 10;
    int input_h = row - chat_h;

    win_chat = newwin(chat_h, col, 0, 0);
    win_input = newwin(input_h, col, chat_h, 0);

    keypad(stdscr, true);
    keypad(win_input, true);

    scrollok(win_chat, true);
    box(win_chat, 0, 0);
    box(win_input, 0, 0);

    wrefresh(win_chat);
    wrefresh(win_input);

    // printf("%d; %d\n", row, col);
    pthread_mutex_init(&lock,NULL);
    int sockfd = -1; // файловый дескриптор сокета
    int numbytes = 0; // точное количество байтов, которое пришло или ушло
    char buffer[MAX_BUF_SIZE]; // буффер для складывания полученных значений

    struct addrinfo  *servinfo = NULL;// вспомогательные переменные для разрешения адреса
    
    int return_value;

    // считываем IP адресс в переменную 
    const char* IP_adress = NULL;
    
    if(parse_argv(argc, argv, &IP_adress, &client_name) != 0){
        endwin();
        return 1;
    }

    if (resolve_address(IP_adress, &servinfo) != 0){
        endwin();
        return 2;
    }

    if (try_to_connect(servinfo, &sockfd) != 0){
        freeaddrinfo(servinfo);
        endwin();
        return 3;
    }

    freeaddrinfo(servinfo);

    if(send_cycle(&sockfd) != 0){
        close(sockfd);
        endwin();                 
        return 4;
    }

    close(sockfd);
    endwin();                     
    return 0;
}