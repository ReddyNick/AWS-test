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

#define check_neg(x, str) do { \
                            if ((x) < 0) {\
                                perror(str);\
                                exit(1); } \
                        } while(0)


const int PORT = 51001;
const int PORT_worker = 51000;
#define MAX_WORKERS 20
const char* broadcast_msg = "WORK!";
int workers[MAX_WORKERS];

int parse_num(int argc, char* argv[]);
int make_broadcast(struct sockaddr_in serv_addr);
int accept_tcp(int listenfd);
void* routine(void* arg);

struct task {
    int worker_id;
    double from, to, dx;
};

pthread_mutex_t mutexsum;

const double START = 0; 
const double END = 2500;
const double dx = 1e-6;
double SUM = 0;

int main(int argc, char* argv[])
{
    int num_workers = 0;
    if ((num_workers = parse_num(argc, argv)) < 0){
        perror("Invalid argument!\n");
        exit(1);
    }
    if (num_workers > MAX_WORKERS){
         perror("too many workers!\n");
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
    printf("<BROADCASTIN>...");
    if (make_broadcast(serv_addr) < 0){
        exit(1);
    }
    printf("completed!\n");

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int turn_on = 1;
    int ret3 = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
    assert(ret3 == 0);


    /* Set a socket as nonblocking */
    int flags; 
    if ((flags = fcntl(listenfd, F_GETFL, 0)) < 0){
        perror("F_GETFL error\n");
        close(listenfd);
        exit(1);
    }    
    flags |= O_NONBLOCK;
    if (fcntl(listenfd, F_SETFL, flags) < 0){
        perror("F_SETFL error\n");
        close(listenfd);
        exit(1);
    }

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind error for tcp");
        close(listenfd);
        exit(1);
    }

    if (listen(listenfd, MAX_WORKERS) < 0) {
        perror("Server listen error");
        close(listenfd);
        exit(1);
    }

    int nready = 0;
    bzero(workers, sizeof(int) * MAX_WORKERS);
    fd_set set;
    int connfd;

    FD_ZERO(&set);
    FD_SET(listenfd, &set);
    struct timeval t = {
         .tv_sec = 7,
         .tv_usec = 0
       };

    printf("<Listening workers>...\n");
    int dicsovery = 1;

    while (nready < num_workers) {

        int ret = select(listenfd + 1, &set, NULL, NULL, &t);
        if (ret < 0){
            perror("Select ERROR\n");
            dicsovery = 0;
            break;
        }
        else if (ret == 0) {
            perror("Time limit exceeded\n");
            dicsovery = 0;
            break;
        }

        
        if ((connfd = accept_tcp(listenfd)) < 0) {
            perror("cant accept worker, waiting for another one\n");
            continue;
        }        

        workers[nready] = connfd; 
        nready++;

        printf("<Connected to %d/%d workers>...\n", nready, num_workers);
    }
    if (!dicsovery) {
        fprintf(stderr, "Can't connected to %d workers\n", num_workers);
        close(listenfd);
        for (int i = 0; i < nready; i++)
            close(workers[i]);
        exit(1);
    }

    printf("<Connection completed!>\n");
    //DISTRIBUTE TASKS
    printf("<Start distributing tasks>...\n");

    struct task* tasks = (struct task*)calloc(sizeof(struct task), num_workers);
    pthread_t* threads = (pthread_t*)calloc(sizeof(pthread_t), num_workers);
    if (threads == NULL) {
        printf("Threads create error\n");
        exit(1);
    }
    pthread_attr_t attr;
    pthread_mutex_init(&mutexsum, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
   
    double per_worker = (END - START) / (double) num_workers;
    int ret_code;
    for (int i = 0; i < num_workers; i++) {
        tasks[i].worker_id = i;
        tasks[i].from = START + i*per_worker;
        tasks[i].to = START + (i+1)*per_worker;
        tasks[i].dx = dx;

        ret_code = pthread_create(&threads[i], &attr, routine, (void *)&tasks[i]); 
        
        if (ret_code){
            printf("ERROR; return code from pthread_create() is %d\n", ret_code);
            exit(-1);
        }
    }

    pthread_attr_destroy(&attr);
    void* status;
    
    for (long int i = 0; i < num_workers; i++) {
        ret_code = pthread_join(threads[i], &status);
        if (ret_code){
            printf("ERROR; return code from pthread_join() is %d\n", ret_code);
            exit(-1);
        }
    }

    
    printf("\nRESULT = %lg\n", SUM);

    pthread_mutex_destroy(&mutexsum);
    free(threads);
    free(tasks);
   
    
    close(listenfd);
    return 0;   
}

void* routine(void* task)
{   
    int id = ((struct task*)task)->worker_id;
    int fd = workers[id];
    
    printf("<Sending task to worker_%d>\n", id);
    if (write(fd, task, sizeof(struct task)) < sizeof(struct task)) {
        printf("Cannot send task to worker_%d\n", id);
        close(workers[id]);
        exit(1);
    }

    printf("<Waiting results from worker_%d>\n", id);
    double res = -1;

    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int retval;
    retval = recvfrom(fd, &res, sizeof(res), 0, NULL, NULL);
    int double_size = 8;
    if (retval < double_size) {
        printf("Cannot recieve answer from worker_%d\n", id);
        close(workers[id]);
        exit(1);
    }

    printf("Received results from worker_%d>\n", id);
    pthread_mutex_lock (&mutexsum);
    SUM += res;
    pthread_mutex_unlock (&mutexsum);

    close(workers[id]);
    return 0;
}

int accept_tcp(int listenfd)
{   
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd  < 0) {
            perror("Accept error\n");
            return -1;
    }
    
    int turn_option_on  = 1,
        cnt             = 5,
        idle            = 5,
        intvl           = 1;
    
    int ret = -1;
    ret = setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &turn_option_on, sizeof(turn_option_on));
    assert(ret == 0);
	ret = setsockopt(connfd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
	assert(ret == 0);
    ret = setsockopt(connfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
	assert(ret == 0);
    ret = setsockopt(connfd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    assert(ret == 0);

    socklen_t optlen = sizeof(turn_option_on);

    return connfd;
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
		perror("Bind error in broadcating\n");
        close(broadcastfd);
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
        close(broadcastfd);
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