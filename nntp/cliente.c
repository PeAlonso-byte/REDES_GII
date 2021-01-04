/*
** Fichero: cliente.c
** Autores
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
#define TAM_COMANDO 512

void handler()
{
    printf("Alarma recibida \n");
}

void clienteTCP(char *, char *);
void clienteUDP(char *, char *);

int main(int argc, char *argv[])
{
    /*
        argv[0]=cliente
        argv[1]=localhost
        argv[2]=TCP/UDP
    */
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <nombre_IP_servidor> <protocolo>\n", argv[0]);
        exit(1);
    }
    else
    {
        /*Comprobamos si es un cliente TCP o UDP*/
        if (0 == strncmp(argv[2], "TCP", 3))
        {
            clienteTCP(argv[0], argv[1]);
        }
        else if (0 == strncmp(argv[2], "UDP", 3))
        {
            clienteUDP(argv[0], argv[1]);
        }
        else
        {
            fprintf(stderr, "El protocolo %s no se correponde con UDP/TCP\n", argv[2]);
            fflush(stderr);
            exit(1);
        }
    }
}

//NO TENGO CLAROS LOS ARGUMENTOS QUE SE LE PASAN AUN
void clienteUDP(char *cliente, char *servidor)
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

    // Cachito de Juanan

    //int tmp;

    /*FILE *entrada, *salida;
    char buf[BUFFERSIZE];
    char exitFileName[100];

    entrada = fopen(nombre_fichero, "r");
    if (NULL == entrada)
    {
        fprintf(stderr, "No se ha podido abrir el fichero de ordenes %s\n", nombre_fichero);
        fflush(stderr);
        exit(1);
    }*/

    // Fin de cachito de Juanan

    /* Create the socket. */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1)
    {
        perror(cliente);
        fprintf(stderr, "%s: unable to create socket\n", cliente);
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
        fprintf(stderr, "%s: unable to bind socket\n", cliente);
        fflush(stderr);
        exit(1);
    }
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1)
    {
        perror(cliente);
        fprintf(stderr, "%s: unable to read socket address\n", cliente);
        fflush(stderr);
        exit(1);
    }

    // Cachito de Juanan.

    /*
    sfprintf(exitFileName, "%d.txt", myaddr_in.sin_port);
    salida = fopen(exitFileName, "w");
    if (NULL == salida)
    {
        fprintf(stderr, "El fichero de salida no se ha podido abrir\n");
        fflush(stderr);
        exit(1);
    }
    */

    // Fin de cachito de Juanan

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
        fprintf(stderr, "%s: No es posible resolver la IP de %s\n",
                cliente, servidor);
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
        fprintf(stderr, "%s: unable to register the SIGALRM signal\n", cliente);
        exit(1);
    }

    // Cachito de Juanan

    /*
    switch (fork())
    {
    case -1: // Unable to fork, for some reason. 
    perror(argv[0]);
    fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
    exit(1);
    case 0:
        do
        {
        n_retry = RETRIES;
        memset(buf, 0, BUFFERSIZE);
        fgets(buf, BUFFERSIZE - 2, entrada);
        fprintf("C: Sent: %s", buf);

        while (n_retry > 0)
        {
            tmp = 0;
            while (buf[tmp] != '\n' && buf[tmp] != '\0')
                tmp++;
            if (buf[tmp] != '\0')
                buf[tmp] = '\0';

            i = 0;
            while ('\n' != buf[i] && '\r' != buf[i] && '\0' != buf[i])
            {
                i++;
            }
            if ('\n' == buf[i])
            {
                buf[i] = '\r';
                buf[i + 1] = '\n';
            }

            if (sendto(s, buf, BUFFERSIZE, 0, (struct sockaddr *)&servaddr_in,
                       sizeof(struct sockaddr_in)) == -1)
            {
                perror(argv[0]);
                fprintf(stderr, "%s: unable to send request\n", argv[0]);
                exit(1);
            }
            else
            {
                n_retry = -99;
            }

            alarm(TIMEOUT);
            if (recvfrom(s, buf, BUFFERSIZE, 0,
                         (struct sockaddr *)&servaddr_in, &addrlen) == -1)
            {
                if (errno == EINTR)
                {
                    fprintf("attempt %d (retries %d).\n", n_retry, RETRIES);
                    n_retry--;
                }
                else
                {
                    fprintf("Unable to get response from");
                    exit(1);
                }
            }
            else
            {
                alarm(0);
                // Print out response. 
                if (reqaddr.s_addr == ADDRNOTFOUND)
                    fprintf("Host %s unknown by nameserver %s\n", nombre_fichero, servidor);
                break;
            }
        }
    } while (!feof(entrada));

    fclose(entrada);
    break;
default: // Father process. 
    while (0 != strncmp(buf, "QUIT", 4))
    {
        if (recv(s, buf, BUFFERSIZE, 0) != BUFFERSIZE)
        {
            perror("cliente");
            fprintf(stderr, "C: error recieving result\n");
            exit(1);
        }
        if (0 != strncmp(buf, "QUIT", 4))
            fprintf(salida, "%s\n", buf);
    }
    fprintf(salida, "Getting out. Bye!\n");

    fclose(salida);
    break;
};
*/

    // Fin de cachito de Juanan.

    n_retry = RETRIES;

    while (n_retry > 0)
    {
        /* Send the request to the nameserver. */
        if (sendto(s, "UDP", strlen("UDP"), 0, (struct sockaddr *)&servaddr_in,
                   sizeof(struct sockaddr_in)) == -1)
        {
            perror(cliente);
            fprintf(stderr, "%s: unable to send request\n", cliente);
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
                fprintf(stdout, "attempt %d (retries %d).\n", n_retry, RETRIES);
                n_retry--;
            }
            else
            {
                fprintf(stdout, "Unable to get response from");
                exit(1);
            }
        }
        else
        {
            alarm(0);
            /* Print out response. */
            if (reqaddr.s_addr == ADDRNOTFOUND)
                fprintf(stdout, "Host %s unknown by nameserver %s\n", "UDP", servidor);
            else
            {
                /* inet_ntop para interoperatividad con IPv6 */
                if (inet_ntop(AF_INET, &reqaddr, hostname, MAXHOST) == NULL)
                    perror(" inet_ntop \n");
                fprintf(stdout, "Address for %s is %s\n", "UDP", hostname);
            }
            break;
        }
    }

    if (n_retry == 0)
    {
        fprintf(stdout, "Unable to get response from");
        fprintf(stdout, " %s after %d attempts.\n", servidor, RETRIES);
    }

} // Fin UDP

void clienteTCP(char *cliente, char *servidor)
{
    int s; /* connected socket descriptor */
    struct addrinfo hints, *res;
    long timevar;                   /* contains time returned by time() */
    struct sockaddr_in myaddr_in;   /* for local socket address */
    struct sockaddr_in servaddr_in; /* for server socket address */
    int addrlen, i, j, errcode;
    /* This example uses TAM_BUFFER byte messages. */
    char buf[TAM_BUFFER];

    // Chachito de Juanan

    /*
    FILE *entrada, *salida;
    char buf[TAM_BUFFER];
    char exitFileName[100];

    int tmp;

    entrada = fopen(nombre_fichero, "r");
    if (NULL == entrada)
    {
        fprintf(stderr, "El fichero de ordenes %s no se ha podido abrir\n", nombre_fichero);
        fflush(stderr);
        exit(1);
    }
    */

    // Fin de cachito de Juanan.

    // Creamos el socket TCP local
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1)
    {
        perror(cliente);
        fprintf(stderr, "%s: unable to create socket\n", cliente);
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
        fprintf(stderr, "%s: No es posible resolver la IP de %s\n", cliente, servidor);
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
        fprintf(stderr, "%s: unable to connect to remote\n", cliente);
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
        fprintf(stderr, "%s: unable to read socket address\n", cliente);
        exit(1);
    }

    // Cachito de Juanan.

    /*
    sfprintf(exitFileName, "%d.txt", myaddr_in.sin_port);
    salida = fopen(exitFileName, "w");
    if (NULL == salida)
    {
        fprintf(stderr, "El fichero de salida no se ha podido abrir\n");
        exit(1);
    }
    */

    // Fin de cachito de Juanan.

    /* Print out a startup message for the user. */
    time(&timevar);
    /* The port number must be converted first to host byte
	 * order before printing.  On most hosts, this is not
	 * necessary, but the ntohs() call is included here so
	 * that this program could easily be ported to a host
	 * that does require it.
	 */
    fprintf(stdout, "Connected to %s on port %u at %s",
            servidor, ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));

    // Cachito de Juanan.

    /*
    switch (fork())
    {
    case -1: // Unable to fork, for some reason.
        perror("cliente");
        fprintf(stderr, "%s: unable to fork daemon\n", "cliente");
        exit(1);

    case 0:
        do
        {
            memset(buf, 0, TAM_BUFFER);
            fgets(buf, TAM_BUFFER - 2, entrada);

            tmp = 0;
            while (buf[tmp] != '\n' && buf[tmp] != '\0')
                tmp++;
            if (buf[tmp] != '\0')
                buf[tmp] = '\0';

            i = 0;
            while ('\n' != buf[i] && '\r' != buf[i] && '\0' != buf[i])
            {
                i++;
            }
            if ('\n' == buf[i])
            {
                buf[i] = '\r';
                buf[i + 1] = '\n';
            }

            if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
            {
                fprintf(stderr, "%s: Connection aborted on error", "client");
                fprintf(stderr, "on send %s\n", buf);
                exit(1);
            }
        } while (!feof(entrada));

        fclose(entrada);

        if (shutdown(s, 1) == -1)
        { //cerramos el socket porque ya no va a enviar nada más el cliente
            perror("client");
            fprintf(stderr, "%s: unable to shutdown socket\n", "client");
            exit(1);
        }
        break;
    default: // Father process.
        while (0 != strncmp(buf, "QUIT", 4))
        {
            if (recv(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
            {
                perror("cliente");
                fprintf(stderr, "C: error recieving result\n");
                exit(1);
            }
            if (0 != strncmp(buf, "QUIT", 4))
                fprintf(salida, "%s\n", buf);
        }
        fprintf(salida, "Getting out. Bye!\n");

        fclose(salida);
        break;
    }; 
    */

    // Fin de cachito de Juanan.

    /* PRUEBA COMANDO POST */
    char comando[TAM_COMANDO]; // Comando indica el comando que vas a enviar y buf recibe el codigo del servidor.

    printf("Escribe el comando que deseas enviar al servidor: ");
    fgets(comando, strlen(comando), stdin);

    //envio de datos
    if (send(s, comando, TAM_COMANDO, 0) != TAM_COMANDO)
    {
        fprintf(stderr, "%s: Connection aborted on error ", cliente);
        fprintf(stderr, "on send number %d\n", i);
        exit(1);
    }

    //terminacion de la fase de envío de datos
    if (shutdown(s, 1) == -1)
    {
        perror(cliente);
        fprintf(stderr, "%s: unable to shutdown socket\n", cliente);
        exit(1);
    }

    /* Con este codigo de aqui se recibe la respuesta al comando POST */
    i = recv(s, buf, TAM_BUFFER, 0);

    if (i == -1)
    {
        perror(cliente);
        fprintf(stderr, "%s: error reading result\n", cliente);
        exit(1);
    }

    /* FIN RESPUESTA COMANDO POST */

    //######## LIST ###########
    if ((strcmp(comando, "LIST\n")== 0)||(strcmp(comando, "list\n")== 0))
    {   //comprobacion de que funciona bien, luego borrar
        if (strcmp(buf, "215") == 0)
        { 
            printf("Recibiendo correctamente 215\n");
        }
    }
    //######## NEWGROUPS ###########
    else if ((strcmp(comando, "NEWGROUPS\n")== 0)||(strcmp(comando, "newgroups\n")== 0))
    {
        if (strcmp(buf, "231") == 0)
        { 
            printf("Recibiendo correctamente 231\n");

        }
    }
    //######## POST ###########
    else if ((strcmp(comando, "POST\n")== 0)||(strcmp(comando, "post\n")== 0))
    {
        if (strcmp(buf, "340") == 0)
        { // Se puede realizar el post.
            printf("340 Subiendo un articulo; finalice con una linea que solo contenga un punto\n");

            // Vamos a enviar bloques de 512 caracteres, hasta que pongamos un solo punto que indicará el fin de envío.
            while (strcmp(comando, ".\n") != 0)
            {
                fgets(comando, TAM_COMANDO, stdin);
                fprintf(stdout, "Se acaba de enviar : %s\n", comando);
                

                if (send(s, comando, TAM_COMANDO, 0) != TAM_COMANDO)
                {
                    fprintf(stderr, "%s: Connection aborted on error ", cliente);
                    fprintf(stderr, "on send number %d\n", i);
                    exit(1);
                }
            }
        }
        else
        { // Este else seria un else if con todos los demas codigos https://tools.ietf.org/html/rfc3977#section-6.3.1
            fprintf(stdout, "Received result number %s\n", buf);
            //printf("440 Posting no permitido\n");
        }
    }
    else
    { // Aqui irian los distintos comandos
        printf("No se ha recibido ningun comando conocido\n");
    }

    /* FIN PRUEBA COMANDO POST */
    
    /* 
    for (i=1; i<=5; i++) {
		*buf = i;
		if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
			fprintf(stderr, "%s: Connection aborted on error ",	cliente);
			fprintf(stderr, "on send number %d\n", i);
			exit(1);
		}
	}

		/* Now, shutdown the connection for further sends.
		 * This will cause the server to receive an end-of-file
		 * condition after it has received all the requests that
		 * have just been sent, indicating that we will not be
		 * sending any further requests.
		 */
    /*
    if (shutdown(s, 1) == -1) {
		perror(cliente);
		fprintf(stderr, "%s: unable to shutdown socket\n", cliente);
		exit(1);
	}

		/* Now, start receiving all of the replys from the server.
		 * This loop will terminate when the recv returns zero,
		 * which is an end-of-file condition.  This will happen
		 * after the server has sent all of its replies, and closed
		 * its end of the connection.
		 */
    /*
    while (i = recv(s, buf, TAM_BUFFER, 0)) {
		if (i == -1) {
            perror(cliente);
			fprintf(stderr, "%s: error reading result\n", cliente);
			exit(1);
		}
			/* The reason this while loop exists is that there
			 * is a remote possibility of the above recv returning
			 * less than TAM_BUFFER bytes.  This is because a recv returns
			 * as soon as there is some data, and will not wait for
			 * all of the requested data to arrive.  Since TAM_BUFFER bytes
			 * is relatively small compared to the allowed TCP
			 * packet sizes, a partial receive is unlikely.  If
			 * this example had used 2048 bytes requests instead,
			 * a partial receive would be far more likely.
			 * This loop will keep receiving until all TAM_BUFFER bytes
			 * have been received, thus guaranteeing that the
			 * next recv at the top of the loop will start at
			 * the begining of the next reply.
			 */
    /*
		while (i < TAM_BUFFER) {
			j = recv(s, &buf[i], TAM_BUFFER-i, 0);
			if (j == -1) {
                     perror(cliente);
			         fprintf(stderr, "%s: error reading result\n", cliente);
			         exit(1);
               }
			i += j;
		}
			/* Print out message indicating the identity of this reply. */
    /*fprintf(stdout, "Received result number %d\n", *buf);
	}

    /* Print message indicating completion of task. */
    time(&timevar);
    fprintf(stdout, "\n\nAll done at %s", (char *)ctime(&timevar));
} // Fin TCP