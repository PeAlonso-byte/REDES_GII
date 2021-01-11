/*
** Fichero: cliente.c
** Autores:
** Pedro Luis Alonso Díez (72190545P)
** Esther Andrés Fraile (70918564L)
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

extern int errno;

#define ADDRNOTFOUND 0xffffffff /* value returned for unknown host */
#define RETRIES 5               /* number of times to retry before givin up */
#define BUFFERSIZE 1024         /* maximum size of packets to be received */
#define PUERTO 8564
#define TIMEOUT 6
#define MAXHOST 512
#define TAM_BUFFER 10
#define TAM_COMANDO 510

void handler()
{
    //printf("Alarma recibida \n");
}

void clienteTCP(char *, char *, char *);
void clienteUDP(char *, char *, char *);

int main(int argc, char *argv[])
{
    /*
        argv[0]=cliente
        argv[1]=localhost
        argv[2]=TCP/UDP
        argv[3]=fichero
    */
    if (argc != 4)
    {
        //fprintf(stderr, "Uso: %s <nombre_IP_servidor> <protocolo> <fichero> \n", argv[0]);
        exit(1);
    }
    else
    {
        /*Comprobamos si es un cliente TCP o UDP*/
        if (0 == strncmp(argv[2], "TCP", 3))
        {
            clienteTCP(argv[0], argv[1], argv[3]);
        }
        else if (0 == strncmp(argv[2], "UDP", 3))
        {
            clienteUDP(argv[0], argv[1], argv[3]);
        }
        else
        {
            //fprintf(stderr, "El protocolo %s no se correponde con UDP/TCP\n", argv[2]);
            fflush(stderr);
            exit(1);
        }
    }
}

void clienteUDP(char *cliente, char *servidor, char *rutaOrdenes)
{
    int i, errcode;
    int retry = RETRIES;            /* holds the retry count */
    int s;                          /* socket descriptor */
    long timevar;                   /* contains time returned by time() */
    struct sockaddr_in myaddr_in;   /* for local socket address */
    struct sockaddr_in servaddr_in; /* for server socket address */
    struct in_addr reqaddr;         /* for returned internet address */
    int addrlen, n_retry;
    struct sigaction vec;
    char hostname[MAXHOST];
    struct addrinfo hints, *res;
    FILE f;
    char buf[TAM_BUFFER];
    FILE *ficheroLog, *ordenes;
    char lineaInfo[TAM_COMANDO];
    char *divide;
    char grupo[100];

    ordenes = fopen(rutaOrdenes, "r");
    if (ordenes == NULL)
    {
        //fprintf(stdout, "Error al abrir el fichero de ordenes\n");
        return;
    }

    /* Create the socket. */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1)
    {
        perror(cliente);
        //fprintf(stderr, "%s: unable to create socket\n", cliente);
        fflush(stderr);
        exit(1);
    }

    /* clear out address structures */
    memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

    /* Bind socket to some local address so that the
		 * server can send the reply back.  A port number
		 * of zero will be used so that the system will
		 * assign any available port number.  An address
		 * of INADDR_ANY will be used so we do not have to
		 * look up the internet address of the local host.
    */

    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_port = 0;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (const struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
    {
        perror(cliente);
        //fprintf(stderr, "%s: unable to bind socket\n", cliente);
        fflush(stderr);
        exit(1);
    }
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1)
    {
        perror(cliente);
        //fprintf(stderr, "%s: unable to read socket address\n", cliente);
        fflush(stderr);
        exit(1);
    }

    char nombrePuerto[50];
    sprintf(nombrePuerto, "%u.txt", ntohs(myaddr_in.sin_port));

    ficheroLog = fopen(nombrePuerto, "w");

    if (ficheroLog == NULL)
    {
        //fprintf(stdout, "Error al crear el fichero log\n");
        return;
    }
    fprintf(ficheroLog, "Connected to %s on port %u at %s\n", servidor, ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));

    /* Print out a startup message for the user. */
    time(&timevar);
    /* The port number must be converted first to host byte
             * order before printing.  On most hosts, this is not
             * necessary, but the ntohs() call is included here so
             * that this program could easily be ported to a host
             * that does require it.
             */
    fprintf(stdout, "Connected to %s on port %u at %s", servidor, ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));

    /* Set up the server address. */
    servaddr_in.sin_family = AF_INET;

    /* Get the host information for the server's hostname that the
		 * user passed in.
		 */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    /* esta funcion es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
    errcode = getaddrinfo(servidor, NULL, &hints, &res);
    if (errcode != 0)
    {
        /* Name was not found.  Return a
               * special value signifying the error. */
        //fprintf(stderr, "%s: No es posible resolver la IP de %s\n", cliente, servidor);
        exit(1);
    }
    else
    {
        /* Copy address of host */
        servaddr_in.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    }
    freeaddrinfo(res);
    /* puerto del servidor en orden de red*/
    servaddr_in.sin_port = htons(PUERTO);

    /* Registrar SIGALRM para no quedar bloqueados en los recvfrom */
    vec.sa_handler = (void *)handler;
    vec.sa_flags = 0;
    if (sigaction(SIGALRM, &vec, (struct sigaction *)0) == -1)
    {
        perror(" sigaction(SIGALRM)");
        //fprintf(stderr, "%s: unable to register the SIGALRM signal\n", cliente);
        exit(1);
    }

    //

    n_retry = RETRIES;

    while (n_retry > 0)
    {
        /* Send the request to the nameserver. */
        if (sendto(s, "UDP", strlen("UDP"), 0, (struct sockaddr *)&servaddr_in,
                   sizeof(struct sockaddr_in)) == -1)
        {
            perror(cliente);
            //fprintf(stderr, "%s: unable to send request\n", cliente);
            exit(1);
        }
        /* Set up a timeout so I don't hang in case the packet
		 * gets lost.  After all, UDP does not guarantee
		 * delivery.
		 */
        alarm(TIMEOUT);
        /* Wait for the reply to come in. */
        if (recvfrom(s, &reqaddr, sizeof(struct in_addr), 0,
                     (struct sockaddr *)&servaddr_in, &addrlen) == -1)
        {
            if (errno == EINTR)
            {
                /* Alarm went off and aborted the receive.
    				 * Need to retry the request if we have
    				 * not already exceeded the retry limit.
    				 */
                //fprintf(stdout, "attempt %d (retries %d).\n", n_retry, RETRIES);
                n_retry--;
            }
            else
            {
                //fprintf(stdout, "Unable to get response from");
                exit(1);
            }
        }
        else
        {
            alarm(0);
            /* Print out response. */
            if (reqaddr.s_addr == ADDRNOTFOUND)
            {
                //fprintf(stdout, "Host %s unknown by nameserver %s\n", "UDP", servidor);
            }
            else
            {
                /* inet_ntop para interoperatividad con IPv6 */
                if (inet_ntop(AF_INET, &reqaddr, hostname, MAXHOST) == NULL)
                    perror(" inet_ntop \n");
                //fprintf(stdout, "Address for %s is %s\n", "UDP", hostname);
            }
            break;
        }
    }

    if (n_retry == 0)
    {
        //fprintf(stdout, "Unable to get response from");
        //fprintf(stdout, " %s after %d attempts.\n", servidor, RETRIES);
    }

} // Fin UDP

void clienteTCP(char *cliente, char *servidor, char *rutaOrdenes)
{
    int s; /* connected socket descriptor */
    struct addrinfo hints, *res;
    long timevar;
    int len;                        /* contains time returned by time() */
    struct sockaddr_in myaddr_in;   /* for local socket address */
    struct sockaddr_in servaddr_in; /* for server socket address */
    int addrlen, i, j, errcode;
    /* This example uses TAM_BUFFER byte messages. */
    char buf[TAM_BUFFER];
    FILE *ficheroLog, *ordenes;
    char lineaInfo[TAM_COMANDO];
    char *divide;
    char grupo[100];

    ordenes = fopen(rutaOrdenes, "r");
    if (ordenes == NULL)
    {
        //fprintf(stdout, "Error al abrir el fichero de ordenes\n");
        return;
    }

    // Creamos el socket TCP local
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1)
    {
        perror(cliente);
        //fprintf(stderr, "%s: unable to create socket\n", cliente);
        exit(1);
    }

    /* clear out address structures */
    memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

    /* Set up the peer address to which we will connect. */
    servaddr_in.sin_family = AF_INET;

    /* Get the host information for the hostname that the
	 * user passed in. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    /* esta función es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta */
    errcode = getaddrinfo(servidor, NULL, &hints, &res);

    if (errcode != 0)
    {
        // Si no se encontró un host con ese hostname
        //fprintf(stderr, "%s: No es posible resolver la IP de %s\n", cliente, servidor);
        exit(1);
    }
    else
    {
        /* Copy address of host */
        servaddr_in.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    }
    freeaddrinfo(res);

    /* puerto del servidor en orden de red*/
    servaddr_in.sin_port = htons(PUERTO);

    /* Try to connect to the remote server at the address
		 * which was just built into peeraddr.
		 */
    if (connect(s, (const struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1)
    {
        perror(cliente);
        //fprintf(stderr, "%s: unable to connect to remote\n", cliente);
        exit(1);
    }

    /* Since the connect call assigns a free address
		 * to the local end of this connection, let's use
		 * getsockname to see what it assigned.  Note that
		 * addrlen needs to be passed in as a pointer,
		 * because getsockname returns the actual length
		 * of the address.
		 */
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1)
    {
        perror(cliente);
        //fprintf(stderr, "%s: unable to read socket address\n", cliente);
        exit(1);
    }

    /* Print out a startup message for the user. */
    time(&timevar);
    /* The port number must be converted first to host byte
	 * order before printing.  On most hosts, this is not
	 * necessary, but the ntohs() call is included here so
	 * that this program could easily be ported to a host
	 * that does require it.
	 */
    char nombrePuerto[50];
    sprintf(nombrePuerto, "%u.txt", ntohs(myaddr_in.sin_port));

    ficheroLog = fopen(nombrePuerto, "w");

    if (ficheroLog == NULL)
    {
        //fprintf(stdout, "Error al crear el fichero log\n");
        return;
    }
    fprintf(ficheroLog, "Connected to %s on port %u at %s\n", servidor, ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));

    char comando[TAM_COMANDO] = ""; // Comando indica el comando que vas a enviar y buf recibe el codigo del servidor.
    char comandoaux[TAM_COMANDO] = "";
    while (1)
    {
        memset(comando, '\0', sizeof(comando));
        memset(comandoaux, '\0', sizeof(comandoaux));
        memset(lineaInfo, '\0', sizeof(lineaInfo));

        //printf("Escribe el comando que deseas enviar al servidor: \n");

        //Descomentar esto para hacerlo por fichero.
        fgets(comando, TAM_COMANDO, (FILE *)ordenes);

        // Comentar esto para hacerlo con fichero
        //fgets(comando, TAM_COMANDO, stdin);
        // ---------------------------------------

        // CODIGO PARA AÑADIR EL \R\N A LOS COMANDOS

        i = 0;
        while ('\n' != comando[i] && '\r' != comando[i] && '\0' != comando[i])
        {
            i++;
        }
        if ('\n' == comando[i])
        {
            comando[i] = '\r';
            comando[i + 1] = '\n';
        }

        // FIN DE FORMATEO DE COMANDOS.

        fprintf(ficheroLog, "C: %s\n", comando);
        //envio de datos
        if (send(s, comando, TAM_COMANDO, 0) != TAM_COMANDO)
        {
            //fprintf(stderr, "%s: Connection aborted on error ", cliente);
            //fprintf(stderr, "on send number %d\n", i);
            exit(1);
        }
        /* Con este codigo de aqui se recibe la respuesta al comando */
        i = recv(s, buf, TAM_BUFFER, 0);

        if (i == -1)
        {
            perror(cliente);
            //fprintf(stderr, "%s: error reading result\n", cliente);
            exit(1);
        }

        if ((strcmp(comando, "QUIT\r\n") == 0 || strcmp(comando, "quit\r\n") == 0))
        {

            if (strcmp(buf, "205\r\n") == 0)
            {
                if (shutdown(s, 1) == -1)
                {
                    perror(cliente);
                    //fprintf(stderr, "%s: unable to shutdown socket\n", cliente);
                    exit(1);
                }
                //printf("205 closing connection\n");
                fprintf(ficheroLog, "S: 205 closing connection\n");
            }
            break;
        }

        //######## LIST ###########
        if ((strcmp(comando, "LIST\r\n") == 0) || (strcmp(comando, "list\r\n") == 0))
        {
            if (strcmp(buf, "215\r\n") == 0)
            {
                //printf("215 listado de los grupos en formato <nombre> <ultimo> <primero> <fecha> <descripcion>\n");
                fprintf(ficheroLog, "S: 215 listado de los grupos en formato <nombre> <ultimo> <primero> <fecha> <descripcion>\n");

                while (1)
                {
                    recv(s, lineaInfo, TAM_COMANDO, 0); // Leemos infinitamente hasta que encontremos un . solo.
                    //fprintf(stdout, "%s", lineaInfo);
                    fprintf(ficheroLog, "S: %s", lineaInfo);

                    if (strncmp(lineaInfo, ".\r\n", 3) == 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                // Errores al abrir el archivo.
                //fprintf(stdout, "Error al abrir el archivo de grupos\n");
                fprintf(ficheroLog, "S: Error al abrir el archivo grupos.\n");
            }
        }
        //######## NEWGROUPS ###########
        else if ((strncmp(comando, "NEWGROUPS", 9) == 0) || (strncmp(comando, "newgroups", 9) == 0))
        {
            memset(comandoaux, '\0', sizeof(comandoaux));
            if (strcmp(buf, "231\r\n") == 0)
            {
                recv(s, comandoaux, TAM_COMANDO, 0);
                //fprintf(stdout, "%s", comandoaux);
                fprintf(ficheroLog, "S: %s", comandoaux);

                while (1)
                {
                    recv(s, lineaInfo, TAM_COMANDO, 0); // Leemos infinitamente hasta que encontremos un . solo.
                    //fprintf(stdout, "%s", lineaInfo);
                    fprintf(ficheroLog, "S: %s", lineaInfo);

                    if (strncmp(lineaInfo, ".\r\n", 3) == 0)
                    {
                        break;
                    }
                }
            }
            else if (strcmp(buf, "501\r\n") == 0)
            {

                //printf("\n501 Error de sintaxis. ");
                //printf("Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
                fprintf(ficheroLog, "S: 501 Error de sintaxis. Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
            }
            else
            {
                //fprintf(stdout, "%s\n", buf);
                //printf("Error al abrir el fichero de grupos.\n");
                fprintf(ficheroLog, "S: Error al abrir el fichero de grupos.\n");
            }
        }
        //######## NEWNEWS ###########
        else if ((strncmp(comando, "NEWNEWS", 7) == 0) || (strncmp(comando, "newnews", 7) == 0))
        {
            memset(comandoaux, '\0', sizeof(comandoaux));
            memset(lineaInfo, '\0', sizeof(lineaInfo));
            if (strcmp(buf, "230\r\n") == 0)
            {
                recv(s, comandoaux, TAM_COMANDO, 0);
                //fprintf(stdout, "%s", comandoaux);
                fprintf(ficheroLog, "S: %s", comandoaux);

                while (1)
                {
                    recv(s, lineaInfo, TAM_COMANDO, 0); // Leemos infinitamente hasta que encontremos un . solo.
                    //fprintf(stdout, "%s", lineaInfo);
                    fprintf(ficheroLog, "S: %s", lineaInfo);

                    if (strncmp(lineaInfo, ".\r\n", 3) == 0)
                    {
                        break;
                    }
                }
            }
            else if (strcmp(buf, "501\r\n") == 0)
            {

                //printf("\n501 Error de sintaxis. ");
                //printf("Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
                fprintf(ficheroLog, "S: 501 Error de sintaxis. Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
            }
            else if (strcmp(buf, "430\r\n") == 0){
                fprintf(ficheroLog, "S: 430 No se encuentra el articulo.\n"); // Enviamos el primer comando
            } else 
            {
                //fprintf(stdout, "%s\n", buf);
                //printf("Error al abrir el fichero de grupos.\n");
                fprintf(ficheroLog, "S: Error al abrir el fichero de grupos.\n");
            }
        }
        //######## GROUP ###########
        else if ((strncmp(comando, "GROUP", 5) == 0) || (strncmp(comando, "group", 5) == 0))
        {
            memset(grupo, '\0', sizeof(grupo));
            if (strcmp(buf, "211\r\n") == 0)
            {
                recv(s, lineaInfo, TAM_COMANDO, 0); // Leemos el grupo que nos han mandado
                //fprintf(stdout, "221 %s", lineaInfo);
                fprintf(ficheroLog, "S: 221 %s", lineaInfo);
            }
            else if (strcmp(buf, "441\r\n") == 0)
            {
                int longitudComando = strlen(comando);
                comando[longitudComando - 1] = '\0';
                comando[longitudComando - 2] = '\0';
                comando[longitudComando] = '\0';
                divide = strtok(comando, " ");
                divide = strtok(NULL, " ");
                strcpy(grupo, divide);
                fprintf(ficheroLog, "S: 441 %s No existe ese grupo de noticias\n", divide);
                //fprintf(stdout, "441 %s No existe ese grupo de noticias\n", divide);
            }
            else if (strcmp(buf, "501\r\n") == 0)
            {

                //printf("501 Error de sintaxis\n");
                fprintf(ficheroLog, "S: 501 Error de sintaxis. Uso: <group> <grupo_noticias>\n");
            }
            else
            {
                // Errores al abrir el archivo.
                //fprintf(stdout, "El error es: %s\n", buf);
                //fprintf(stdout, "Error al abrir el archivo de grupos\n");
                fprintf(ficheroLog, "S: Error al abrir el archivo grupos.\n");
            }
        }
        //######## ARTICLE ###########
        else if ((strncmp(comando, "ARTICLE", 7) == 0) || (strncmp(comando, "article", 7) == 0))
        {
            memset(lineaInfo, '\0', sizeof(lineaInfo));
            if (strcmp(buf, "223\r\n") == 0)
            {
                recv(s, comandoaux, TAM_COMANDO, 0);
                //fprintf(stdout, "%s", comandoaux);
                fprintf(ficheroLog, "S: %s", comandoaux);
                memset(comandoaux, '\0', sizeof(comandoaux));
                while (1)
                {
                    recv(s, lineaInfo, TAM_COMANDO, 0); // Leemos infinitamente hasta que encontremos un . solo.
                    //fprintf(stdout, "%s", lineaInfo);
                    fprintf(ficheroLog, "S: %s", lineaInfo);

                    if (strncmp(lineaInfo, ".\r\n", 3) == 0)
                    {
                        break;
                    }
                    memset(lineaInfo, '\0', sizeof(lineaInfo));
                }
            }
            else if (strcmp(buf, "501\r\n") == 0)
            {
                //fprintf(stdout, "501 Error de sintaxis. Uso: <article> <numero_articulo>\n");
                fprintf(ficheroLog, "S: 501 Error de sintaxis. Uso: <article> <numero_articulo>\n");
            }
            else if (strcmp(buf, "430\r\n") == 0)
            {
                //fprintf(stdout, "430 No se encuentra ese articulo\n");
                fprintf(ficheroLog, "S: 430 No se encuentra ese articulo\n");
            }
            else if (strcmp(buf, "423\r\n") == 0)
            {
                //fprintf(stdout, "423 No existe el articulo en este grupo de noticias\n");
                fprintf(ficheroLog, "S: 423 No existe el articulo en este grupo de noticias\n");
            }
        }
        //######## HEAD ###########
        else if ((strncmp(comando, "HEAD", 4) == 0) || (strncmp(comando, "head", 4) == 0))
        {
            memset(lineaInfo, '\0', sizeof(lineaInfo));
            if (strcmp(buf, "221\r\n") == 0)
            {
                recv(s, comandoaux, TAM_COMANDO, 0);
                //fprintf(stdout, "%s", comandoaux);
                fprintf(ficheroLog, "S: %s", comandoaux);
                memset(comandoaux, '\0', sizeof(comandoaux));
                while (1)
                {
                    recv(s, lineaInfo, TAM_COMANDO, 0);
                    //fprintf(stdout, "%s", lineaInfo);
                    fprintf(ficheroLog, "S: %s", lineaInfo);

                    if (strncmp(lineaInfo, "\r\n", 2) == 0)
                    {
                        break;
                    }
                    memset(lineaInfo, '\0', sizeof(lineaInfo));
                }
            }
            else if (strcmp(buf, "501\r\n") == 0)
            {
                //fprintf(stdout, "501 Error de sintaxis. Uso: <head> <numero_articulo>\n");
                fprintf(ficheroLog, "S: 501 Error de sintaxis. Uso: <head> <numero_articulo>\n");
            }
            else if (strcmp(buf, "430\r\n") == 0)
            {
                //fprintf(stdout, "430 No se encuentra ese articulo\n");
                fprintf(ficheroLog, "S: 430 No se encuentra ese articulo\n");
            }
            else if (strcmp(buf, "423\r\n") == 0)
            {
                //fprintf(stdout, "423 No existe el articulo en este grupo de noticias\n");
                fprintf(ficheroLog, "S: 423 No existe el articulo en este grupo de noticias\n");
            }
        }
        //######## BODY ###########
        else if ((strncmp(comando, "BODY", 4) == 0) || (strncmp(comando, "body", 4) == 0))
        {
            int flagBody = 0;
            memset(lineaInfo, '\0', sizeof(lineaInfo));
            if (strcmp(buf, "222\r\n") == 0)
            {
                recv(s, comandoaux, TAM_COMANDO, 0);
                //fprintf(stdout, "%s", comandoaux);
                fprintf(ficheroLog, "S: %s", comandoaux);
                memset(comandoaux, '\0', sizeof(comandoaux));
                while (1)
                {
                    recv(s, lineaInfo, TAM_COMANDO, 0);

                    if (strcmp(lineaInfo, "\r\n") != 0 && flagBody == 0)
                    {
                        continue;
                    }
                    flagBody = 1;
                    //fprintf(stdout, "%s", lineaInfo);
                    fprintf(ficheroLog, "S: %s", lineaInfo);

                    if (strcmp(lineaInfo, ".\r\n") == 0)
                    {
                        memset(lineaInfo, '\0', sizeof(lineaInfo));
                        break;
                    }
                    memset(lineaInfo, '\0', sizeof(lineaInfo));
                }
            }
            else if (strcmp(buf, "501\r\n") == 0)
            {
                //fprintf(stdout, "501 Error de sintaxis. Uso: <body> <numero_articulo>\n");
                fprintf(ficheroLog, "S: 501 Error de sintaxis. Uso: <body> <numero_articulo>\n");
            }
            else if (strcmp(buf, "430\r\n") == 0)
            {
                //fprintf(stdout, "430 No se encuentra ese articulo\n");
                fprintf(ficheroLog, "S: 430 No se encuentra ese articulo\n");
            }
            else if (strcmp(buf, "423\r\n") == 0)
            {
                //fprintf(stdout, "423 No existe el articulo en este grupo de noticias\n");
                fprintf(ficheroLog, "S: 423 No existe el articulo en este grupo de noticias\n");
            }
        }
        //######## POST ###########
        else if ((strncmp(comando, "POST", 4) == 0) || (strncmp(comando, "post", 4) == 0))
        {
            if (strcmp(buf, "340\r\n") == 0)
            { // Se puede realizar el post.
                //printf("340 Subiendo un articulo; finalice con una linea que solo contenga un punto\n");
                fprintf(ficheroLog, "S: 340 Subiendo un articulo; finalice con una linea que solo contenga un punto\n");

                // Vamos a enviar bloques de 510 caracteres, hasta que pongamos un solo punto que indicará el fin de envío.
                while (strcmp(comando, ".\r\n") != 0)
                {
                    memset(comando, '\0', sizeof(comando));
                    /* -------------------------------------------------------------------------------- */

                    //fgets(comando, TAM_COMANDO, stdin); // Comentar esto para hacerlo con ficheros.
                    fgets(comando, TAM_COMANDO, (FILE *)ordenes); // Descomentar esto para hacerlo con ficheros.

                    /* ---------------------------------------------------------------------------------------- */

                    // Formateamos el comando para que acabe en /r/n
                    i = 0;
                    while ('\n' != comando[i] && '\r' != comando[i] && '\0' != comando[i])
                    {
                        i++;
                    }
                    if ('\n' == comando[i])
                    {
                        comando[i] = '\r';
                        comando[i + 1] = '\n';
                    }

                    // Fin de formateo.
                    fprintf(ficheroLog, "C: %s", comando);
                    if (send(s, comando, TAM_COMANDO, 0) != TAM_COMANDO) // Este send da el error que aparece en la esquina derecha de la pantalla
                    {
                        //fprintf(stderr, "%s: Connection aborted on error ", cliente);
                        //fprintf(stderr, "on send number %d\n", i);
                        fprintf(ficheroLog, "%s: Connection aborted on error ", cliente);
                        fprintf(ficheroLog, "on send number %d\n", i);
                        exit(1);
                    }
                }

                // Aqui tenemos que esperar a que el servidor nos envie si se ha publicado con exito o no.
                i = recv(s, buf, TAM_BUFFER, 0);
                if (strcmp(buf, "240\r\n") == 0)
                {
                    //printf("240 Article received OK\n");
                    fprintf(ficheroLog, "S: 240 Article received OK\n");
                }
                else
                {
                    //printf("441 Posting failed\n");
                    fprintf(ficheroLog, "S: 441 Posting failed\n");
                }
            }
            else
            {
                fprintf(ficheroLog, "Received result number %s\n", buf);
                //printf("440 Posting no permitido\n");
            }
        }
        else
        {
            if (strcmp(buf, "500\r\n") == 0)
            {
                //printf("500 Comando no reconocido\n");
                fprintf(ficheroLog, "S: 500 Comando no reconocido\n");
            }
        }
    }
    /* Print message indicating completion of task. */
    time(&timevar);
    //fprintf(stdout, "\n\nAll done at %s", (char *)ctime(&timevar));
    fprintf(ficheroLog, "\n\nAll done at %s", (char *)ctime(&timevar));
    fclose(ficheroLog);
} // Fin TCP