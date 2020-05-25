#include <header.h>

const char* broadcast_msg = "WORK!";

#define MAX_WORKERS 20
int workers[MAX_WORKERS];
int work_thr[MAX_WORKERS];

const double START = 0; 
const double END = 3500;
const double dx = 1e-6;
double SUM = 0;
pthread_mutex_t mutexsum;

int parse_num(int argc, char* argv[]);
int make_broadcast(struct sockaddr_in serv_addr);
int accept_tcp(int listenfd);
void* routine(void* arg);

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
      .sin_port = htons(PORT_server),
      .sin_addr = htonl(INADDR_ANY)
    };

    /* BROADCAST PROCESS to discover available workers*/
    printf("<BROADCASTIN>...");
    if (make_broadcast(serv_addr) < 0){
        exit(1);
    }
    printf("completed!\n");

    /* Create and configure TCP socket */
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int turn_on = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
    assert(ret == 0);


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

    /** 
     *  Waiting for 'num_workers' number of available computers
     *  to be connected with the server
     */
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

        // configure tcp connection
        if ((connfd = accept_tcp(listenfd)) < 0) {
            perror("cant accept worker, waiting for another one\n");
            continue;
        }        


        workers[nready] = connfd; 
        
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int retval;

        retval = recvfrom(connfd, &work_thr[nready], sizeof(int), 0, NULL, NULL);
        int res_size = sizeof(int);
        
        if (retval < res_size) {
            printf("Cannot recieve answer from worker_%d\n", nready);
            break;
        }

        nready++;

        printf("<Connected to %d/%d workers>...\n", nready, num_workers);
    }

    /** 
     * if not enough computer are available then
     * stop work and exit
     */
    if (!dicsovery) {
        fprintf(stderr, "Can't connected to %d workers\n", num_workers);
        close(listenfd);
        for (int i = 0; i < nready; i++)
            close(workers[i]);
        exit(1);
    }

    close(listenfd);
    printf("<Connection completed!>\n");

    // DISTRIBUTE TASKS
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
    
    
    int dist = 0;
    for (int i = 0; i < num_workers; i++)
        dist += work_thr[i];


    double per_worker = (END - START) / (double) dist;
    int ret_code;
    int beg = START;
    for (int i = 0; i < num_workers; i++) {
        tasks[i].worker_id = i;
        tasks[i].from = beg;
        tasks[i].to = beg + work_thr[i]*per_worker;
        tasks[i].dx = dx;
        beg = beg + work_thr[i]*per_worker;

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
    return 0;   
}

void* routine(void* task)
{   
    int id = ((struct task*)task)->worker_id;
    int fd = workers[id];
    
    printf("<Sending task to worker_%d>\n", id);
    int size_task = sizeof(struct task);
    if (write(fd, task, sizeof(struct task)) < size_task) {
        printf("Cannot send task to worker_%d\n", id);
        close(workers[id]);
        pthread_exit((void*)0);
    }

    printf("<Waiting results from worker_%d>\n", id);
    double res = -1;

    struct timeval tv;
    tv.tv_sec = 18;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int retval;
    retval = recvfrom(fd, &res, sizeof(res), 0, NULL, NULL);
    int res_size = sizeof(res);
    if (retval < res_size) {
        printf("Cannot recieve answer from worker_%d\n", id);
        close(workers[id]);
        pthread_exit((void*)0);
    }

    printf("Received results from worker_%d>\n", id);
    pthread_mutex_lock (&mutexsum);
    SUM += res;
    pthread_mutex_unlock (&mutexsum);

    close(workers[id]);
    pthread_exit((void*)0);
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
    int ret = setsockopt(broadcastfd, SOL_SOCKET, SO_BROADCAST, &broadcast_on, sizeof(int));
    assert(ret == 0);
    ret = setsockopt(broadcastfd, SOL_SOCKET, SO_REUSEADDR, &broadcast_on, sizeof(int));
    assert(ret == 0);

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