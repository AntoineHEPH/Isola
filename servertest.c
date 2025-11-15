#define REUSE 1
#define REVDNS 1

#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#ifdef REVDNS
#include <netdb.h>
#endif

int main() {
    int s, c, n, clilen;
    struct sockaddr_in srv, cli;
    char buf[512];

#ifdef REUSE
    int optval;
#endif
    char namebuf[128],portbuf[32];
#ifdef REVDNS

#endif

    /* Create socket */
    s=socket(AF_INET,SOCK_STREAM,0); // type: DGRAM, STREAM (flux continu:TCP), RAW (paquets (control total)) Seul root peut créer un socket RAW (sous linux)
    // protocol : 0 = on laisse le système choisir | fichiers ETC/PROTOCOL : pour voir les protocols
    //domain : sockaddr_ir dépend de la famille(=domain) et du port


#ifdef REUSE
    /* Set option REUSEADDR to allow direct reconnection on socket when client closed */
    optval=1;
    setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&optval,sizeof(optval));
#endif

    /* Bind to port, Address = ANY (0.0.0.0) */
    bzero(&srv,sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons(6000);
    if (bind(s,(struct sockaddr *) &srv,sizeof(srv))<0) {
        printf("error bind\n");
    }

    /* Put socket in Listen mode, max backlog = 1 */
    if (listen(s,1)!=0) {
        printf("error listen\n");
    }
    /* Accept an incoming connexion. Other side address in cli */
    clilen=sizeof(cli); /* Initial value = size of address buffer */
    c = accept(s,(struct sockaddr *) &cli, &clilen);
            /* And now, clilen holds the size of the filled part of address buffer */
            //C'est le socket c qui va nous être utile, le socket s sert juste à autoriser la connexion

    /* Display incoming address */
    printf("Connexion from %s \n",inet_ntoa(cli.sin_addr));

#ifdef REVDNS
    getnameinfo((struct sockaddr *) &cli, sizeof(cli),
            namebuf,sizeof(namebuf),
            portbuf,sizeof(portbuf), /* Or NULL,0 for no port info */
            0); /* NI_NOFQDN, NI_NUMERIHOST */
    printf("Hostname : %s\n",namebuf);
#endif

    /* Communication using socket c */
    write(c,"Hello\n",6);

    while(1) {
        bzero(buf,512);
        n=read(c,buf,512);
        if (strncmp(buf,"exit",4) == 0) {
            write(c,"OK\n",4);
            close(c);
            break;
        }
        write(c,buf,n);
    }
    close(s);
    return 0;
}


/* getaddrinfo : flags = AI_PASSIVE => si adresse = NULL, utiliser INADDR_ANY
 * Permet de retrouver le numÃ©ro de port Ã  partir du protocole. */
