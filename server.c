#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

#define _BSD_SOURCE

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define PORT 9000
#define MAX_PRIPOJENI 256
#define MAX_SIETI 100
#define BUFFER 256

typedef struct Connected {
    int jeUzol;
    int port;
    int socketID;
    int idPripojeneho;

    int *counter;

    int *data;
    pthread_mutex_t *mut;
    pthread_cond_t *spracuj;
} CONNECTED;

typedef struct Network {
    int idSiete;
    int idKlienta;
    int pocetUzlov;
    int *uzlySiete;
    CONNECTED *prip;

    int *counter;

    int *data;
    pthread_mutex_t *mut;
    pthread_cond_t *spracuj;
} NETW;

CONNECTED pripojeni[MAX_PRIPOJENI];
NETW siete[MAX_SIETI];
int pocetPripojeni = 0;
int pripojeneUzly = 0;
int pocetSieti = 0;
int masterSocket = -1;
int odpojil;

char *itoa(int val, int base) {
    static char buf[32] = {0};
    int i = 30;
    for (; val && i; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];
    return &buf[i + 1];
}

void getInfo(int id, char *dopln, CONNECTED *p) {
    char ip[INET_ADDRSTRLEN];
    char port[10];


    struct sockaddr_in peeraddr;
    socklen_t peeraddrlen = sizeof(peeraddr);
    getpeername(p[id].socketID, &peeraddr, &peeraddrlen);
    inet_ntop(AF_INET, &(peeraddr.sin_addr), ip, INET_ADDRSTRLEN);
    strcpy(port, itoa(p[id].port, 10));
    printf("IP: %s, Port: %s", ip, port);
    int portlen = strlen(port);
    int iplen = strlen(ip);
    dopln[strlen(dopln)] = iplen;
    dopln[strlen(dopln)] = portlen;
    strcpy(dopln + strlen(dopln), ip);
    strcpy(dopln + strlen(dopln), port);
}


void *network(void *param) {
    NETW *mojaSiet = (NETW *) param;
    int networkID = mojaSiet->idSiete;
    char buffer[BUFFER];
    printf("Sieť[%d] bola spustená, počet uzlov: %d\n", mojaSiet->idSiete, mojaSiet->pocetUzlov);


    bzero(buffer, BUFFER);
    buffer[0] = 100;
    buffer[1] = networkID + 1;

    getInfo(mojaSiet->uzlySiete[0], buffer, mojaSiet->prip);
    write(pripojeni[mojaSiet->idKlienta].socketID, buffer, BUFFER);
    printf("Sieť[%d]: Klient obdrzal spravu\n", networkID);

    for (int i = 0; i < mojaSiet->pocetUzlov; i++) {
        bzero(buffer, BUFFER);
        if (i + 1 != mojaSiet->pocetUzlov) {
            buffer[0] = 10;
            buffer[1] = networkID + 1;
            getInfo(mojaSiet->uzlySiete[i + 1], buffer, mojaSiet->prip);
            write(pripojeni[mojaSiet->uzlySiete[i]].socketID, buffer, BUFFER);
            printf("Sieť[%d]: Uzol %d obdrzal spravu\n", networkID, mojaSiet->uzlySiete[i]);
        } else {
            buffer[0] = 11;
            buffer[1] = networkID + 1;
            write(pripojeni[mojaSiet->uzlySiete[i]].socketID, buffer, BUFFER);
            printf("Sieť[%d]: Uzol %d obdrzal spravu o tom ze je koncovy\n", networkID, mojaSiet->uzlySiete[i]);
        }
    }
    int exit = 1;
    while (exit) {
        //pthread_mutex_lock(mojaSiet->mut);
        pthread_cond_wait(mojaSiet->spracuj, mojaSiet->mut); // čaká na požiadavku
        printf("Sieť[%d] informuje svojich clenov\n", networkID);
        int odpojeny = mojaSiet->data[0];

        if (odpojeny == -100) {
            printf("Sieť[%d] ukončila svoju činnosť z dôvodu -100\n", networkID);
            free(mojaSiet->uzlySiete);
            mojaSiet->uzlySiete = NULL;
            return NULL;
        }
        if (odpojeny == mojaSiet->idKlienta) {
            bzero(buffer, BUFFER);
            buffer[0] = 109;
            buffer[1] = networkID + 1;
            write(pripojeni[mojaSiet->idKlienta].socketID, buffer, BUFFER);

            bzero(buffer, BUFFER);
            buffer[0] = 19;
            buffer[1] = networkID + 1;
            for (int i = 0; i < mojaSiet->pocetUzlov; i++) {
                if (mojaSiet->uzlySiete[i] != odpojeny) {
                    write(pripojeni[mojaSiet->uzlySiete[i]].socketID, buffer, BUFFER);// segmentation fail nullptr
                }
            }
            exit = 0;
        } else {
            for (int i = 0; i < mojaSiet->pocetUzlov; i++) {
                if (mojaSiet->uzlySiete[i] == odpojeny) {
                    //vymen uzol!!
                    ///////////////////////////////////////////////////////////////////////
                    // docasne riesenie
                    bzero(buffer, BUFFER);
                    buffer[0] = 109;
                    buffer[1] = networkID + 1;
                    write(pripojeni[mojaSiet->idKlienta].socketID, buffer, BUFFER);
                    for (int i = 0; i < mojaSiet->pocetUzlov; i++) {
                        bzero(buffer, BUFFER);
                        buffer[0] = 19;
                        buffer[1] = networkID + 1;
                        write(pripojeni[mojaSiet->uzlySiete[i]].socketID, buffer, BUFFER);
                    }
                    /////////////////////////////////////////////////////////////////////////
                    exit = 0;
                    break;
                }
            }
        }
        pthread_mutex_unlock(mojaSiet->mut);
    }

    mojaSiet->counter[0]--;
    free(mojaSiet->uzlySiete);
    mojaSiet->uzlySiete = NULL;
    printf("Sieť[%d] ukončila svoju činnosť z dôvodu vypadku jedneho z účastníkov siete.\n", networkID);
    return NULL;
}

void *connection(void *param) {
    CONNECTED *con = (CONNECTED *) param;
    int id = con->idPripojeneho;
    printf("Host [%d] sa pripojil \n", id);
    char buffer[BUFFER];
    bzero(buffer, BUFFER);
    if (read(con->socketID, buffer, BUFFER) == 0) {
        pthread_mutex_lock(con->mut);
        con->data[0] = id;
        printf("Host [%d] sa odpojil \n", con->data[0]);
        pthread_mutex_unlock(con->mut);
        pthread_cond_signal(con->spracuj);
        con->counter[0]--;
        printf("Ostava pripojenych %d\n", con->counter[0]);
        close(con->socketID);
        con->socketID = 0;
        return NULL;
    } else {
        pthread_mutex_lock(con->mut);
        if (con->data[0] == -100) {
            return NULL;
        }
        pthread_mutex_unlock(con->mut);
        pthread_cond_signal(con->spracuj);
    }

}

pthread_mutex_t mut_ex;
pthread_cond_t spracuj;
pthread_t tid;
pthread_t network_thread;

void clientExit(int sign) {

    odpojil = -100;
    close(masterSocket);

    int last = 0;
    for (int i = 0; i < pocetPripojeni; i++);
    {
        for (int j = last; j < MAX_PRIPOJENI; j++) {
            if (pripojeni[j].socketID != 0) {
                close(pripojeni[j].socketID);
                last = j + 1;
                break;
            }
        }
    }
    pthread_mutex_destroy(&mut_ex);
    pthread_cond_destroy(&spracuj);
}

int main(int argc, char *argv[]) {
    odpojil = -1;
    pthread_cond_init(&spracuj, NULL);
    pthread_mutex_init(&mut_ex, NULL);

    signal(SIGINT, clientExit);

    char buffer[BUFFER];
    struct sockaddr_in server_address, client_address;
    int new_client;
    socklen_t client_address_length;

    memset((char *) &server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons((u_short) PORT);
    masterSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (masterSocket < 0) {
        printf("Nepodarilo sa vytvoriť socket\n");
        return 1;
    }
    if (bind(masterSocket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        printf("Nepodaril sa nabindovať socket\n");
        return 2;
    }
    if (listen(masterSocket, 15) < 0) {
        printf("Problem pri listening\n");
        return 3;
    }
    client_address_length = sizeof(client_address);
    printf("Server beží na porte: %d\n", PORT);

    while (1) {
        if ((new_client = accept(masterSocket, (struct sockaddr *) &client_address, &client_address_length)) <
            0) {
            perror("ERROR on accept");
            return 4;
        }
        bzero(buffer, BUFFER);
        read(new_client, buffer, BUFFER);
        if (buffer[0] == 1) {
            printf("pripojil sa klient \n");

            int novaSiet, novyPripojeny;
            for (int i = 0; i < MAX_PRIPOJENI; i++) {
                if (pripojeni[i].socketID == 0) {
                    novyPripojeny = i;
                    break;
                }
            }
            for (int i = 0; i < MAX_SIETI; i++) {
                if (siete[i].uzlySiete == NULL) {
                    novaSiet = i;
                    break;
                }
            }


            printf("pocet uzlov od klienta %d - %d \n", buffer[1], buffer[0]);
            pripojeni[novyPripojeny].jeUzol = 0;
            siete[novaSiet].pocetUzlov = buffer[1];///
            siete[novaSiet].idSiete = novaSiet;
            siete[novaSiet].idKlienta = novyPripojeny;
            siete[novaSiet].prip = pripojeni;
            siete[novaSiet].data = &odpojil;
            siete[novaSiet].mut = &mut_ex;
            siete[novaSiet].spracuj = &spracuj;
            siete[novaSiet].counter = &pocetSieti;


            int *uz = (int *) malloc(siete[novaSiet].pocetUzlov * sizeof(int));


            if (pripojeneUzly >= siete[novaSiet].pocetUzlov) {
                int last = 0;
                for (int i = 0; i < siete[novaSiet].pocetUzlov; i++) {
                    for (int j = last; j < pocetPripojeni; j++) {
                        if (pripojeni[j].jeUzol == 1) {
                            uz[i] = j;
                            last = j + 1;
                            break;
                        }
                    }
                }
                siete[novaSiet].uzlySiete = uz;
            } else {
                close(new_client);
            }

            pripojeni[novyPripojeny].socketID = new_client;
            pripojeni[novyPripojeny].idPripojeneho = novyPripojeny;
            pripojeni[novyPripojeny].data = &odpojil;
            pripojeni[novyPripojeny].mut = &mut_ex;
            pripojeni[novyPripojeny].spracuj = &spracuj;
            pripojeni[novyPripojeny].port = 0;
            pripojeni[novyPripojeny].counter = &pocetPripojeni;


            pthread_create(&tid, NULL, connection, &pripojeni[novyPripojeny]);
            pocetPripojeni++;
            pthread_create(&network_thread, NULL, network, &siete[pocetSieti]);
            pocetSieti++;
        } else if (buffer[0] == 2) {
            int novyPripojeny = pocetPripojeni;
            for (int i = 0; i < MAX_PRIPOJENI; i++) {
                if (pripojeni[i].socketID == 0) {
                    novyPripojeny = i;
                    break;
                }
            }

            pripojeni[novyPripojeny].jeUzol = 1;
            int portUzla = atoi(&buffer[1]);
            pripojeni[novyPripojeny].port = portUzla;
            pripojeni[novyPripojeny].socketID = new_client;
            pripojeni[novyPripojeny].idPripojeneho = novyPripojeny;
            pripojeni[novyPripojeny].data = &odpojil;
            pripojeni[novyPripojeny].mut = &mut_ex;
            pripojeni[novyPripojeny].spracuj = &spracuj;
            pripojeni[novyPripojeny].counter = &pocetPripojeni;
            printf("Prripojil sa uzol jeho port je %d \n", portUzla);
            pthread_create(&tid, NULL, connection, &pripojeni[novyPripojeny]);
            pripojeneUzly++;
            pocetPripojeni++;
        }


        if (10 < pocetPripojeni)
            break;
    }
    close(masterSocket);

    pthread_join(tid, NULL);
    pthread_join(network_thread, NULL);
    pthread_mutex_destroy(&mut_ex);
    pthread_cond_destroy(&spracuj);
    printf("Vlákna ukončili svoju činnosť.\n");


    pocetPripojeni = 0;
    return 0;
}