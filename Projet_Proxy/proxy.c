//
//  proxy.c
//  Projet_Proxy
//
//  Created by Jean-Baptiste Dominguez on 04/05/2016.
//  Copyright © 2016 Jean-Baptiste Dominguez. All rights reserved.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <pthread.h>

#define BUFSIZE 66000
#define SOMAXCONN 20

struct params_thread {
    int* sockFd;
    pthread_t threadId;
    int* counter;
};

void printLog(char* str) {
    printf("::Log:: %s", str);
}

void printError(char* str) {
    printf("::Error:: %s", str);
}

char* get_host(char* httpRequest){
    unsigned short i = 0, j = 0;
    char* buffer = strstr(httpRequest, "Host: " );
    char* host = (char *)malloc(256 * sizeof(char));
    while( buffer != NULL && buffer[i] != '\n')
        i++;
    for(j = 6; j < i-1; j++)
        host[j - 6] = buffer[j];
    host[j-6+1] = '\0';
    return host;
}

int openTCP (char* addr) {
    
    /**
     * Déclaration des variables
     */
    
    int sockfd, isHttps = 0;
    struct sockaddr_in  serv_addr;
    struct hostent* serv_host;
    
    //printf("ADDR:%s %d\n", addr, strlen(addr));
    
    /*
     * Ouvrir une socket (a TCP socket)
     */
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) <0) {
        perror("servmulti : Probleme socket\n");
        exit (2);
    }
    
    char https[strlen(addr)];
    int find=0;
    
    if((strstr(addr, ":443")) != NULL) {
        isHttps=1;
        strcpy(https, addr);
        for (int i=0; i<strlen(addr); ++i) {
            if (addr[i]==':') {
                find=1;
            }
            if (find) {
                https[i]='\0';
            }
        }
        addr = https;
    }
    
    /*char* https = strstr(addr, ":443");
     if (https != NULL) {
     //addr = strcmp(addr, ":443");
     printf("LEN: %d", strlen(addr));
     addr[strlen(addr)-1]='\0';
     }*/
    
    //printf("ADDR:%s\n", addr);
    //printf("HTTPS:%s", https);
    
    /*
     * On récupère l'host
     */
    serv_host = gethostbyname(addr);
    if (serv_host == NULL) {
        //perror("Host introuvable...\n");
        return -1;
    }
    
    /*
     * Lier le host à la socket TCP
     */
    memset( (char*) &serv_addr,0, sizeof(serv_addr) );
    serv_addr.sin_family = serv_host->h_addrtype;
    serv_addr.sin_addr = *(struct in_addr*) serv_host->h_addr_list[0];
    
    if (isHttps) {
        serv_addr.sin_port = htons((ushort) atoi("443"));
    }
    else {
        serv_addr.sin_port = htons((ushort) atoi("80"));
    }
    
    //printf("IP Address: %s\n", inet_ntoa(serv_addr.sin_addr));
    
    
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        //perror("Server : Error connection to destination\n");
        return -1;
    }
    
    printf("Server : Connection established...\n");
    
    return sockfd;
}

void operation(void* params) {
    printLog("Start operation\n");
    
    /**
     * Variable declaration
     */
    struct params_thread* pt = (struct params_thread*) params;
    int sockCliFd = * (pt->sockFd), sockServFd;
    int nrcv, nsnd;
    char msg[BUFSIZE];
    
    /**
     * msg initialization & reading request on sockCliFd
     */
    memset((char*) msg, 0, sizeof(msg));
    if ((nrcv = read(sockCliFd, msg, sizeof(msg)-1)) < 0) {
        printError("Proxy : Error read\n");
        exit(1);
    }
    msg[nrcv] = '\0';
    
    printf("%s", msg);
    
    char* dest_addr = get_host(msg);
    int err = 0;
    
    /**
     * Opening TCP connection
     */
    if ((sockServFd = openTCP(dest_addr)) < 0) {
        printError("Server : Error opening TCP connection to destination\n");
        err = 1;
    }
    
    /**
     * Writing to destination
     */
    if (!err && (nsnd = write(sockServFd, msg, nrcv)) < 0) {
        printError("Server : Error writing to destination\n");
        err = 1;
    }
    
    /**
     * Reading from destination
     */
    if (!err && (nrcv = read(sockServFd, msg, sizeof(msg)-1)) < 0) {
        printError("Client : Error reading message from destination\n");
        err = 1;
    }
    
    /**
     * Writing to client
     */
    if (!err && (nsnd = write(sockCliFd, msg, nrcv)) < 0) {
        printError("Client : Error writing message to client\n");
        err = 1;
    }
    
    printf("%d", * (pt->sockFd));
    
    printLog("End operation\n");
    
    (*(pt->counter))--;
    
    close(sockServFd);
    close(sockCliFd);
    pthread_cancel(pt->threadId);
}

int main (int argc,char *argv[]) {
    
    printLog("Launching of proxy\n");
    
    /**
     * Variable delerations
     */
    int sockfd;
    struct sockaddr_in  serv_addr, cli_addr;
    socklen_t clilen;
    int counter = 0;
    
    /**
     * Check the number of arguments
     */
    if (argc != 2){
        printError("Usage: Port du proxy \n");
        exit(1);
    }
    
    /**
     * Open a TCP socket
     */
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) <0) {
        printError("Server : Problem socket\n");
        exit(2);
    }
    
    /**
     * Configuration of the local address & port
     */
    memset( (char*) &serv_addr,0, sizeof(serv_addr) );
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));
    
    /**
     * Link local address & port to the TCP Socket
     */
    if (bind(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr) ) <0) {
        printError("Server : Error bind\n");
        exit(1);
    }
    
    /**
     * Setting of the queue
     */
    if (listen(sockfd, SOMAXCONN) <0) {
        printError("Server : Error listen\n");
        exit(1);
    }
    
    printLog("Server operational\n");
    
    for (;;) {
        printLog("Waiting client\n");
        
        /**
         * Accepting client 
         */
        clilen = sizeof(cli_addr);
        int sockCliFd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        
        if (sockCliFd < 0) {
            printError("Server : Error accept\n");
            exit(1);
        }
        
        printLog("New client\n");
        
        /**
         * Multi threading
         */
        pthread_t t;
        struct params_thread pt;
        pt.sockFd = &sockCliFd;
        pt.threadId = t;
        pt.counter = &counter;
        
        printLog("Creating thread for the client\n");
        
        if (pthread_create (&t, NULL, (void*) operation, &pt) < 0) {
            printError("Server : Error thread creation\n");
            exit (1);
        }
        
        counter++;
    }
    
    
}
