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
#include <math.h>

#define check_neg(x, str) do { \
                            if ((x) < 0) {\
                                perror(str);\
                                exit(1); } \
                        } while(0)


const int PORT = 51000;
const int PORT_serv = 51001;

const int MAX_WORKERS = 20;
const char* broadcast_msg = "WORK!";

struct task {
    int worker_id;
    double from, to, dx;
};
int routine(double* res, struct task mytask);
int main()
{
    struct sockaddr_in worker_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    int turn_on = 1;
    setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
    bind(udp, (struct sockaddr*)& worker_addr, sizeof(worker_addr));
     
    
    struct sockaddr_in serv_addr;
    socklen_t len = sizeof(serv_addr);

        fflush(stdout);
        fflush(stdin);

        printf("<Listening broadcast>...");  
        int tcp = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
        
        //timeout
        // struct timeval tv;
        // tv.tv_sec = 5;
        // tv.tv_usec = 0;
        // setsockopt(tcp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        bind(tcp, (struct sockaddr*)& worker_addr, sizeof(worker_addr));

        //-----------------------------------------------
        char buf[100];
        if (recvfrom(udp, buf, strlen(buf), 0, (struct sockaddr*)&serv_addr, &len) < 0) {
            printf("recfrom\n");
            close(tcp); close(udp);
            exit(1);
        }
        printf("recieved broadcast!\n");
        
        printf("<Trying to connect the the server>...");
        
        if (connect(tcp, (struct sockaddr*)&serv_addr, len) < 0) {
            printf("can't establish connection with the server\n");
            close(tcp); close(udp);
            exit(1);
        }
        printf("connected!\n");

        //---------------------------------------------------------
        struct task mytask;
        printf("<Waiting for task>...");
        
        if (read(tcp, &mytask, sizeof(mytask)) < sizeof(mytask)) {
            printf("Can't recieve task from the server!\n");
            close(tcp); close(udp);
            exit(1);
        }
        printf("received!\n");
        double res = -1;

        printf("<Working>...");
        routine(&res, mytask);
        printf("completed!\n""<Sending result to the server>\n\n");
        
        if ((write(tcp, (void*)&res, sizeof(res))) < sizeof(res)) {
            printf("Can't send result to the server\n");
            close(tcp); close(udp);
            exit(1);
        }
        
        close(tcp); close(udp);

    return 0;
}

int routine(double* res, struct task mytask)
{
    double a = mytask.from;
    while (a < mytask.to) {
        *res += mytask.dx * sqrt(a);
        a += mytask.dx;
    }
    
    return 0;
}