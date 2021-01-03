/*
** Fichero: cliente.c
** Autores
** Juan Antonio Muñoz Gómez (71704175L)
** Carlos Valdunciel Gonzalo (70925424W)
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <time.h>

extern int errno;

#define PUERTO 4175
#define TAM_BUFFER 512

#define ADDRNOTFOUND    0xffffffff    /* value returned for unknown host */
#define RETRIES    5        /* number of times to retry before givin up */
#define BUFFERSIZE    1024    /* maximum size of packets to be received */
#define TIMEOUT 1
#define MAXHOST 10

void handler()
{
 printf("Alarma recibida \n");
}

void clienteTCP(char *, char *);
void clienteUDP(char *, char *);

int main(int argc, char *argv[])
{

    if(argc != 4){
        fprintf(stderr, "Usage %s <nameserver> <protocol> <namefile>\n", argv[0]);
        return 2;
    }
      else{
      /*Comprobamos si es un cliente TCP o UDP*/
      if(0 == strncmp(argv[2],"TCP",3)){
          clienteTCP(argv[1],argv[3]);
      }
      else if(0 == strncmp(argv[2],"UDP",3)){
          clienteUDP(argv[1],argv[3]);
      }
      else{
          fprintf(stderr,"Protocolo no identificado: %s\n",argv[2]);
          fflush(stderr);
          exit (1);
      }
    }

}


void clienteUDP(char * servidor, char * nombre_fichero) {
      int errcode;
      int retry = RETRIES;        /* holds the retry count */
      int s;                /* socket descriptor */
      long timevar;                       /* contains time returned by time() */
      struct sockaddr_in myaddr_in;    /* for local socket address */
      struct sockaddr_in servaddr_in;    /* for server socket address */
      struct in_addr reqaddr;        /* for returned internet address */
      int    addrlen, n_retry;
      struct sigaction vec;
      char hostname[MAXHOST];
      struct addrinfo hints, *res;

      int i;
      int tmp;

      FILE *entrada,*salida;
      char buf[TAM_BUFFER];
      char exitFileName[100];

      entrada = fopen(nombre_fichero,"r");
      if(NULL == entrada){
          fprintf(stderr,"El fichero de ordenes %s no se ha podido abrir\n",nombre_fichero);
          fflush(stderr);
          exit (1);
      }

          /* Create the socket. */
      s = socket (AF_INET, SOCK_DGRAM, 0);
      if (s == -1) {
          perror("cliente");
          fprintf(stderr, "%s: unable to create socket\n", "cliente");
          exit(1);
      }

      /* clear out address structures */
      memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
      memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

      myaddr_in.sin_family = AF_INET;
      myaddr_in.sin_port = 0;
      myaddr_in.sin_addr.s_addr = INADDR_ANY;
      if (bind(s, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
          perror("cliente");
          fprintf(stderr, "%s: unable to bind socket\n", "cliente");
          exit(1);
         }
      addrlen = sizeof(struct sockaddr_in);
      if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
              perror("cliente");
              fprintf(stderr, "%s: unable to read socket address\n", "cliente");
              exit(1);
      }

      sprintf(exitFileName,"%d.txt",myaddr_in.sin_port);
      salida = fopen(exitFileName,"w");
      if(NULL == salida){
          fprintf(stderr,"El fichero de salida no se ha podido abrir\n");
          exit(1);
      }

      time(&timevar);
      printf("Connected to %s on port %u at %s", servidor, ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));

      /* Set up the server address. */
      servaddr_in.sin_family = AF_INET;

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;
       /* esta funciÛn es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
      errcode = getaddrinfo (servidor, NULL, &hints, &res);
      if (errcode != 0){
              /* Name was not found.  Return a
               * special value signifying the error. */
          fprintf(stderr, "%s: No es posible resolver la IP de %s\n",
                  "cliente", servidor);
          exit(1);
        }
      else {
              /* Copy address of host */
          servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
       }
       freeaddrinfo(res);
       /* puerto del servidor en orden de red*/
       servaddr_in.sin_port = htons(PUERTO);

     /* Registrar SIGALRM para no quedar bloqueados en los recvfrom */
      vec.sa_handler = (void *) handler;
      vec.sa_flags = 0;
      if ( sigaction(SIGALRM, &vec, (struct sigaction *) 0) == -1) {
              perror(" sigaction(SIGALRM)");
              fprintf(stderr,"%s: unable to register the SIGALRM signal\n", "cliente");
              exit(1);
          }

      switch (fork()) {
          case -1:        /* Unable to fork, for some reason. */
              perror("cliente");
              fprintf(stderr, "%s: unable to fork daemon\n", "cliente");
              exit(1);
          case 0:
              do {
                  n_retry=RETRIES;
                  memset(buf,0,TAM_BUFFER);
                  fgets(buf,TAM_BUFFER - 2,entrada);
                  printf("C: Sent: %s",buf);

                  while (n_retry > 0) {
                      tmp = 0;
                      while (buf[tmp] != '\n' && buf[tmp] != '\0')
                          tmp++;
                      if (buf[tmp] != '\0')
                          buf[tmp] = '\0';

                      i=0;
                      while('\n' != buf[i] && '\r' != buf[i] && '\0' != buf[i]){
                          i++;
                      }
                      if('\n' == buf[i]){
                          buf[i] = '\r';
                          buf[i+1] = '\n';
                      }

                      if (sendto (s, buf, TAM_BUFFER, 0, (struct sockaddr *)&servaddr_in,
                              sizeof(struct sockaddr_in)) == -1) {
                              perror("cliente");
                              fprintf(stderr, "%s: unable to send request\n", "cliente");
                              exit(1);
                      } else {
                        n_retry = -99;
                      }

                      alarm(TIMEOUT);
                      if (recvfrom (s, buf, TAM_BUFFER, 0,
                                      (struct sockaddr *)&servaddr_in, &addrlen) == -1) {
                          if (errno == EINTR) {
                               printf("attempt %d (retries %d).\n", n_retry, RETRIES);
                               n_retry--;
                          } else  {
                              printf("Unable to get response from");
                              exit(1);
                          }
                      } else {
                          alarm(0);
                          /* Print out response. */
                          if (reqaddr.s_addr == ADDRNOTFOUND)
                             printf("Host %s unknown by nameserver %s\n", nombre_fichero, servidor);
                          break;
                      }
                  }
              } while(!feof(entrada));

              fclose(entrada);
              break;
          default:        /* Father process. */
              while (0 != strncmp(buf,"QUIT",4)) {
                 if (recv(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
                     perror("cliente");
                     fprintf(stderr, "C: error recieving result\n");
                     exit(1);
                 }
                 if(0 != strncmp(buf,"QUIT",4))
                     fprintf(salida,"%s\n", buf);
             }
             fprintf(salida,"Getting out. Bye!\n");

             fclose(salida);
             break;
      };


      if (n_retry == 0) {
         printf("Unable to get response from");
         printf(" %s after %d attempts.\n", servidor, RETRIES);
         }
}

void clienteTCP(char * servidor, char * nombre_fichero) {
    int s;
    struct addrinfo hints, *res;
    long timevar;
    struct sockaddr_in myaddr_in;
    struct sockaddr_in servaddr_in;
    int addrlen, errcode;

    int i;

    FILE *entrada,*salida;
    char buf[TAM_BUFFER];
    char exitFileName[100];

    int tmp;

    entrada = fopen(nombre_fichero,"r");
    if(NULL == entrada){
        fprintf(stderr,"El fichero de ordenes %s no se ha podido abrir\n",nombre_fichero);
        fflush(stderr);
        exit (1);
    }

    // Creamos el socket TCP local
    s = socket (AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        perror("cliente");
        fprintf(stderr, "%s: no se ha podido crear el socket\n", "cliente");
        fflush(stderr);
        exit(1);
    }

    // Limpiamos las estructuras
    memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

    servaddr_in.sin_family = AF_INET;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;

    // Obtenemos la ip asociada al nombre del host
    errcode = getaddrinfo (servidor, NULL, &hints, &res);
    if (errcode != 0){
        // Si no se encontró un host con ese hostname
        fprintf(stderr, "%s: No es posible resolver la IP de %s\n", "cliente", servidor);
        exit(1);
    } else {
        // Copiamos la dirección del host
        servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
        }
    freeaddrinfo(res);

    // Se asigna el puerto
    servaddr_in.sin_port = htons(PUERTO);

    /*-- CONECTAMOS CON EL SERVIDOR --*/

    if (connect(s, (const struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
        perror("cliente");
        fprintf(stderr, "%s: unable to connect to remote\n", "cliente");
        exit(1);
    }

    // Obtenemos la información del host local
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
        perror("cliente");
        fprintf(stderr, "%s: unable to read socket address\n", "cliente");
        exit(1);
     }

    sprintf(exitFileName,"%d.txt",myaddr_in.sin_port);
    salida = fopen(exitFileName,"w");
    if(NULL == salida){
        fprintf(stderr,"El fichero de salida no se ha podido abrir\n");
        exit(1);
    }

    time(&timevar);

    printf("Connected to %s on port %u at %s",
            servidor, ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));


    switch (fork()) {
        case -1:        /* Unable to fork, for some reason. */
            perror("cliente");
            fprintf(stderr, "%s: unable to fork daemon\n", "cliente");
            exit(1);

        case 0:
            do {
                memset(buf,0,TAM_BUFFER);
                fgets(buf,TAM_BUFFER - 2,entrada);

                tmp = 0;
                while (buf[tmp] != '\n' && buf[tmp] != '\0')
                    tmp++;
                if (buf[tmp] != '\0')
                    buf[tmp] = '\0';

                i=0;
                while('\n' != buf[i] && '\r' != buf[i] && '\0' != buf[i]){
                    i++;
                }
                if('\n' == buf[i]){
                    buf[i] = '\r';
                    buf[i+1] = '\n';
                }

                if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
                    fprintf(stderr, "%s: Connection aborted on error", "client");
                    fprintf(stderr, "on send %s\n", buf);
                    exit(1);
                }
            } while(!feof(entrada));

            fclose(entrada);

            if (shutdown(s, 1) == -1) {    //cerramos el socket porque ya no va a enviar nada más el cliente
                perror("client");
                fprintf(stderr, "%s: unable to shutdown socket\n", "client");
                exit(1);
            }
            break;
        default:        /* Father process. */
             while (0 != strncmp(buf,"QUIT",4)) {
                if (recv(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
                    perror("cliente");
                    fprintf(stderr, "C: error recieving result\n");
                    exit(1);
                }
                if(0 != strncmp(buf,"QUIT",4))
                    fprintf(salida,"%s\n", buf);
            }
            fprintf(salida,"Getting out. Bye!\n");

            fclose(salida);
            break;
    };

    time(&timevar);
    printf("\n\nAll done at %s", (char *)ctime(&timevar));


}
