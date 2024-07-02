#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include  <signal.h>

#define BUFFLEN 2500
int serverSocket;

typedef struct Network {
    int idSiete;
    int socketLeft;
    int socketRight;
} NET;

typedef struct Uzol {
    int serverSocket;
    int nodeSocket;
    NET* siete;
} UZOL;

char *itoa(int val, int base) {
    static char buf[32] = {0};
    int i = 30;
    for (; val && i; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];
    return &buf[i + 1];
}
int pripojenieKUzlu(char* info) {
    char ip[INET_ADDRSTRLEN];
    bzero(ip,INET_ADDRSTRLEN);
    char port[10];
    bzero(port,10);
    int socketForConnection;
    int ip_t = info[2];

    int idsiete = info[1] - 1;
    strncpy(ip, &info[4], info[2]);
    strncpy(port, &info[4 + ip_t], info[3]);
    struct sockaddr_in serv_addr;
    struct hostent *server;
    server = gethostbyname(ip);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(
            (char *) server->h_addr,
            (char *) &serv_addr.sin_addr.s_addr,
            server->h_length
    );
    serv_addr.sin_port = htons(atoi(port));
    socketForConnection = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(socketForConnection, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting to socket");
        return -1;
    }
    char buffer[BUFFLEN];
    bzero(buffer, BUFFLEN);
    buffer[0] = idsiete + 1;
    write(socketForConnection, buffer, BUFFLEN);
    return socketForConnection;
}



void *preposielanie(void *param) {
    sleep(1);
    NET *mojaSiet = (NET *) param;
    int id = mojaSiet->idSiete;
    printf("Siet[%d] bola vytvorená socketL[%d] socketR[%d]\n",id,mojaSiet->socketLeft,mojaSiet->socketRight);
    int Lsocket = mojaSiet->socketLeft;
    int Rsocket = mojaSiet->socketRight;




    char buffer[BUFFLEN];
    bzero(buffer, BUFFLEN);
    fd_set socket1;
    fd_set socket2;
    FD_ZERO(&socket1);
    FD_ZERO(&socket2);
    while (1) {
        FD_SET(Lsocket, &socket1);
        FD_SET(Rsocket, &socket2);
        select(Lsocket + 1, &socket1, NULL, NULL, NULL);
        if (FD_ISSET(Lsocket, &socket1)) {
            if (read(Lsocket, buffer, BUFFLEN) <= 0) {
                break;
            }
            //if (buffer[0] != 0) {
                printf("Dostal som správu zlava.|%s|\n", buffer);
                if (write(Rsocket, buffer, BUFFLEN) <= 0) {
                    break;
                }
                printf("Preposielam správu doprava.|%s|\n", buffer);
                bzero(buffer, BUFFLEN);
            //}
        }
        select(Rsocket + 1, &socket2, NULL, NULL, NULL);
        if (FD_ISSET(Rsocket, &socket2)) {
            if (read(Rsocket, buffer, BUFFLEN) <= 0) {
                break;
            }
            printf("Dostal som správu zprava.|%s|\n", buffer);
            if (write(Lsocket, buffer, BUFFLEN) <= 0) {
                break;
            }
            printf("Preposielam správu doľava.|%s|\n", buffer);
            bzero(buffer, BUFFLEN);
        }
    }

    printf("Sieť[%d]:Koniec vlákna\n", id);
    close(Lsocket);
    close(Rsocket);
    printf("Sieť[%d]:Vlákno ukončene\n", id);
    return NULL;
}


void *koncovyUzol(void *param) {
    NET *mojaSiet = (NET *) param;
    sleep(1);//mutex
    char buffer[BUFFLEN];
    fd_set fdsck;
    FD_ZERO(&fdsck);
    int id = mojaSiet->idSiete;
    int socketfwd = mojaSiet->socketLeft;

    while (1) {
        bzero(buffer, BUFFLEN);
        FD_SET(socketfwd, &fdsck);
        select(socketfwd + 1, &fdsck, NULL, NULL, NULL);

        if (FD_ISSET(socketfwd, &fdsck)) {
            if (read(socketfwd, buffer, BUFFLEN) <= 0) {
                break;
            }
            //if (buffer[0] != 0) {
                printf("Sieť[%d]: %d Dostal som správu %s\n", id, buffer[0], buffer);


                /*
                char web_address[BUFFLEN] = "www.google.com";
                strcpy(web_address,buffer);
                bzero(buffer,BUFFLEN);
                snprintf(buffer, sizeof(buffer), "wget %s", web_address);
                system(buffer);
                bzero(buffer,BUFFLEN);
                //TODO poslat subor nas5 pomocou buffra
                strcpy(buffer, "stranka bola stiahnuta");

                 */

                if (buffer[0] == 0) {
                    bzero(buffer, BUFFLEN);
                    strcpy(buffer, "Pripojený do sieťe.");
                }
                else {
                    bzero(buffer, BUFFLEN);
                    strcpy(buffer, "chytaj spravicku");
                }
                if (write(socketfwd, buffer, BUFFLEN) <= 0) {
                    break;
                }
                printf("Sieť[%d]: Poslaná správa: %s\n", id, buffer);
                bzero(buffer, BUFFLEN);
            }
        //}
    }
    printf("Sieť[%d]:Koniec vlákna\n", id);
    close(socketfwd);
    printf("Sieť[%d]:Vlákno ukončene\n", id);
}


void *uzolServer(void *param) {
    pthread_t network;
    pthread_t out;
    UZOL *node = (UZOL *) param;
    char serverMSG[256];
    char consoleInput[256];
    bzero(serverMSG, 256);
    bzero(consoleInput, 256);

    fd_set socket1;
    FD_ZERO(&socket1);
    while (1) {
        FD_SET(serverSocket, &socket1);
        FD_SET(STDIN_FILENO, &socket1);
        select(serverSocket + 1, &socket1, NULL, NULL, NULL);
        if (FD_ISSET(serverSocket, &socket1)) {
            if (read(serverSocket, serverMSG, BUFFLEN) <= 0) {
                break;
            }
            printf("%s", serverMSG);
            int id = serverMSG[1] - 1;
            printf("Spracovavám požiadavku zo servera id siete je (%d)\n",id);
            switch (serverMSG[0]) {
                case 10 :
                    node->siete[id].idSiete = id;
                    node->siete[id].socketRight = pripojenieKUzlu(serverMSG);
                    pthread_create(&network, NULL, preposielanie, &node->siete[id]);
                    break;
                case 11 :
                    node->siete[id].idSiete = id;
                    pthread_create(&out, NULL, koncovyUzol, &node->siete[id]);
                    break;
                case 19 :
                    close(node->siete[id].socketLeft);
                    break;
            }
        }

        if (FD_ISSET(STDIN_FILENO, &socket1)) {
            bzero(consoleInput, 256);
            fgets(consoleInput, 256, stdin);
            printf("%s",consoleInput);
            if (strcmp(consoleInput, ":exit\n") == 0 || consoleInput[0] == 'x') {
                close(node->serverSocket);
                /*for (int i = 0; i < 100; i++) {
                    if (node->siete[i].socketLeft != 0) {
                        close(node->siete[i].socketLeft);
                    }
                    if (node->siete[i].socketRight != 0) {
                        close(node->siete[i].socketRight);
                    }
                }*/
                printf("nodesock:%d\n",node->nodeSocket);
                close(node->nodeSocket);
                pthread_cancel(out);
                pthread_detach(out);
                pthread_cancel(network);
                pthread_detach(network);
                return NULL;
            }
            bzero(consoleInput, 256);
        }
    }

    //pthread_join(network,NULL);
    //pthread_join(out,NULL);

    printf("ukoncene spojenie uzol server\n");
}


void *klientUzol(void *param) {
    char *info = (char *) param;
    int nodeSocket = pripojenieKUzlu(info);
    char buffer[BUFFLEN];
    bzero(buffer, BUFFLEN);
    fd_set fds;
    FD_ZERO(&fds);
    while (1) {
        FD_SET(nodeSocket, &fds);
        FD_SET(STDIN_FILENO, &fds);
        select(nodeSocket + 1, &fds, NULL, NULL, NULL);
        if (FD_ISSET(nodeSocket, &fds)) {
            bzero(buffer, 256);
            if (read(nodeSocket, buffer, BUFFLEN) <= 0) {
                break;
            }
            printf("Dostal som správu|%s|\n", buffer);
        }
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            bzero(buffer, BUFFLEN);
            fgets(buffer, BUFFLEN, stdin);
            if (strcmp(buffer, ":exit\n") == 0) {
                close(serverSocket);
                break;
            }
            if (buffer[strlen(buffer) - 1] == '\n')
                buffer[strlen(buffer) - 1] = 0;
            if (write(nodeSocket, buffer, strlen(buffer)) <= 0) {
                break;
            }
            printf("Odosielam request:|%s|\n", buffer);
        }
    }
    close(nodeSocket);

    printf("%d",serverSocket);
    printf("Koniec vlákna Klient-Uzol\n");
    return NULL;
}

pthread_t serverT;
void clientExit(int sign){
    pthread_join(serverT,NULL);
    printf("koniec285\n");
    sleep(1);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    NET siete[100];

    char buff[256];
    bzero(buff, 256);
    struct sockaddr_in serv_addr;
    struct hostent *server;



    if (argc < 3) {
        printf("Nedostatočný počet argumentov\n");
        return 100;
    }
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        printf("Neplatná ip adresa servera\n");
        return 200;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(argv[2]));
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        printf("Nepodarilo sa vytvoriť socket pre spojenie so serverom.\n");
        return 300;
    }
    if (argc > 4 && strcmp(argv[3], "node") == 0) {
        int nodeSock, pripojeny, nodePort;
        UZOL tentoUzol;
        nodePort = atoi(argv[4]);
        struct sockaddr_in peer;
        struct sockaddr_in serv;
        socklen_t socksize = sizeof(struct sockaddr_in);





        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = htonl(INADDR_ANY);
        serv.sin_port = htons(nodePort);
        nodeSock = socket(AF_INET, SOCK_STREAM, 0);
        if (nodeSock < 0) {
            printf("");
            return 500;
        }

        if (bind(nodeSock, (struct sockaddr *) &serv, sizeof(struct sockaddr)) < 0) {
            printf("");
            return 600;
        }
        if (listen(nodeSock, 1) < 0) {
            printf("Problem pri listeningu\n");
            return 700;
        }

        if (connect(serverSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            printf("Nepodarilo sa pripojit na server.\n");
            return 800;
        }
        bzero(buff,256);
        buff[0] = 2;
        char *portString = itoa(nodePort, 10);
        strcpy(&buff[1], portString);
        write(serverSocket, buff, strlen(buff) + 1);

        tentoUzol.nodeSocket = nodeSock;
        tentoUzol.serverSocket = serverSocket;
        tentoUzol.siete = siete;
        pthread_create(&serverT, NULL, uzolServer, &tentoUzol);

        printf("nodesocket %d\n", nodeSock);
        printf("serversocket %d\n", serverSocket);
        signal(SIGINT, clientExit);
        pripojeny = accept(nodeSock, (struct sockaddr *) &peer, &socksize);
        while (pripojeny > 0) {
            bzero(buff, 256);
            read(pripojeny, buff, 256);
            int id = buff[0] - 1;
            printf("id siete: %d\n", id);
            siete[id].socketLeft = pripojeny;

            printf("Pripojil sa host so socketom %d|%d\n", pripojeny, siete[id].socketLeft);
            pripojeny = accept(nodeSock, (struct sockaddr *) &peer, &socksize);
        }
        pthread_join(serverT,NULL);
        printf("koniec285\n");
        close(nodeSock);
    } else {
        if (connect(serverSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            printf("Nepodarilo sa pripojit na server.\n");
            return 900;
        }


        char hops = (char) atoi(argv[4]);
        char serverBuffer[256];
        bzero(serverBuffer, 256);
        serverBuffer[0] = 1;
        serverBuffer[1] = hops;
        printf("pocet uzlov %d\n", hops);



        write(serverSocket, serverBuffer, strlen(serverBuffer) + 1);
        bzero(serverBuffer, 256);
        pthread_t nodeT;
        int r = 0;
        do {
            r = read(serverSocket, serverBuffer, 256);
            if (r > 0) {
                switch (serverBuffer[0]) {
                    case 100 :
                        pthread_create(&nodeT, NULL, klientUzol, &serverBuffer);
                        pthread_join(nodeT, NULL);
                        bzero(serverBuffer,256);
                        r = read(serverSocket, serverBuffer, 256);
                        break;
                    case 109 :
                        close(serverSocket);
                        break;
                }
            }
        } while (r > 0);
        printf("Ukončujem klienta\n");

    }
    close(serverSocket);
    printf("THE END\n");
    return 0;
}