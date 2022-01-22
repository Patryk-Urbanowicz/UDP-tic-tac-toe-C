#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>

#define MSG_LEN 512
#define n 8765

int flagMyMove = 0;

int main(int argc, char* argv[]) {
    int mySockfd, opSockfd;
    struct sockaddr_in myAddress, opponentAddress;
    char recBuffer[MSG_LEN], sndBuffer[MSG_LEN];
    int childPid;

    if ((mySockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket() failed\n");
        exit(-1);
    }
    printf("[Socket created]\n");

    memset(&myAddress, 0, sizeof(myAddress));
    memset(&opponentAddress, 0, sizeof(opponentAddress));

    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    myAddress.sin_port = htons(n);

    opponentAddress.sin_family = AF_INET;
    opponentAddress.sin_addr.s_addr = inet_addr(argv[1]);
    opponentAddress.sin_port = htons(n);

    if (bind(mySockfd, (struct sockaddr*) &myAddress, sizeof(myAddress)) < 0) {
        printf("bind() failed\n");
        exit(-1);
    }
    printf("[bind completed]\n");

    opSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    socklen_t len = sizeof(opponentAddress);

    if ((childPid = fork()) == 0) {
        while(1) {
        printf("[Child waiting for a message]\n");
        int recLen = recvfrom(mySockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
        recBuffer[recLen] = '\0';
        printf("[Child: Received message]\n");
        printf("message: %s", recBuffer);
        }
        
        exit(0);
    } else {
        while(1) {
        printf("Give me message:\n");
        fgets(sndBuffer, MSG_LEN, stdin);
        printf("[Sending message]\n");
        sendto(opSockfd, sndBuffer, strlen(sndBuffer), 0, (struct sockaddr*) &opponentAddress, len);
        }
        
    }
    close(mySockfd);
    return 0;
}