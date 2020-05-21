#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

void main(int argc, char** argv)
{
    int sockfd;
    int n;
    char sendline[1000], recvline[1000];

    struct sockaddr_in servaddr;
    if (argc != 2){
        printf("no argumets!\n");
        exit(1);
    }
    bzero(sendline, 1000);
    bzero(recvline, 1000);

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror(NULL);
        close(sockfd);
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(51000);
    if (inet_aton(argv[1], &servaddr.sin_addr) == 0){
        printf("Invalid IP\n");
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr*)& servaddr, sizeof(servaddr)) < 0){
        perror(NULL);
        close(sockfd);
        exit(1);
    }
    

    for (int i = 0; i < 3; i++){
        printf("String => ");
        fflush(stdin);
        fgets(sendline, 1000, stdin);
        if ((n = write(sockfd, sendline, strlen(sendline) + 1)) < 0){
            perror("Cant write\n");
            close(sockfd);
            exit(1);
        }

        if ((n = read(sockfd, recvline, 999)) < 0){
            perror("Cant read\n");
            close(sockfd);
            exit(1);
        }

        printf("%s", recvline);
    }
    close(sockfd);
}