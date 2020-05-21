#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <assert.h>

#define check_neg(x, str) do { \
                            if ((x) < 0) {\
                                perror(str);\
                                exit(1); } \
                        } while(0)


const int PORT = 51001;
const int PORT_worker = 51000;
const int MAX_WORKERS = 20;
const char* broadcast_msg = "WORK!";

int parse_num(int argc, char* argv[]);
int make_broadcast(struct sockaddr_in serv_addr);

int main(int argc, char* argv[])
{
    int num_workers = 0;
    if ((num_workers = parse_num(argc, argv)) < 0){
        perror("Invalid argument!\n");
        exit(1);
    }

    struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr = htonl(INADDR_ANY)
    };

    /**
     * BROADCAST PROCESS to discover available workers
    */
    if (make_broadcast(serv_addr) < 0){
        exit(1);
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int turn_on = 1;
    int ret3 = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
    assert(ret3 > -1);

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind error");
        exit(1);
    }

    if (listen(listenfd, MAX_WORKERS) < 0) {
        perror("Server listen error");
        exit(1);
    }

    int nready = 0;
    int workers[MAX_WORKERS];
    bzero(workers, sizeof(int) * MAX_WORKERS);
    fd_set set;
    int connfd;

    FD_ZERO(&set);
    FD_SET(listenfd, &set);
    struct timeval t = {
         .tv_sec = 7,
         .tv_usec = 0
       };

    while (nready < num_workers) {
        int ret = select(listenfd + 1, &set, NULL, NULL, &t);
        if (ret < 0){
            perror("Select ERROR\n");
            exit(1);
        }
        else if (ret == 0) {
            perror("Time limit exceeded\n");
            exit(-1);
        }

        workers[nready] = connfd; 
        nready++;

        connfd = accept(listenfd, NULL, NULL);
    }

}
int make_broadcast(struct sockaddr_in serv_addr)
{   
    int broadcastfd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(broadcastfd > 0);
    
    int broadcast_on = 1;
    int ret1 = setsockopt(broadcastfd, SOL_SOCKET, SO_BROADCAST, &broadcast_on, sizeof(int));
    assert(ret1 == 0);
    int ret2 = setsockopt(broadcastfd, SOL_SOCKET, SO_REUSEADDR, &broadcast_on, sizeof(int));
    assert(ret2 == 0);

    if (bind(broadcastfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("Bind error");
        return -1;
	}

    struct sockaddr_in broadcast_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT_worker),
            .sin_addr.s_addr = htonl(INADDR_BROADCAST) 
        };

    int retval = sendto(broadcastfd, (void*) broadcast_msg, sizeof(broadcast_msg), 0,
            (struct sockaddr*) &broadcast_addr, sizeof(broadcast_addr));
    
    if (retval < 0){
        perror("Sendto broadcast error\n");
        return -1;
    }

    close(broadcastfd);
    return 0;
}

int parse_num(int argc, char* argv[])
{   
    if (argc != 2)
        return -1;

    int n = 0;
    char c = 0;
    int nsymbl = 0;
    nsymbl = sscanf(argv[1], "%d%c", &n, &c);

    if (nsymbl != 1 || n < 1){
        perror("Invalid argument!\n");
        return -1;
    }
    else 
        return n;
}