/*********************************************************************
 *                                                                   *
 * FICHIER: SERVER_TCP                                               *
 *                                                                   *
 * DESCRIPTION: Utilisation de TCP socket par une application serveur*
 *              application client                                   *
 *                                                                   *
 * principaux appels (du point de vue serveur) pour un protocole     *
 * oriente connexion:                                                *
 *     socket()                                                      *
 *                                                                   *
 *     bind()                                                        *
 *                                                                   *
 *     listen()                                                      *
 *                                                                   *
 *     accept()                                                      *
 *                                                                   *
 *     read()                                                        *
 *                                                                   *
 *     write()                                                       *
 *                                                                   *
 *********************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>

#define BUFSIZE 1500

char * get_host(char * httpRequest){
    unsigned short i = 0, j = 0;
    char * buffer = strstr(httpRequest, "Host: " );
    char * host = (char *)malloc(256 * sizeof(char));
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
    
    int sockfd;
    struct sockaddr_in  serv_addr;
    struct hostent* serv_host;
    
    /*
     * Ouvrir une socket (a TCP socket)
     */
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) <0) {
        perror("servmulti : Probleme socket\n");
        exit (2);
    }
    
    
    /*
     * On récupère l'host
     */
    serv_host = gethostbyname(addr);
    if (serv_host == NULL) {
        perror("Host introuvable...");
        return -1;
    }
    
    /*
     * Lier le host à la socket TCP
     */
    memset( (char*) &serv_addr,0, sizeof(serv_addr) );
    serv_addr.sin_family = serv_host->h_addrtype;
    serv_addr.sin_addr = *(struct in_addr*) serv_host->h_addr_list[0];
    printf("IP Address: %s\n", inet_ntoa(serv_addr.sin_addr));
    //serv_addr.sin_port = htons(atoi(argv[1]));
    
    
    /*
     
     if (bind(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr) ) <0) {
     perror ("servmulti : erreur bind\n");
     exit (1);
     }
     
     /* ParamËtrer le nombre de connexion "pending" *
     if (listen(sockfd, SOMAXCONN) <0) {
     perror ("servmulti : erreur listen\n");
     exit (1);
     }*/
    return 0;
}

int str_echo (int sockfd)
{
    int nrcv, nsnd = 1;
    char msg[BUFSIZE];
    
    /*    * Attendre  le message envoye par le client
     */
    memset( (char*) msg, 0, sizeof(msg) );
    if ( (nrcv= read ( sockfd, msg, sizeof(msg)-1) ) < 0 )  {
        perror ("servmulti : : readn error on socket");
        exit (1);
    }
    msg[nrcv]='\0';
    
    char* addr_dest = get_host(msg);
    
    if (openTCP(addr_dest) < 0) {
        perror("Erreur lors de l'ouverture de la connexion TCP de la destination...");
        return 0;
    }
    
    //printf("HOST: %s\n", addr_dest);
    
    /*printf ("servmulti :message recu=%s du processus %d nrcv = %d \n",msg,getpid(), nrcv);
     
     if ( (nsnd = write (sockfd, msg, nrcv) ) <0 ) {
     printf ("servmulti : writen error on socket");
     exit (1);
     }
     printf ("nsnd = %d \n", nsnd);*/
    return 0;
} /* end of function */


void usage(){
    printf("usage : servmulti numero_port_serveur_TCP\n");
}


int main (int argc,char *argv[])

{
    printf("Launching of proxy...\n");
    
    int sockfd, sockfd1, n, newsockfd, childpid, servlen, fin;
    struct sockaddr_in  serv_addr, cli_addr;
    socklen_t clilen;
    int tab_clients[FD_SETSIZE];
    
    fd_set rset, pset;
    int maxfdp1, nbfd;
    
    /* Verifier le nombre de paramËtre en entrÈe */
    if (argc != 2){
        usage();
        exit(1);
    }
    
    /**
     *
     *
     * Configuration socket TCP
     *
     *
     */
    
    /*
     * Ouvrir une socket (a TCP socket)
     */
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) <0) {
        perror("servmulti : Probleme socket\n");
        exit (2);
    }
    
    /*
     * Lier l'adresse  locale à la socket TCP
     */
    memset( (char*) &serv_addr,0, sizeof(serv_addr) );
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));
    
    
    if (bind(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr) ) <0) {
        perror ("servmulti : erreur bind\n");
        exit (1);
    }
    
    /* ParamËtrer le nombre de connexion "pending" */
    if (listen(sockfd, SOMAXCONN) <0) {
        perror ("servmulti : erreur listen\n");
        exit (1);
    }
    
    // J'initialise ce dont on a besoin
    for (int i=0; i<FD_SETSIZE; i++) {
        tab_clients[i]=-1;
    }
    
    maxfdp1=sockfd+1;
    
    FD_ZERO(&rset);
    FD_ZERO(&pset);
    FD_SET(sockfd, &rset);
    
    printf("Server is operationnal (%s).\n", argv[1]);
    
    for (;;) {
        
        // Je sauvegarde rset
        FD_COPY(&rset, &pset);
        pset=rset;
        
        printf("Waiting client...\n");
        
        // Je fais le select
        nbfd = select(maxfdp1, &pset, NULL, NULL, NULL);
        
        //printf("nbfd=%d\n", nbfd);
        
        /*
         * Client TCP
         */
        // Je sauvegarde la nouvelle socket de dialog
        if(FD_ISSET(sockfd, &pset)) {
            
            clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,  &clilen);
            if (newsockfd < 0) {
                perror("servmulti : erreur accept\n");
                exit (1);
            }
            
            // Je cherche une place pour mon client
            int i=0;
            while ((i < FD_SETSIZE) && (tab_clients[i] >= 0)) i++;
            if (i == FD_SETSIZE) {
                exit(1);
            }
            
            // Je rajoute mon client
            tab_clients[i] = newsockfd;
            FD_SET(newsockfd, &rset);
            
            // Je change le maxfdp si besoin
            if (newsockfd >= maxfdp1) {
                maxfdp1=newsockfd+1;
            }
            
            nbfd--;
            
            printf("Nouveau client: %d\n", newsockfd);
            
        }
        
        // Parcourir le tableau des connectés
        int i=0;
        int sockcli;
        while ((nbfd > 0) && (i < FD_SETSIZE)) {
            if ( ((sockcli = tab_clients[i]) >= 0) && (FD_ISSET(sockcli, &pset)) ) {
                if (str_echo(sockcli) == 0) {
                    close(sockcli);
                    tab_clients[i] = -1;
                    FD_CLR(sockcli, &rset);
                }
                nbfd--;
            }
            i++;
        }
    }
}



















