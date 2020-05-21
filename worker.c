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


const int PORT = 51000;
const int PORT_serv = 51001;

const int MAX_WORKERS = 20;
const char* broadcast_msg = "WORK!";



int main()
{
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    int tcp = socket(AF_INET, SOCK_STREAM, 0);
    int turn_on;
    setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));

    struct sockaddr_in worker_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };

    bind(udp, (struct sockaddr*)& worker_addr, sizeof(worker_addr));
    bind(tcp, (struct sockaddr*)& worker_addr, sizeof(worker_addr));
    
    char buf[100] = "";
    struct sockaddr_in serv_addr;
    socklen_t len = sizeof(serv_addr);
    if (recvfrom(udp, buf, strlen(buf), 0, (struct sockaddr*)&serv_addr, &len) < 0){
        perror("recfrom\n");
        exit(1);
    }

    if (connect(tcp, (struct sockaddr*)&serv_addr, len) < 0){
        perror("connect");
        exit(1);
    }

    printf("OK \n");

    return 0;
}