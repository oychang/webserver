#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT "3421"
#define BACKLOG 5

#define MAXBUF 4096 // 4K
typedef char buf[MAXBUF];
typedef char string[MAXBUF];

// enum verb {GET, POST};
enum status {
    OK           = 200,
    NOT_FOUND    = 404,
    SERVER_ERROR = 500
};
