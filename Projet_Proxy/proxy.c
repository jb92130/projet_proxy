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

#define BUFSIZE 20000
#define LOG_ACTIVATE 1

struct params_thread {
    int sockFd;
    pthread_t* threadId;
    int* counter;
};

void printLog(char* str) {
    if (LOG_ACTIVATE) {
        printf("::Log:: %s", str);
    }
}

void printError(char* str) {
    if (LOG_ACTIVATE) {
        printf("::Error:: %s", str);
    }
}

char* get_host(char* httpRequest) {
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

char* delete_url(char* httpRequest) {
    char* newRequest = malloc(sizeof(char)*strlen(httpRequest));
    
    /**
     *  METHOD ... url/path ...
     */
    int count = 0, len = 0;
    int i = 0;
    int modeDelete = 0;
    int nevermore = 0;
    while (httpRequest[i] != '\0') {
        if (nevermore == 0
            && httpRequest[i] == 'h' && httpRequest[i+1] == 't'
            && httpRequest[i+2] == 't' && httpRequest[i+3] == 'p') {
            modeDelete = 1;
        }
        
        if (modeDelete) {
            nevermore = 1;
        }
        
        if (httpRequest[i] == '/') {
            count++;
            
            if (count == 3) {
                modeDelete = 0;
            }
        }
        
        if (!modeDelete) {
            newRequest[len] = httpRequest[i];
            len++;
        }
        i++;
    }
    return newRequest;
}

int openTCP (char* addr) {
    
    /**
     * Déclaration des variables
     */
    
    int sockfd, isHttps = 0;
    struct sockaddr_in  serv_addr;
    struct hostent* serv_host;
    
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
    
    /*
     * On récupère l'host
     */
    serv_host = gethostbyname(addr);
    if (serv_host == NULL) {
        return -2;
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
    
    
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        return -1;
    }
    
    printLog("Server : Connection established...\n");
    
    return sockfd;
}

void operation(void* params) {
    printLog("Start operation\n");
    
    /**
     * Variable declaration
     */
    struct params_thread* pt = (struct params_thread*) params;
    int sockCliFd = pt->sockFd, sockServFd;
    int nrcv, nsnd;
    char msg[BUFSIZE];
    char* request;
    int err = 0;
    
    /**
     * msg initialization & reading request on sockCliFd
     */
    memset((char*) msg, 0, sizeof(msg));
    if ((nrcv = read(sockCliFd, msg, sizeof(msg)-1)) < 0) {
        printError("Proxy : Error read\n");
        err = 1;
    }
    
    if (err == 0) {
        
        
        char* dest_addr = get_host(msg);
        
        request = delete_url(msg);
        nrcv = strlen(request);
        request[nrcv] = '\0';
        
        /**
         * Opening TCP connection
         */
        if ((sockServFd = openTCP(dest_addr)) < 0) {
            if (sockServFd == -2) {
                printError("Server : Host introuvable\n");
            }
            printError("Server : Error opening TCP connection to destination\n");
            err = 1;
        }
        
        free(dest_addr);
        
        /**
         * Writing to destination
         */
        if (err > 0 || (nsnd = write(sockServFd, request, nrcv)) < 0) {
            printError("Server : Error writing to destination\n");
            err = 1;
        }
        
        free(request);
        
        /**
         * Reading from destination
         */
        while (err == 0 && (nrcv = read(sockServFd, msg, sizeof(msg)-1)) > 0) {
            
            msg[nrcv] = '\0';
            
            /**
             * Writing to client
             */
            if ((nsnd = write(sockCliFd, msg, nrcv)) < 0) {
                printError("Client : Error writing message to client\n");
                err = 1;
            }
            
        }
        
        if (err == 0 && nrcv == -1) {
            printError("Client : Error reading message from destination\n");
            err = 1;
        }
    }
    
    printLog("End operation\n");
    
    (*(pt->counter))--;
    
    close(sockServFd);
    close(sockCliFd);
    
    pthread_t t = *pt->threadId;
    free(pt->threadId);
    free(pt);
    
    pthread_cancel(t);
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
        printError("Server : Error bind, port already used\n");
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
    int err = 0;
    
    for (;;) {
        printLog("Waiting client\n");
        
        /**
         * Accepting client 
         */
        clilen = sizeof(cli_addr);
        int sockCliFd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        
        if (sockCliFd < 0) {
            printError("Server : Error accept\n");
            err = 1;
        }
        
        if (err == 0) {
        
            printLog("New client\n");
            
            /**
             *  En attente de libération des threads...
             */
            while (counter>10) {
                sleep(100);
            }
            
            /**
             * Multi threading
             */
            pthread_t* t = malloc(sizeof(pthread_t));
            struct params_thread* pt = malloc(sizeof(struct params_thread));
            pt->sockFd = sockCliFd;
            pt->threadId = t;
            pt->counter = &counter;
            
            printLog("Creating thread for the client\n");
            
            if (pthread_create(t, NULL, (void*) operation, pt) < 0) {
                printError("Server : Error thread creation\n");
                exit (1);
            }
            
            counter++;
        }
    }
    
    
}
