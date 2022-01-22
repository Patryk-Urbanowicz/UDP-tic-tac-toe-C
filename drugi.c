#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSG_LEN 512
#define n 8765
#define PLAYREQ "PLAYREQ"
#define PLAYACC "PLAYOK"
#define ENDGAME "<koniec>"
#define GETSCORE "<wynik>" 
#define NICKLEN 50
#define MY_TURN 1
#define OP_TURN 0

struct gameInfo {
    int myPoints;
    int opPoints;
    int myTurn;
    char game[3][3];
    char myNick[NICKLEN];
    char opNick[NICKLEN];
} *myGame;

char recBuffer[MSG_LEN], sndBuffer[MSG_LEN];

void createGameSession(int sockfd, struct sockaddr_in opponentAddress, socklen_t len) {
    int tmp;
    //Sends play request and awaits for answer
    printf("Wysylanie propozycji gry...\n");
    sendto(sockfd, PLAYREQ, strlen(PLAYREQ), 0, (struct sockaddr*) &opponentAddress, len);
    printf("[Propozycja gry wysłana]\n");
    tmp = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
    printf("Dostalem odpowiedz\n");
    recBuffer[tmp] = '\0';
    //define who is moving first and what nicknames are
    //after receiving PLAYACC send nickname and wait for opponent's one
    if (strcmp(recBuffer, PLAYREQ) == 0) {
        printf("dostalem PLAYREQ\n");
        usleep(200);
        sendto(sockfd, PLAYACC, strlen(PLAYACC), 0, (struct sockaddr*) &opponentAddress, len); //sends it on different port - WHY
        printf("Wyslalem PLAYACC\n");
        printf("Oczekuje na nick\n");
        tmp = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len); 
        printf("Dostalem nick\n");
        recBuffer[tmp] = '\0';
        strcpy(myGame->opNick, recBuffer);
        sendto(sockfd, myGame->myNick, strlen(myGame->myNick), 0, (struct sockaddr*) &opponentAddress, len);
        printf("Wyslalem nick");
        myGame->myTurn = MY_TURN;
    } else if (strcmp(recBuffer, PLAYACC) == 0) {
        printf("Dostalem PLAYACC\n");
        sendto(sockfd, myGame->myNick, strlen(myGame->myNick), 0, (struct sockaddr*) &opponentAddress, len);
        printf("Wyslalem nick\n");
        tmp = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
        printf("Dostalem nick\n");
        recBuffer[tmp] = '\0';
        strcpy(myGame->opNick, recBuffer);
        myGame->myTurn = OP_TURN;
    } else {
        printf("mamy problem\n");
    }
}

int main(int argc, char* argv[]) {
    int sockfd;
    struct in_addr naddr;
    struct sockaddr_in myAddress, opponentAddress;
    int childPid;
    char opIpAddr[20];

    myGame = malloc(sizeof(int)*3 + sizeof(char)*9 + sizeof(char*) * 2 * NICKLEN);

    //check arguments and set up nick and address
    if (argc < 2) {
        printf("Usage: program address [nickname]\n");
        exit(-1);
    }

    strncpy(myGame->myNick, (argc == 3 ? argv[2] : "NN"), NICKLEN);
    strncpy(opIpAddr, argv[1], 20); //TODO: domain addresses support

    myGame->myPoints = 0;
    myGame->opPoints = 0;
    myGame->myTurn = -1;
    memset(myGame->game, 0, sizeof(char)*9);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket() failed\n");
        exit(-1);
    }

    bzero(&myAddress, sizeof(struct sockaddr_in));
    bzero(&opponentAddress, sizeof(struct sockaddr_in));

    inet_pton(AF_INET, opIpAddr, &naddr);

    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = INADDR_ANY;
    myAddress.sin_port = htons(n);

    opponentAddress.sin_family = AF_INET;
    opponentAddress.sin_addr = naddr;
    opponentAddress.sin_port = htons(n);

    if (bind(sockfd, (struct sockaddr*) &myAddress, sizeof(myAddress)) < 0) {
        printf("bind() failed\n");
        exit(-1);
    }
    
    socklen_t len = sizeof(opponentAddress);

    printf("Rozpoczynam gre z %s. Napisz <koniec> aby zakonczyc lub <wynik> by wyswietlic aktualny wynik gry\n",opIpAddr);
    createGameSession(sockfd, opponentAddress, len);

    //test
    // printf("myPoints: %d\n", myGame->myPoints);
    // printf("opPoints: %d\n", myGame->opPoints);
    // printf("myNick: %s\n", myGame->myNick);
    // printf("opNick: %s\n", myGame->opNick);
    // printf("myTurn: %d\n", myGame->myTurn);

    //Proces potomny odpowiada za otrzymywanie wiadomości
    //Proces macierzysty odpowiada za wysyłanie wiadomości
    if ((childPid = fork()) == 0) {
        while(1) {
        int recLen = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
        recBuffer[recLen] = '\0';
        printf("message: %s", recBuffer);
        }
        exit(0);
    } else {
        while(1) {
        fgets(sndBuffer, MSG_LEN, stdin);
        sendto(sockfd, sndBuffer, strlen(sndBuffer), 0, (struct sockaddr*) &opponentAddress, len);
        }
    }
    close(sockfd);
    close(sockfd);
    return 0;
}