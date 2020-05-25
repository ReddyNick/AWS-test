#include <header.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>

void* routine(void* idx);
int parse_num(int argc, char* argv[]);
void* idle_routine(void* idx);

double START; 
double END;
double dx;
double SUM = 0;

pthread_mutex_t mutexsum;

double per_thread = 0;
int read_num_threads(int argc, char* argv[])
{
    int n_threads = 0;

    if (argc == 1) return 1;
    
    if (argc > 2){
        perror("Only one argument is required!\n");
        return -1;
    }

    char c = 0;
    int nsymbl = 0;
    nsymbl = sscanf(argv[1], "%d%c", &n_threads, &c);

    if (nsymbl != 1 || n_threads < 1){
        perror("Invalid argument!\n");
        return -1;
    }
    else 
        return n_threads;
}
int main(int argc, char* argv[])
{   
    int num_threads = 0;

    num_threads = read_num_threads(argc, argv);
    int n_cpus = get_nprocs(); 
    int n = n_cpus;

    if (num_threads > n_cpus) {
        printf("Warning!\n"
               "The number of threads is bigger then the number of available CPUs (%d).\n", n_cpus);
        
        n = num_threads;
    }
    
    if (num_threads < 0) exit(1);


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
    
    
    if (write(tcp, &num_threads, sizeof(int)) < 4) {
        printf("Cannot connect to server\n");
        close(tcp);
        exit(1);
    }

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

    //----------------------------
    START = mytask.from;
    END = mytask.to;
    dx = mytask.dx;


    pthread_t* threads = (pthread_t*)malloc(n * sizeof(pthread_t));
    if (threads == NULL) {
        printf("oops\n");
        exit(-1);
    }
    pthread_attr_t attr;
    
    void* status;
    int ret_code = 0;

    pthread_mutex_init(&mutexsum, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
   
    per_thread = (END - START) / (double) num_threads;
    
    for (long int i = 0; i < num_threads; i++) {
        ret_code = pthread_create(&threads[i], &attr, routine, (void *)i); 
        
        if (ret_code){
            printf("ERROR; return code from pthread_create() is %d\n", ret_code);
            exit(-1);
        }
    }

    for (long int i = num_threads; i < n; i++) {

        ret_code = pthread_create(&threads[i], &attr, idle_routine, (void *)i); 
        
        if (ret_code){
            printf("ERROR; return code from pthread_create() is %d\n", ret_code);
            exit(-1);
        }
    }
    
    pthread_attr_destroy(&attr);
    
    for (long int i = 0; i < num_threads; i++) {
        ret_code = pthread_join(threads[i], &status);
        if (ret_code){
            printf("ERROR; return code from pthread_join() is %d\n", ret_code);
            exit(-1);
        }
    }
    
    pthread_mutex_destroy(&mutexsum);
    free(threads);
    
    printf("%lg\n", SUM);

    //-------------------------------------

    //sending task to the server
    
    printf("completed!\n""<Sending result to the server>\n\n");
    if ((write(tcp, (void*)&SUM, sizeof(res))) < size_res) {
        printf("Can't send result to the server\n");
        close(tcp); close(udp);
        exit(1);
    }
    
    close(tcp); close(udp);
    return 0;
}


void* idle_routine(void* idx)
{
    for(;;);
    pthread_exit((void*)0);
}

void* routine(void* idx)
{
    double res = 0;
    double a = (START + (long int)idx * per_thread);
    double end = a + per_thread;

    while (a < end) {
        res += dx * sqrt(a);
        a += dx;
    }
    
    pthread_mutex_lock (&mutexsum);
    SUM += res;
    pthread_mutex_unlock (&mutexsum);

    pthread_exit((void*)0);
}

// int routine(double* res, struct task mytask)
// {
//     double a = mytask.from;
//     while (a < mytask.to) {
//         *res += mytask.dx * sqrt(a);
//         a += mytask.dx;
//     }
    
//     return 0;
// }
