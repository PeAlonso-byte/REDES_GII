/*
 *                  S E R V I D O R
 *
 *    This is an example program that demonstrates the use of
 *    sockets TCP and UDP as an IPC mechanism.
 *
 */

/*
** Fichero: servidor.c
** Autores
** Juan Antonio Muñoz Gómez (71704175L)
** Carlos Valdunciel Gonzalo (70925424W)
*/


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>


#define PUERTO 4175
#define ADDRNOTFOUND    0xffffffff    /* return address for unfound host */
#define TAM_BUFFER 512
#define MAXHOST 128


#define USER           101
#define NICK           102
#define PRIVMSG        103
#define PRIVMSG_CANAL  104
#define JOIN           105
#define PART           106
#define QUIT           107


#define ERR_NICKNAMEINUSE "433 Nickname is already in use: "
#define ERR_ALREADYREGISTRED "462 You may not reregister:  "
#define OK_USERNAME "Welcome to de Internet Relay Network"
#define ERR_NOSUCHNICK "401 No such nick/channel"
#define ERR_NOSUCHCHANNEL "403 No such channel"

extern int errno;


/* ############ ESTRUCTURAS ############
   ##################################### */

typedef char buffer[TAM_BUFFER];

    /* USUARIOS 'CLIENTES' */

struct usuario {
    buffer username;    //nombre del usuario
    buffer nickname;    //apodo del usuario
    unsigned int host_ip;     //ip del host donde se ejcuta el cliente
    int sock;
} typedef tipoUsuario;

typedef tipoUsuario * tipoUsuarioRef;

typedef struct tipoNodo {
    tipoUsuario user;
    struct tipoNodo * sig;
} tipoNodo;

typedef tipoNodo * tipoNodoRef;
typedef tipoNodo * ListaEnlazada;
typedef ListaEnlazada * ListaEnlazadaRef;

    /* CANALES */

typedef struct tipoCanal {
    buffer nombre;
    ListaEnlazada l;
    struct tipoCanal * sig;
} tipoCanal;

typedef tipoCanal * ListaCanales;
typedef ListaCanales * ListaCanalesRef;



typedef struct datosTCP{
    int s;
    struct sockaddr_in clientaddr_in;
    ListaEnlazadaRef listaTCP;
    ListaCanalesRef listaCanalesTCP;
} datosTCP;

typedef struct datosUDP{
    int s;
    buffer buffer;
    struct sockaddr_in clientaddr_in;
    ListaEnlazadaRef listaUDP;
    ListaCanalesRef listaCanalesUDP;
} datosUDP;

/* ############### MÉTODOS #############
   ##################################### */

void * serverTCP(void * datos);
void * serverUDP(void * datos);
void errout(char *);        /* declare error out routine */

//Comprobar ordenes
int ordenNICK(char * buf, int numCaracteresAQuitar);
int ordenUSER(char * buf, int numCaracteresAQuitar);
int ordenPRIVMSG(char * buf, char* receptor, char* mensaje, int numCaracteresAQuitar);
int ordenJOIN(char* buf, int numCaracteresAQuitar);
int ordenPART(char * buf, char * receptor, char * mensaje, int numCaracteresAQuitar);

tipoNodoRef creaNodo(tipoUsuarioRef info);
int insertarAlFinal(ListaEnlazadaRef raiz, tipoUsuarioRef info);

unsigned int compruebaReceptor(ListaEnlazadaRef raiz, char *receptor);
int compruebaCanal(ListaCanalesRef canales, char * canal);
void AddUserToChannel(ListaCanalesRef raiz, char * buf, int socket);
void crearCanal(ListaCanalesRef raiz, char * buf);
void getOutChannel(ListaCanalesRef raiz, char * canal, int socket);
void sendMsgInChannel(ListaCanalesRef raiz,char * canal, char * mensaje, int socket);
void removeUserFromChannel(ListaEnlazadaRef raiz, int socket);


int FIN = 0;             /* Para el cierre ordenado */
void finalizar(){ FIN = 1; }


int main(int argc, char *argv[]) {

    int s_UDP;          /* connected socket descriptor */
    int ls_TCP;                /* listen socket descriptor */
    int tempS_UDP;
    datosTCP * infoTCP;
    pthread_t miHiloTCP;
    datosUDP * infoUDP;
    pthread_t miHiloUDP;

    int cc;                    /* contains the number of bytes read */

    struct sigaction sa = {.sa_handler = SIG_IGN}; /* used to ignore SIGCHLD */

    struct sockaddr_in myaddr_in;    /* for local socket address */
    struct sockaddr_in clientaddr_in;    /* for peer socket address */
    unsigned int addrlen;

    fd_set readmask;
    int numfds,s_mayor;

    char buffer[TAM_BUFFER];    /* buffer for packets to be read into */

    struct sigaction vec;

    //Listas de usuarios y de canales
    ListaEnlazada listaUDP = NULL;
    ListaEnlazada listaTCP = NULL;
    ListaCanales listaCanalesTCP = NULL;
    ListaCanales listaCanalesUDP = NULL;

    /* Create the listen socket. */
    ls_TCP = socket (AF_INET, SOCK_STREAM, 0);
    if (ls_TCP == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
        exit(1);
    }
    /* clear out address structures */
    memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
       memset ((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));

    addrlen = sizeof(struct sockaddr_in);

    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    myaddr_in.sin_port = htons(PUERTO);

    /* Bind the listen address to the socket. */
    if (bind(ls_TCP, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
        exit(1);
    }

    if (listen(ls_TCP, 5) == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
        exit(1);
    }


    /* Create the socket UDP. */
    s_UDP = socket (AF_INET, SOCK_DGRAM, 0);
    if (s_UDP == -1) {
        perror(argv[0]);
        printf("%s: unable to create socket UDP\n", argv[0]);
        exit(1);
       }
    /* Bind the server's address to the socket. */
    if (bind(s_UDP, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
        perror(argv[0]);
        printf("%s: unable to bind address UDP\n", argv[0]);
        exit(1);
        }
    setpgrp();

    switch (fork()) {
    case -1:        /* Unable to fork, for some reason. */
        perror(argv[0]);
        fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
        exit(1);

    case 0:
        //fclose(stdin);
        //fclose(stderr);

        if ( sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror(" sigaction(SIGCHLD)");
            fprintf(stderr,"%s: unable to register the SIGCHLD signal\n", argv[0]);
            exit(1);
            }

            /* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
        vec.sa_handler = (void *) finalizar;
        vec.sa_flags = 0;
        if ( sigaction(SIGTERM, &vec, (struct sigaction *) 0) == -1) {
            perror(" sigaction(SIGTERM)");
            fprintf(stderr,"%s: unable to register the SIGTERM signal\n", argv[0]);
            exit(1);
            }

        while (!FIN) {
            /* Meter en el conjunto de sockets los sockets UDP y TCP */
            FD_ZERO(&readmask);
            FD_SET(ls_TCP, &readmask);
            FD_SET(s_UDP, &readmask);

            if (ls_TCP > s_UDP) s_mayor=ls_TCP;
            else s_mayor=s_UDP;

            if ( (numfds = select(s_mayor+1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0) {
                if (errno == EINTR) {
                    FIN=1;
                    close (ls_TCP);
                    close (s_UDP);
                    perror("\nFinalizando el servidor. Sennal recibida en elect\n ");
                }
            }
           else {

                /* Comprobamos si el socket seleccionado es el socket TCP */
                if (FD_ISSET(ls_TCP, &readmask)) {

                if(NULL == (infoTCP = malloc(sizeof(datosTCP))))
                    exit(1);
                infoTCP->s = accept(ls_TCP, (struct sockaddr *) &clientaddr_in, &addrlen);
                if (infoTCP->s == -1)
                    exit(1);

                infoTCP->clientaddr_in = clientaddr_in;
                infoTCP->listaTCP = &listaTCP;
                //PARA TMB TNEER UNA REFERENCIA DE LA LISTA DE LOS CANALES
                infoTCP->listaCanalesTCP = &listaCanalesTCP;


                if (pthread_create(&miHiloTCP, NULL, &serverTCP, (void *)infoTCP) != 0) {
                    printf("Error al crear el Thread\n");
                    return(-1);
                }
             } /* De TCP*/

          if (FD_ISSET(s_UDP, &readmask)) {

                cc = recvfrom(s_UDP, buffer, TAM_BUFFER - 1, 0,
                   (struct sockaddr *)&clientaddr_in, &addrlen);
                if ( cc == -1) {
                    perror(argv[0]);
                    printf("%s: recvfrom error\n", argv[0]);
                    exit (1);
                }
                if(NULL == (infoUDP = malloc(sizeof(datosUDP))))
                    exit(1);

                buffer[cc]='\0';
                infoUDP->s  = socket (AF_INET, SOCK_DGRAM, 0);
                if (tempS_UDP == -1) {
                    perror(argv[0]);
                    printf("%s: unable to create socket UDP\n", argv[0]);
                    exit(1);
                   }

                myaddr_in.sin_port = 0;
                if (bind(tempS_UDP, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
                    perror(argv[0]);
                    printf("%s: unable to bind address UDP\n", argv[0]);
                    exit(1);
                    }
                /* Make sure the message received is
                * null terminated.
                */


                infoUDP->clientaddr_in = clientaddr_in;
                infoUDP->listaUDP = &listaUDP;
                strcpy(infoUDP->buffer, buffer);
                infoUDP->listaCanalesUDP = &listaCanalesUDP;


                if (pthread_create(&miHiloUDP, NULL, &serverUDP, (void *)infoUDP) != 0) {
                    printf("Error al crear el Thread\n");
                    return(-1);
                }
             }
           }
        }

        close(ls_TCP);
        close(s_UDP);

        int t = 0;

        printf("\nFin de programa servidor!\n");
    default:        /* Parent process comes here. */
        exit(0);
    }

}


void * serverTCP(void * datos) {
    datosTCP * p_datos = (datosTCP *) datos;

    int reqcnt = 0;
    buffer buf;
    char hostname[MAXHOST];
    char * ip;
    char * hora;

    int len, len1, status;
    long timevar;

    struct linger linger;       /* Permite un cierre suave */
    int comprobarOrden;

    buffer receptor;
    buffer canal;
    buffer mensaje;
    buffer msgEnviar;
    int sReceptor;

    FILE * fileLog;

    fileLog = fopen("ircd.log","a+");
    if(NULL == fileLog){
        fprintf(stderr,"El fichero log %s no se ha podido abrir\n","ircd.log");
        fflush(stderr);
        exit (1);
    }

    // Obtiene la informacion del host
    status = getnameinfo((struct sockaddr *)&(p_datos->clientaddr_in), sizeof((p_datos->clientaddr_in)), hostname, MAXHOST, NULL, 0, 0);
    if(status) {
        // Formatea la informacion del host
        if (inet_ntop(AF_INET, &((p_datos->clientaddr_in).sin_addr), hostname, MAXHOST) == NULL)
            perror(" inet_ntop \n");
    }

    time (&timevar);
    printf("\n -- Startup from %s port %u at %s -- \n", hostname, ntohs((p_datos->clientaddr_in).sin_port), (char *) ctime(&timevar));

    // Prepara el socket para un cierre suave
    linger.l_onoff  = 1;
    linger.l_linger = 1;
    if (setsockopt(p_datos->s, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1)
        errout(hostname);

    tipoUsuario tmpUser;

    /*  Recivimos las ordenes del cliente  */
    while ((len = recv(p_datos->s, buf, TAM_BUFFER, 0))) {
        reqcnt++;
        hora = (char *) ctime(&timevar);
        hora[sizeof(hora)-1] = '\0';

        ip = inet_ntoa((p_datos->clientaddr_in).sin_addr);
        fprintf(fileLog,"[%s %s %s %d %s]: %s\n", hostname, ip, "TCP", (p_datos->clientaddr_in).sin_port, hora, buf);

        /* comprobamos que orden es */
        comprobarOrden = -1;
        if(!strncmp(buf,"NICK",4))
            comprobarOrden = NICK;
        else if(!strncmp(buf,"USER",4))
            comprobarOrden = USER;
        else if(!strncmp(buf,"PRIVMSG",7)) {
            comprobarOrden = PRIVMSG;
            if(!strncmp(buf,"PRIVMSG #",9))
               comprobarOrden = PRIVMSG_CANAL;
        }else if(!strncmp(buf,"JOIN",4))
            comprobarOrden = JOIN;
        else if(!strncmp(buf,"PART",4))
            comprobarOrden = PART;
        else if(!strncmp(buf,"QUIT",4))
            comprobarOrden = QUIT;

        switch(comprobarOrden) {
            case NICK:
                ordenNICK(buf, 5);
                strcpy(tmpUser.nickname, buf);
                tmpUser.host_ip = ntohs((p_datos->clientaddr_in).sin_port);
                tmpUser.sock = p_datos->s;
                break;
            case USER:
                ordenUSER(buf, 5);
                if (send(p_datos->s,OK_USERNAME, TAM_BUFFER, 0) != TAM_BUFFER) {
                    errout(hostname);
                }
                strcpy(tmpUser.username, buf);
                insertarAlFinal(p_datos->listaTCP, &tmpUser);
                break;
            case PRIVMSG:
                ordenPRIVMSG(buf, receptor, mensaje, 8);

                unsigned int num;
                sprintf(msgEnviar,"%s%s",tmpUser.nickname,mensaje);
                if((sReceptor = compruebaReceptor(p_datos->listaTCP, receptor)) != -1){
                    if ((num = send(sReceptor, msgEnviar, TAM_BUFFER, 0)) != TAM_BUFFER) {
                        errout(hostname);
                    }
                }

                fprintf(fileLog,"[%s to %s, %s]: %s", tmpUser.nickname, receptor, hora ,mensaje);

                break;
            case PRIVMSG_CANAL:
                  ordenPRIVMSG(buf, receptor, mensaje, 8);
                  if(compruebaCanal(p_datos->listaCanalesTCP, receptor) == -1){
                      if (send(p_datos->s, ERR_NOSUCHNICK, TAM_BUFFER, 0) != TAM_BUFFER) {
                          errout(hostname);
                      }
                  } else {
                    sprintf(msgEnviar,"[%s] %s%s",receptor, tmpUser.nickname, mensaje);
                    sendMsgInChannel(p_datos->listaCanalesTCP, receptor, msgEnviar, p_datos->s);
                 }

                 fprintf(fileLog,"[%s to %s, %s] %s", tmpUser.nickname, receptor, hora ,mensaje);

                break;
            case JOIN:
                ordenJOIN(buf,5);
                if(compruebaCanal(p_datos->listaCanalesTCP, buf) == -1){
                    crearCanal(p_datos->listaCanalesTCP, buf);
                    AddUserToChannel(p_datos->listaCanalesTCP, buf, p_datos->s);
                } else {
                    AddUserToChannel(p_datos->listaCanalesTCP, buf, p_datos->s);
                }
                break;
            case PART:
                ordenPART(buf,canal,mensaje,5);
                if(compruebaCanal(p_datos->listaCanalesTCP, canal) == -1){
                   sendMsgInChannel(p_datos->listaCanalesTCP, canal, mensaje, p_datos->s);
                   getOutChannel(p_datos->listaCanalesTCP, canal, p_datos->s);
                } else {
                  if (send(p_datos->s, ERR_NOSUCHCHANNEL, TAM_BUFFER, 0) != TAM_BUFFER) {
                      errout(hostname);
                  }
                }
                break;
            case QUIT:
		            if (send(p_datos->s, "QUIT", TAM_BUFFER, 0) != TAM_BUFFER) {
                   errout(hostname);
                }
          	    removeUserFromChannel(p_datos->listaTCP, p_datos->s);
                break;
            default:
                break;
        };
        comprobarOrden = 0;
        if (len == -1) errout(hostname);

        sleep(1);
    }

    close(p_datos->s);

    printf("\nFin de programa servidor!\n");

    time (&timevar);
    printf("Completed %s port %u, %d requests, at %s\n",
        hostname, ntohs(p_datos->clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));
}

void errout(char *hostname) {
    printf("Connection with %s aborted on error\n", hostname);
    exit(1);
}


void * serverUDP(void * datos) {
    datosUDP * p_datos = (datosUDP *) datos;

    int reqcnt = 0;
    buffer buf;
    char hostname[MAXHOST];
    char * ip;
    char * hora;

    int len, len1, status;
    long timevar;

    struct linger linger;       /* Permite un cierre suave */
    int comprobarOrden;

    buffer receptor;
    buffer canal;
    buffer mensaje;
    buffer msgEnviar;
    int sReceptor;

    int addrlen;
    addrlen = sizeof(struct sockaddr_in);

    FILE * fileLog;

    fileLog = fopen("ircd.log","a+");
    if(NULL == fileLog){
        fprintf(stderr,"El fichero log %s no se ha podido abrir\n","ircd.log");
        fflush(stderr);
        exit (1);
    }

    strcpy(buf, p_datos->buffer);
    // Obtiene la informacion del host
    status = getnameinfo((struct sockaddr *)&(p_datos->clientaddr_in), sizeof((p_datos->clientaddr_in)), hostname, MAXHOST, NULL, 0, 0);
    if(status) {
        // Formatea la informacion del host
        if (inet_ntop(AF_INET, &((p_datos->clientaddr_in).sin_addr), hostname, MAXHOST) == NULL)
            perror(" inet_ntop \n");
    }

    time (&timevar);
    printf("\n -- Startup from %s port %u at %s -- \n", hostname, ntohs((p_datos->clientaddr_in).sin_port), (char *) ctime(&timevar));

    // Prepara el socket para un cierre suave
    linger.l_onoff  = 1;
    linger.l_linger = 1;
    if (setsockopt(p_datos->s, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1)
        errout(hostname);

    tipoUsuario tmpUser;

    /*  Recivimos las ordenes del cliente  */
    do {
        reqcnt++;
        hora = (char *) ctime(&timevar);
        hora[sizeof(hora)-1] = '\0';

        ip = inet_ntoa((p_datos->clientaddr_in).sin_addr);
        fprintf(fileLog,"[%s %s %s %d %s]: %s\n", hostname, ip, "UDP", (p_datos->clientaddr_in).sin_port, hora, buf);

        /* comprobamos que orden es */
        comprobarOrden = -1;
        if(!strncmp(buf,"NICK",4))
            comprobarOrden = NICK;
        else if(!strncmp(buf,"USER",4))
            comprobarOrden = USER;
        else if(!strncmp(buf,"PRIVMSG",7)) {
            comprobarOrden = PRIVMSG;
            if(!strncmp(buf,"PRIVMSG #",9))
               comprobarOrden = PRIVMSG_CANAL;
        }else if(!strncmp(buf,"JOIN",4))
            comprobarOrden = JOIN;
        else if(!strncmp(buf,"PART",4))
            comprobarOrden = PART;
        else if(!strncmp(buf,"QUIT",4))
            comprobarOrden = QUIT;

        switch(comprobarOrden) {
            case NICK:
                ordenNICK(buf, 5);
                strcpy(tmpUser.nickname, buf);
                tmpUser.host_ip = ntohs((p_datos->clientaddr_in).sin_port);
                tmpUser.sock = p_datos->s;
                break;
            case USER:
                ordenUSER(buf, 5);
                if (sendto(p_datos->s,OK_USERNAME, TAM_BUFFER, 0, (struct sockaddr *)&(p_datos->clientaddr_in), &addrlen) != TAM_BUFFER) {
                    errout(hostname);
                }
                strcpy(tmpUser.username, buf);
                insertarAlFinal(p_datos->listaUDP, &tmpUser);
                break;
            case PRIVMSG:
                ordenPRIVMSG(buf, receptor, mensaje, 8);

                unsigned int num;
                sprintf(msgEnviar,"%s%s",tmpUser.nickname,mensaje);
                if((sReceptor = compruebaReceptor(p_datos->listaUDP, receptor)) != -1){
                    if ((num = sendto(sReceptor, msgEnviar, TAM_BUFFER, 0, (struct sockaddr *)&(p_datos->clientaddr_in), &addrlen)) != TAM_BUFFER) {
                        errout(hostname);
                    }
                }

                fprintf(fileLog,"[%s to %s, %s]: %s", tmpUser.nickname, receptor, hora ,mensaje);

                break;
            case PRIVMSG_CANAL:
                  ordenPRIVMSG(buf, receptor, mensaje, 8);
                  if(compruebaCanal(p_datos->listaCanalesUDP, receptor) == -1){
                      if (sendto(p_datos->s, ERR_NOSUCHNICK, TAM_BUFFER, 0, (struct sockaddr *)&(p_datos->clientaddr_in), &addrlen) != TAM_BUFFER) {
                          errout(hostname);
                      }
                  } else {
                    sprintf(msgEnviar,"[%s] %s%s",receptor, tmpUser.nickname, mensaje);
                    sendMsgInChannel(p_datos->listaCanalesUDP, receptor, msgEnviar, p_datos->s);
                 }

                 fprintf(fileLog,"[%s to %s, %s] %s", tmpUser.nickname, receptor, hora ,mensaje);

                break;
            case JOIN:
                ordenJOIN(buf,5);
                if(compruebaCanal(p_datos->listaCanalesUDP, buf) == -1){
                    crearCanal(p_datos->listaCanalesUDP, buf);
                    AddUserToChannel(p_datos->listaCanalesUDP, buf, p_datos->s);
                } else {
                    AddUserToChannel(p_datos->listaCanalesUDP, buf, p_datos->s);
                }
                break;
            case PART:
                ordenPART(buf,canal,mensaje,5);
                if(compruebaCanal(p_datos->listaCanalesUDP, canal) == -1){
                   sendMsgInChannel(p_datos->listaCanalesUDP, canal, mensaje, p_datos->s);
                   getOutChannel(p_datos->listaCanalesUDP, canal, p_datos->s);
                } else {
                  if (sendto(p_datos->s, ERR_NOSUCHCHANNEL, TAM_BUFFER, 0, (struct sockaddr *)&(p_datos->clientaddr_in), &addrlen) != TAM_BUFFER) {
                      errout(hostname);
                  }
                }
                break;
            case QUIT:
		            if (sendto(p_datos->s, "QUIT", TAM_BUFFER, 0, (struct sockaddr *)&(p_datos->clientaddr_in), &addrlen) != TAM_BUFFER) {
                   errout(hostname);
                }
          	    removeUserFromChannel(p_datos->listaUDP, p_datos->s);
                break;
            default:
                break;
        };
        comprobarOrden = 0;
        if (len == -1) errout(hostname);
        
    } while ((len = recvfrom(p_datos->s, buf, TAM_BUFFER, 0, (struct sockaddr *)&(p_datos->clientaddr_in), &addrlen)));

    close(p_datos->s);

    printf("\nFin de programa servidor!\n");

    time (&timevar);
    printf("Completed %s port %u, %d requests, at %s\n",
        hostname, ntohs(p_datos->clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));
 }


/* ################## FUNCIONES ################## */

/* -------> comprobamos las ordenes */

 int ordenNICK(char * buf, int numCaracteresAQuitar){
     int i = numCaracteresAQuitar;

     while(buf[i] != '\r') {
         buf[i-numCaracteresAQuitar] = buf[i];
         i++;
     }
     for(i = i-numCaracteresAQuitar; i<TAM_BUFFER; i++)
         buf[i] = '\0';

     return 0;
 }

 int ordenUSER(char * buf, int numCaracteresAQuitar){
     int i = numCaracteresAQuitar;

     while(buf[i] != ' ') {
         buf[i-numCaracteresAQuitar] = buf[i];
         i++;
     }
     for(i = i-numCaracteresAQuitar; i<TAM_BUFFER; i++)
         buf[i] = '\0';

     return 0;
 }


 int ordenPRIVMSG(char * buf, char * receptor, char * mensaje, int numCaracteresAQuitar){
       int i, j;
       j = numCaracteresAQuitar;
       while(buf[j] != ' '){
         receptor[j-numCaracteresAQuitar] = buf[j];
         j++;
       }
       for(i = j-numCaracteresAQuitar; i<TAM_BUFFER; i++)
          receptor[i] = '\0';
       j++;
       i=0;
       while(buf[j] != '\r'){
         mensaje[i] = buf[j];
         i++;
         j++;
       }
       while(i<TAM_BUFFER){
         mensaje[i] = '\0';
         i++;
       }

     return 0;
 }


 int ordenJOIN(char * buf, int numCaracteresAQuitar){
     int i = numCaracteresAQuitar;

     while(buf[i] != ' ') {
         buf[i-numCaracteresAQuitar] = buf[i];
         i++;
     }
     for(i = i-numCaracteresAQuitar; i<TAM_BUFFER; i++)
         buf[i] = '\0';

     return 0;
 }

 int ordenPART(char * buf, char * receptor, char * mensaje, int numCaracteresAQuitar){
       int i, j;
       j = numCaracteresAQuitar;
       while(buf[j] != ' '){
         receptor[j-numCaracteresAQuitar] = buf[j];
         j++;
       }
       for(i = j-numCaracteresAQuitar; i<TAM_BUFFER; i++)
          receptor[i] = '\0';
       j++;
       i=0;
       while(buf[j] != '\r'){
         mensaje[i] = buf[j];
         i++;
         j++;
       }
       while(i<TAM_BUFFER){
         mensaje[i] = '\0';
         i++;
       }

     return 0;
 }


 /* -------> listas enlazadas */

tipoNodoRef creaNodo(tipoUsuarioRef info) {
    tipoNodoRef n;
    if ((n = malloc(sizeof(tipoNodo))) == NULL){
      exit(1);
    }
    else {
        n->user = * info;
        n->sig = NULL;
    }
    return n;
}

int insertarAlFinal(ListaEnlazadaRef raiz, tipoUsuarioRef info) {
    tipoNodoRef nuevo, aux, ant;

    if ((nuevo = creaNodo(info)) == NULL)
        return -1;
    else {
        if (*raiz == NULL) {
            *raiz = nuevo;
        } else {
            aux = *raiz;
            while (aux != NULL){
                ant = aux;
                aux = aux->sig;
            }
            if(aux == NULL) {
                ant->sig = nuevo;
            } else {
                free(nuevo);
                return -2;
            }
        }
        return 0;
    }
}


/* -------> canales */

int compruebaCanal(ListaCanalesRef canales, char * canal){
   ListaCanales aux;

    if(*canales == NULL)
        return -1;
    else {
        aux  = *canales;
        while(aux != NULL) {
            if(strncmp(aux->nombre, canal, sizeof(canal)) == 0){
                return 0;
            }
            aux = aux->sig;
        }
    }
    return -1;
}

unsigned int compruebaReceptor(ListaEnlazadaRef raiz, char * receptor) {
      tipoNodoRef aux;

      if(*raiz != NULL) {
          aux = *raiz;
          if(strcmp(aux->user.nickname,receptor) == 0)
              return aux->user.sock;
          while(aux != NULL) {
              aux = aux->sig;
              if(strcmp(aux->user.nickname,receptor) == 0)
                  return aux->user.sock;
          }
      }
      return -1;
}

int estaVacia(ListaCanales raiz){
	return(raiz == NULL);
}

void crearCanal(ListaCanalesRef raiz, char * buf){
    ListaCanales aux;
    tipoCanal * nuevo;

    if((nuevo = malloc(sizeof(tipoCanal))) == NULL){
      exit(1);
    } else {
      aux = *raiz;
    	if(estaVacia(*raiz)){
      		 nuevo->sig = * raiz;
      		 *raiz = nuevo;
      		 nuevo->l = NULL;
      	   strcpy(nuevo->nombre,buf);
    	} else {
         aux = *raiz;
      	 while(aux != NULL)
      			aux = aux->sig;
      	 if(aux == NULL){
        		aux->sig = nuevo;
        		nuevo->sig = NULL;
        		nuevo->l = NULL;
        		strcpy(nuevo->nombre,buf);
      	 } else{
        		free(nuevo);
            exit(1);
      	 }
    	}
   }
}


void AddUserToChannel(ListaCanalesRef raiz, char * canal, int s) {
    tipoNodoRef nuevo;
    ListaEnlazada clientes;
    ListaCanales aux;

    if(NULL == (nuevo = malloc(sizeof(tipoNodo))))
      exit(1);

    aux = *raiz;
    while(aux != NULL && strcmp(aux->nombre,canal) !=0){
        aux = aux->sig;
    }
    if(strcmp(aux->nombre,canal) == 0){
        if(aux->l == NULL){
    	      nuevo->user.sock = s;
    	      nuevo->sig = NULL;
    	      aux->l = nuevo;
	      } else {
  	        clientes = aux->l;
  	        while(clientes->sig !=NULL)
  		          clientes = clientes->sig;
  	        nuevo->user.sock = s;
  	        nuevo->sig = NULL;
  	        clientes->sig = nuevo;
	      }
    }
}


void getOutChannel(ListaCanalesRef raiz, char * canal, int s){
    ListaCanales aux;
    ListaCanales ant;

    ListaEnlazada clientes;
    ListaEnlazada anterior;

    aux = *raiz;
    while(aux != NULL && strncmp(aux->nombre,canal,sizeof(canal)) !=0){
        ant = aux;
        aux = aux->sig;
    }

    if(strncmp(aux->nombre,canal,sizeof(canal)) ==0){
        clientes = aux->l;
        while(clientes != NULL && clientes->user.sock != s){
            anterior = clientes;
            clientes = clientes->sig;
        }
        if(clientes->user.sock == s){
            anterior->sig = clientes->sig;
            free(clientes);
        }
    }

    if(aux->l == NULL){
	     ant = aux->sig;
       free(aux);
    }

}

void sendMsgInChannel(ListaCanalesRef indice,char * canal, char *  mensaje, int sock){
    ListaCanales aux = *indice;

    ListaEnlazada clientes = NULL;

    while(aux != NULL && strncmp(aux->nombre,canal,sizeof(canal)) !=0){
        aux = aux->sig;
    }

    if(strncmp(aux->nombre,canal,sizeof(canal)) ==0){
        clientes = aux->l;
        while(clientes !=NULL){
             if(clientes->user.sock != sock){
                if (send(clientes->user.sock, mensaje, TAM_BUFFER, 0) != TAM_BUFFER) {
                    exit(1);
                }
            }
            clientes = clientes->sig;
        }
    }
}


void removeUserFromChannel(ListaEnlazadaRef raiz, int s){
     tipoNodoRef aux, ant;

     aux = *raiz;
     if(aux->user.sock == s){
         ant = aux->sig;
         *raiz = ant;
         free(aux);
     } else {
         while(aux != NULL && aux->user.sock != s){
            ant = aux;
            aux = aux->sig;
         }
         if(aux != NULL){
            ant = aux->sig;
            free(aux);
         }
     }
}
