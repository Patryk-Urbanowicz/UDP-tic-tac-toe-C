#include <signal.h>
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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netdb.h>

#define MSG_LEN 512
#define n 8777
#define nString "8777"
#define PLAYREQ "PLAYREQ"
#define PLAYACC "PLAYOK"
#define PLAYEND "PLAYEND"
#define ENDGAME "<koniec>"
#define GETSCORE "<wynik>" 
#define NICKLEN 50
#define MY_TURN 1
#define OP_TURN 0
#define KEY 12347

//stores neccesary information about game session
struct gameInfo {
    int myPoints;
    int opPoints;
    int myTurn; //If set to 1 it's player's turn, if 0 it's opponent's turn
    char game[3][3];
    char myNick[NICKLEN];
    char opNick[NICKLEN];
    char *opAddress;
    char mySymbol;
    char opSymbol;
    int gameActive;
} *myGame;

char recBuffer[MSG_LEN], sndBuffer[MSG_LEN];
int childPid;
struct sockaddr_in myAddress, opponentAddress;
socklen_t len;
int sockfd;
int shmid;

//provides communication required in the start of the game and fills myGame with required info
void setupGameSession(int sockfd, struct sockaddr_in opponentAddress, socklen_t len);

//prints the playing field
void printGame();
//used whenever users sends something in stdin
void inputHandler(char *input);
//used by inputHandler() when receiving GETSCORE 
void printScore();
//used by inputHandler() when receiving ENDGAME
void finishGame();
//used to finish the process with proper cleanup
void cleanExit(int code);
//used to see if game is won
int checkWin(char symbol);
//resets playing field
void setGameField();
//handle incoming messages
void recHandler(char *rec);
//Used to set opponent's address as required
void setAddress(char *info);

int main(int argc, char* argv[]) {
    char opIpAddr[20];

    myGame = malloc(sizeof(int)*3 + sizeof(char)*11 + sizeof(char*) * 3 * NICKLEN);
    myGame->gameActive = 0;

    if((shmid = shmget(KEY, sizeof(int)*3 + sizeof(char)*11 + sizeof(char*) * 3 * NICKLEN, 0644 | IPC_CREAT)) == -1) {
        printf("shmid error\n");
        exit(0);
    }

    if ((myGame = (struct gameInfo *) shmat(shmid, NULL, 0)) == (void *)-1) {
        printf("shmat error\n");
        exit(0);
    }

    //check arguments and set up nick and address
    if (argc < 2) {
        printf("Usage: program address [nickname]\n");
        exit(-1);
    }

    //setup player's nickname and get opponent address (support for domain and IP)
    strncpy(myGame->myNick, (argc == 3 ? argv[2] : "NN"), NICKLEN);
    strncpy(opIpAddr, argv[1], 20); //TODO: domain addresses support

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket() failed\n");
        exit(-1);
    }

    bzero(&myAddress, sizeof(struct sockaddr_in));
    bzero(&opponentAddress, sizeof(struct sockaddr_in));


    //---Implementing addrinfo
    struct addrinfo hints, *info;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int errCode;
    if ((errCode = getaddrinfo(argv[1], nString, &hints, &info)) != 0) {
        printf("%s\n",gai_strerror(errCode));
        exit(-1);
    }

    opponentAddress = *((struct sockaddr_in*) info->ai_addr);
    //---
    myAddress.sin_family = AF_INET;
    myAddress.sin_addr.s_addr = INADDR_ANY;
    myAddress.sin_port = htons(n);


    if (bind(sockfd, (struct sockaddr*) &myAddress, sizeof(myAddress)) < 0) {
        printf("bind() failed\n");
        exit(-1);
    }
    
    len = sizeof(opponentAddress);
    
    
    //connecting
    printf("Rozpoczynam gre z %s. Napisz <koniec> aby zakonczyc lub <wynik> by wyswietlic aktualny wynik gry\n", inet_ntoa(opponentAddress.sin_addr));
    setupGameSession(sockfd, opponentAddress, len);
    

    //test
    // printf("myPoints: %d\n", myGame->myPoints);
    // printf("opPoints: %d\n", myGame->opPoints);
    // printf("myNick: %s\n", myGame->myNick);
    // printf("opNick: %s\n", myGame->opNick);
    // printf("myTurn: %d\n", myGame->myTurn);
    if(myGame->myTurn == MY_TURN) printGame();

    signal(SIGCHLD, NULL);
    //child process receives messages
    //main process sends them
    if ((childPid = fork()) == 0) {
        while(1) {
        if (myGame->gameActive == 0) {
            setupGameSession(sockfd, opponentAddress, len);
            if (myGame->myTurn == MY_TURN) {
                printGame();
                printf("[wybierz pole] ");
                fflush(stdout);
            }
        }
        int recLen = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
        recBuffer[recLen] = '\0';
        recHandler(recBuffer);
        }
    } else {
        if (myGame->myTurn == MY_TURN) printf("[Wybierz pole] ");
        while(1) {
        fgets(sndBuffer, MSG_LEN, stdin);
        inputHandler(sndBuffer);
        }
    }
    close(sockfd);
    close(sockfd);
    return 0;
}

void setupGameSession(int sockfd, struct sockaddr_in opponentAddress, socklen_t len) {
    int tmp;
    //Sends play request and awaits for answer
    //printf("Wysylanie propozycji gry...\n");
    sendto(sockfd, PLAYREQ, strlen(PLAYREQ), 0, (struct sockaddr*) &opponentAddress, len);
    //printf("[Propozycja gry wysłana]\n");
    tmp = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
    //printf("Dostalem odpowiedz\n");
    recBuffer[tmp] = '\0';
    //define who is moving first and what nicknames are
    //after receiving PLAYACC send nickname and wait for opponent's one
    //fill myGame with needed information depending on whether you are opponents are starting the game
    if (strcmp(recBuffer, PLAYREQ) == 0) {
        usleep(200);
        sendto(sockfd, PLAYACC, strlen(PLAYACC), 0, (struct sockaddr*) &opponentAddress, len); //sends it on different port - WHY
        tmp = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len); 
        recBuffer[tmp] = '\0';
        strcpy(myGame->opNick, recBuffer);
        sendto(sockfd, myGame->myNick, strlen(myGame->myNick), 0, (struct sockaddr*) &opponentAddress, len);
        myGame->myTurn = MY_TURN;
        myGame->mySymbol = 'O';
        myGame->opSymbol = 'X';
    } else if (strcmp(recBuffer, PLAYACC) == 0) {
        sendto(sockfd, myGame->myNick, strlen(myGame->myNick), 0, (struct sockaddr*) &opponentAddress, len);
        tmp = recvfrom(sockfd, recBuffer, MSG_LEN, 0, (struct sockaddr*) &opponentAddress, &len);
        recBuffer[tmp] = '\0';
        strcpy(myGame->opNick, recBuffer);
        myGame->myTurn = OP_TURN;
        myGame->mySymbol = 'X';
        myGame->opSymbol = 'O';
    } else {
        printf("mamy problem\n");
        exit(0);
    }

    myGame->opAddress = inet_ntoa(opponentAddress.sin_addr);
    printf("[%s (%s) dolaczyl do gry]\n", myGame->opNick, myGame->opAddress);
    setGameField();
    myGame->myPoints = 0;
    myGame->opPoints = 0;
    myGame->gameActive = 1;
}

void printGame() {
    for (int row = 0; row < 3; row++) {
        printf("%c", myGame->game[row][0]);
        printf("|%c|", myGame->game[row][1]);
        printf("%c\n", myGame->game[row][2]);
    }
}

void inputHandler(char *input) {
    input[strlen(input) - 1] = '\0'; //get rid of new line

    if (strcmp(input, GETSCORE) == 0) {
        printScore();
        return;
    }
    if (strcmp(input, ENDGAME) == 0) {
        finishGame();
        return;
    }

    if (myGame->gameActive == 0) {
        printf("[Poczekaj na dolaczenie nowego gracza]\n");
        return;
    }

    if (strlen(input) > 1 || input[0] < 'a' || input[0] > 'i') {
        printf("[Niepoprawana komenda]\n");
        return;
    }
    
    //move requested
    if (myGame->myTurn == OP_TURN) {
        printf("[Teraz kolej drugiego gracza, poczekaj na swoja kolej]\n");
        return;
    }

    //check if move is legal
    int helper = input[0] - 97;
    int row = (helper) / 3;
    int column = helper % 3;
    if (myGame->game[row][column] == myGame->mySymbol || myGame->game[row][column] == myGame->opSymbol) {
        printf("[Tego pola nie mozesz wybrac, wybierz inne pole] ");
        return;
    }

    //make a move
    myGame->game[row][column] = myGame->mySymbol;
    myGame->myTurn = OP_TURN;
    char tmp[2];
    tmp[0] = input[0];
    tmp[1] = '\0';
    sendto(sockfd, tmp, 2, 0, (struct sockaddr*) &opponentAddress, len);

    if(checkWin(myGame->mySymbol)) {
        myGame->myPoints++;
        printf("[Wygrana! Kolejna rozgrywka, poczekaj na swoja kolej]\n");
        setGameField();
    }
}

void printScore() {
    printf("Ty %d : %d %s\n", myGame->myPoints, myGame->opPoints, myGame->opNick);
}

void finishGame() {
    sendto(sockfd, PLAYEND, strlen(PLAYEND), 0, (struct sockaddr*) &opponentAddress, len);
    cleanExit(0);
}

void cleanExit(int code) {
    kill(childPid, SIGKILL);
    shmdt(myGame);
    shmctl(shmid, IPC_RMID, 0);
    exit(code);
}

int checkWin(char symbol) {
    int flag = 1; 

    //check rows
    for (int row = 0; row < 3; row++) {
        flag = 1;
        for (int column = 0; column < 3; column++) {
            if (myGame->game[row][column] != symbol) flag = 0; 
        }
        if (flag) return 1;
    }

    //check columns
    for (int column = 0; column < 3; column++) {
        flag = 1;
        for(int row = 0; row < 3; row++) {
            if (myGame->game[row][column] != symbol) flag = 0;
        }
        if (flag) return 1;
    }

    //check diagonals
    if (myGame->game[0][0] == symbol && myGame->game[1][1] == symbol && myGame->game[2][2] == symbol) return 1;
    if (myGame->game[0][2] == symbol && myGame->game[1][1] == symbol && myGame->game[2][0] == symbol) return 1;

    return 0;
}

void setGameField() {
    char tmp = 'a';
    for (int row = 0; row < 3; row++) 
        for (int column = 0; column < 3; column++)
            myGame->game[row][column] = tmp++;
}

void recHandler(char *rec) {

    if (strlen(rec) == 1) {
        int helper = rec[0] - 97;
        int row = helper / 3;
        int column = helper % 3;
        myGame->game[row][column] = myGame->opSymbol;
        printf("[%s (%s) wybral pole %c]\n",myGame->opNick, myGame->opAddress, rec[0]);
        printGame();
        if (checkWin(myGame->opSymbol)) {
            printf("[Przegrana! Zagraj jeszcze raz]\n");
            myGame->opPoints++;
            setGameField();
            printGame();
        }
        myGame->myTurn = MY_TURN;
        printf("[wybierz pole] ");
        fflush(stdout);
        return;
    } 

        printf("[%s (%s) zakonczyl gre, mozesz poczekac na kolejnego gracza]\n", myGame->opNick, myGame->opAddress);
        fflush(stdout);
        myGame->gameActive = 0;
}