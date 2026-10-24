#include "include/client.h"


int main(int argc, char* argv[]){
    int error = 0;
    error = client_start(argc, argv);
    return error;
}