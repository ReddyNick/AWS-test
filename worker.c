#include <header.h>

int routine(double* res, struct task mytask);

int main()
{
    struct sockaddr_in worker_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT_worker),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };

    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    int turn_on = 1;
    setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
    bind(udp, (struct sockaddr*)& worker_addr, sizeof(worker_addr));
      
    struct sockaddr_in serv_addr;
    socklen_t len = sizeof(serv_addr);
      
    int tcp = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR, &turn_on, sizeof(int));
    bind(tcp, (struct sockaddr*)& worker_addr, sizeof(worker_addr));

    // Waiting for server broadcast
    printf("<Listening broadcast>...");
    char buf[100];
    if (recvfrom(udp, buf, strlen(buf), 0, (struct sockaddr*)&serv_addr, &len) < 0) {
        printf("recfrom\n");
        close(tcp); close(udp);
        exit(1);
    }
    printf("recieved broadcast!\n");
    
    // making tcp connection
    printf("<Trying to connect the the server>...");
    if (connect(tcp, (struct sockaddr*)&serv_addr, len) < 0) {
        printf("can't establish connection with the server\n");
        close(tcp); close(udp);
        exit(1);
    }
    printf("connected!\n");
    
    // receiving task
    struct task mytask;
    int size_task = sizeof(mytask);
    printf("<Waiting for task>...");
    
    if (read(tcp, &mytask, sizeof(mytask)) < size_task) {
        printf("Can't recieve task from the server!\n");
        close(tcp); close(udp);
        exit(1);
    }

    printf("received!\n");

    //start caclulation
    double res = -1;
    int size_res = sizeof(res);
    printf("<Working>...");
    routine(&res, mytask);


    //sending task to the server
    printf("completed!\n""<Sending result to the server>\n\n");
    if ((write(tcp, (void*)&res, sizeof(res))) < size_res) {
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