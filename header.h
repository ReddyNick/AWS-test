#ifndef _HEADER 
#define _HEADER

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <pthread.h>

const int PORT_server = 51001;
const int PORT_worker = 51000;

struct task {
    int worker_id;
    double from, to, dx;
};

#endif //_HEADER