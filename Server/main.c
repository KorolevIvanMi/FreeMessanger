#include "include/server.h"

int main(int argc, char* argv[]){
    int error = 0;
    error = server_start(argc, argv);
    return error;
}