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


void printLog(char* str) {
    printf("::Log:: %s", str);
}

void printError(char* str) {
    printf("::Error:: %s", str);
}

void operation(void* param) {
    int sockCliFd = (int) param;
    printLog("Creating thread for the client\n");
    
}

int main (int argc,char *argv[]) {
    
    printLog("Launching of proxy\n");
    
    /**
     * Variable delerations
     */
    int sockfd, sockCliFd, sockServFd;
    struct sockaddr_in  serv_addr, cli_addr;
    socklen_t clilen;
    
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
        sockCliFd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        
        if (sockCliFd < 0) {
            printError("Server : Error accept\n");
            exit(1);
        }
        
        printLog("New client\n");
        
        /**
         * Multi threading
         */
        pthread_t t;
        
        if (pthread_create (&t, NULL, (void*) operation, sockCliFd) < 0) {
            printError("Server : Error thread creation\n");
            exit (1);
        }
        
    }
    
    
}
