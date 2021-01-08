/*
** Fichero: servidor.c
** Autores
** Pedro Luis Alonso Díez (72190545P)
** Esther Andrés Fraile (70918564L)
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

#define PUERTO 8564
#define ADDRNOTFOUND 0xffffffff /* return address for unfound host */
#define BUFFERSIZE 1024			/* maximum size of packets to be received */
#define TAM_BUFFER 10
#define MAXHOST 128
#define TAM_COMANDO 510
//#define TAM_NG 25

extern int errno;

/*
 *			M A I N
 *
 *	This routine starts the server.  It forks, leaving the child
 *	to do all the work, so it does not have to be run in the
 *	background.  It sets up the sockets.  It
 *	will loop forever, until killed by a signal.
 *
 */

//METODOS

void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, char *buffer, struct sockaddr_in clientaddr_in);
void errout(char *); /* declare error out routine */

int FIN = 0; /* Para el cierre ordenado */
void finalizar() { FIN = 1; }

int main(argc, argv) int argc;
char *argv[];
{

	int s_TCP, s_UDP; /* connected socket descriptor */
	int ls_TCP;		  /* listen socket descriptor */

	int cc; /* contains the number of bytes read */

	struct sigaction sa = {.sa_handler = SIG_IGN}; /* used to ignore SIGCHLD */

	struct sockaddr_in myaddr_in;	  /* for local socket address */
	struct sockaddr_in clientaddr_in; /* for peer socket address */
	int addrlen;

	fd_set readmask;
	int numfds, s_mayor;

	char buffer[BUFFERSIZE]; /* buffer for packets to be read into */

	struct sigaction vec;

	/* Create the listen socket. */
	ls_TCP = socket(AF_INET, SOCK_STREAM, 0);
	if (ls_TCP == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
		exit(1);
	}
	/* clear out address structures */
	memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));

	addrlen = sizeof(struct sockaddr_in);

	/* Set up address structure for the listen socket. */
	myaddr_in.sin_family = AF_INET;
	/* The server should listen on the wildcard address,
		 * rather than its own internet address.  This is
		 * generally good practice for servers, because on
		 * systems which are connected to more than one
		 * network at once will be able to have one server
		 * listening on all networks at once.  Even when the
		 * host is connected to only one network, this is good
		 * practice, because it makes the server program more
		 * portable.
		 */
	myaddr_in.sin_addr.s_addr = INADDR_ANY;
	myaddr_in.sin_port = htons(PUERTO);

	/* Bind the listen address to the socket. */
	if (bind(ls_TCP, (const struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
		exit(1);
	}
	/* Initiate the listen on the socket so remote users
		 * can connect.  The listen backlog is set to 5, which
		 * is the largest currently supported.
		 */
	if (listen(ls_TCP, 5) == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
		exit(1);
	}

	/* Create the socket UDP. */
	s_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_UDP == -1)
	{
		perror(argv[0]);
		printf("%s: unable to create socket UDP\n", argv[0]);
		exit(1);
	}
	/* Bind the server's address to the socket. */
	if (bind(s_UDP, (struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
	{
		perror(argv[0]);
		printf("%s: unable to bind address UDP\n", argv[0]);
		exit(1);
	}

	/* Now, all the initialization of the server is
		 * complete, and any user errors will have already
		 * been detected.  Now we can fork the daemon and
		 * return to the user.  We need to do a setpgrp
		 * so that the daemon will no longer be associated
		 * with the user's control terminal.  This is done
		 * before the fork, so that the child will not be
		 * a process group leader.  Otherwise, if the child
		 * were to open a terminal, it would become associated
		 * with that terminal as its control terminal.  It is
		 * always best for the parent to do the setpgrp.
		 */
	setpgrp();

	switch (fork())
	{
	case -1: /* Unable to fork, for some reason. */
		perror(argv[0]);
		fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
		exit(1);

	case 0: /* The child process (daemon) comes here. */

		/* Close stdin and stderr so that they will not
			 * be kept open.  Stdout is assumed to have been
			 * redirected to some logging file, or /dev/null.
			 * From now on, the daemon will not report any
			 * error messages.  This daemon will loop forever,
			 * waiting for connections and forking a child
			 * server to handle each one.
			 */
		fclose(stdin);
		fclose(stderr);

		/* Set SIGCLD to SIG_IGN, in order to prevent
			 * the accumulation of zombies as each child
			 * terminates.  This means the daemon does not
			 * have to make wait calls to clean them up.
			 */
		if (sigaction(SIGCHLD, &sa, NULL) == -1)
		{
			perror(" sigaction(SIGCHLD)");
			fprintf(stderr, "%s: unable to register the SIGCHLD signal\n", argv[0]);
			exit(1);
		}

		/* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
		vec.sa_handler = (void *)finalizar;
		vec.sa_flags = 0;
		if (sigaction(SIGTERM, &vec, (struct sigaction *)0) == -1)
		{
			perror(" sigaction(SIGTERM)");
			fprintf(stderr, "%s: unable to register the SIGTERM signal\n", argv[0]);
			exit(1);
		}

		while (!FIN)
		{
			/* Meter en el conjunto de sockets los sockets UDP y TCP */
			FD_ZERO(&readmask);
			FD_SET(ls_TCP, &readmask);
			FD_SET(s_UDP, &readmask);
			/* 
            Seleccionar el descriptor del socket que ha cambiado. Deja una marca en 
            el conjunto de sockets (readmask)
            */
			if (ls_TCP > s_UDP)
				s_mayor = ls_TCP;
			else
				s_mayor = s_UDP;

			if ((numfds = select(s_mayor + 1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0)
			{
				if (errno == EINTR)
				{
					FIN = 1;
					close(ls_TCP);
					close(s_UDP);
					perror("\nFinalizando el servidor. Se�al recibida en elect\n ");
				}
			}
			else
			{

				/* Comprobamos si el socket seleccionado es el socket TCP */
				if (FD_ISSET(ls_TCP, &readmask))
				{
					/* Note that addrlen is passed as a pointer
                     * so that the accept call can return the
                     * size of the returned address.
                     */
					/* This call will block until a new
    				 * connection arrives.  Then, it will
    				 * return the address of the connecting
    				 * peer, and a new socket descriptor, s,
    				 * for that connection.
    				 */
					s_TCP = accept(ls_TCP, (struct sockaddr *)&clientaddr_in, &addrlen);
					if (s_TCP == -1)
						exit(1);
					switch (fork())
					{
					case -1: /* Can't fork, just exit. */
						exit(1);
					case 0:			   /* Child process comes here. */
						close(ls_TCP); /* Close the listen socket inherited from the daemon. */
						serverTCP(s_TCP, clientaddr_in);
						exit(0);
					default: /* Daemon process comes here. */
							 /* The daemon needs to remember
        					 * to close the new accept socket
        					 * after forking the child.  This
        					 * prevents the daemon from running
        					 * out of file descriptor space.  It
        					 * also means that when the server
        					 * closes the socket, that it will
        					 * allow the socket to be destroyed
        					 * since it will be the last close.
        					 */
						close(s_TCP);
					}
				} /* De TCP*/
				/* Comprobamos si el socket seleccionado es el socket UDP */
				if (FD_ISSET(s_UDP, &readmask))
				{
					/* This call will block until a new
                * request arrives.  Then, it will
                * return the address of the client,
                * and a buffer containing its request.
                * BUFFERSIZE - 1 bytes are read so that
                * room is left at the end of the buffer
                * for a null character.
                */
					cc = recvfrom(s_UDP, buffer, BUFFERSIZE - 1, 0,
								  (struct sockaddr *)&clientaddr_in, &addrlen);
					if (cc == -1)
					{
						perror(argv[0]);
						printf("%s: recvfrom error\n", argv[0]);
						exit(1);
					}
					/* Make sure the message received is
                * null terminated.
                */
					buffer[cc] = '\0';
					serverUDP(s_UDP, buffer, clientaddr_in);
				}
			}
		} /* Fin del bucle infinito de atencion a clientes */
		/* Cerramos los sockets UDP y TCP */
		close(ls_TCP);
		close(s_UDP);

		printf("\nFin de programa servidor!\n");

	default: /* Parent process comes here. */
		exit(0);
	}
}

/*
 *				S E R V E R T C P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */
void serverTCP(int s, struct sockaddr_in clientaddr_in)
{
	int flagHeader = 0; // Tiene que valer 2 para que el header este completo.
	int flag = 0;		// 0 = HEADER; 1 = BODY
	int flagGrupo = 0;
	int reqcnt = 0;					/* keeps count of number of requests */
	char buf[TAM_BUFFER];			/* This example uses TAM_BUFFER byte messages. */
	char hostname[MAXHOST];			/* remote host's name string */
	char comando[TAM_COMANDO] = ""; // Para recibir comandos.
	int len, len1, status;
	struct hostent *hp; /* pointer to host info for remote host */
	long timevar;		/* contains time returned by time() */
	FILE *f, *grupos, *noticia;
	int num_lineas = 0;
	char linea[TAM_COMANDO];
	char lineanoticia[TAM_COMANDO], lineanoticia2[TAM_COMANDO];
	char header[TAM_COMANDO];
	char body[TAM_COMANDO * 5]; // Solo se van a poder enviar 5 lineas de body.
	struct linger linger;		/* allow a lingering, graceful close; */
								/* used when setting SO_LINGER */
	//char newgroups[80];
	char *token, *token2, *token5, *token6;
	int token3, token4;
	char *separator, *separator1, *separator2, *separator5;
	int separator3, separator4;
	char *grusub, *grupo, *subgrupo, *ruta;
	char *grusub1, *grupo1, *subgrupo1;
	char *sepnoticia, *sepnoticia2, *numeroId;
	int fechanoticia, horanoticia;

	int flagExisteGrupo = 0;

	/* Look up the host information for the remote host
	 * that we have connected with.  Its internet address
	 * was returned by the accept call, in the main
	 * daemon loop above.
	 */

	status = getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in),
						 hostname, MAXHOST, NULL, 0, 0);
	if (status)
	{
		/* The information is unavailable for the remote
			 * host.  Just format its internet address to be
			 * printed out in the logging information.  The
			 * address will be shown in "internet dot format".
			 */
		/* inet_ntop para interoperatividad con IPv6 */
		if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL)
			perror(" inet_ntop \n");
	}
	/* Log a startup message. */
	time(&timevar);
	/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
	//	f = fopen("nntpd.log", "a+");
	f = fopen("prueba.txt", "a+");
	if (f == NULL)
	{
		fprintf(stderr, "El fichero no se ha podido abrir\n");
		fflush(stderr);
		exit(1);
	}
	fprintf(f, "Startup from %s port %u at %s : TCP\n",
			hostname, ntohs(clientaddr_in.sin_port), (char *)ctime(&timevar));
	fclose(f);

	printf("Startup from %s port %u at %s",
		   hostname, ntohs(clientaddr_in.sin_port), (char *)ctime(&timevar));

	/* Set the socket for a lingering, graceful close.
		 * This will cause a final close of this socket to wait until all of the
		 * data sent on it has been received by the remote host.
		 */
	linger.l_onoff = 1;
	linger.l_linger = 1;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger,
				   sizeof(linger)) == -1)
	{
		errout(hostname);
	}

	/* Go into a loop, receiving requests from the remote
		 * client.  After the client has sent the last request,
		 * it will do a shutdown for sending, which will cause
		 * an end-of-file condition to appear on this end of the
		 * connection.  After all of the client's requests have
		 * been received, the next recv call will return zero
		 * bytes, signalling an end-of-file condition.  This is
		 * how the server will know that no more requests will
		 * follow, and the loop will be exited.
		 */

	while (len = recv(s, comando, TAM_COMANDO, 0))
	{
		if (len == -1)
			errout(hostname); /* error from recv */
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
			 * the begining of the next request.
			 */
		while (len < TAM_COMANDO)
		{
			len1 = recv(s, &comando[len], TAM_COMANDO - len, 0);
			if (len1 == -1)
				errout(hostname);
			len += len1;
		}
		/* Increment the request count. */
		reqcnt++;
		/* This sleep simulates the processing of the
			 * request that a real server might do.
			 */
		/* Send a response back to the client. */

		//######## LIST ###########
		if ((strcmp(comando, "LIST\r\n") == 0) || (strcmp(comando, "list\r\n") == 0))
		{
			printf("\n215 listado de los grupos en formato <nombre> <ultimo> <primero> <fecha> <descripcion>\n\n");
			strcpy(buf, "215");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				printf("No se ha podido leer el fichero grupos");
			}

			while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
			{
				printf("%s", linea);
			}
			printf(".\n");
			fclose(grupos);
		} //######## NEWGROUPS ###########
		else if ((strncmp(comando, "NEWGROUPS\r\n", 9) == 0) || (strncmp(comando, "newgroups\r\n", 9) == 0))
		{
			strcpy(buf, "231\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			recv(s, comando, TAM_COMANDO, 0);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);

			token = strtok(comando, " ");
			//TODO: no se compara el char * con el char
			//if ((strcmp(token, "NEWGROUPS") == 0)||(strcmp(token, "newgroups")))
			if (token != NULL)
			{
				//"newgroups"
				token2 = token;
				//printf("\n%s\n", token2);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
			}
			token = strtok(NULL, " ");
			if ((strlen(token) == 6))
			{
				//fecha
				token3 = atoi(token);
				//printf("%d\n", token3);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
			}
			token = strtok(NULL, " ");
			if ((strlen(token) == 6))
			{
				//hora
				token4 = atoi(token);
				//printf("%d\n", token4);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newgroups> <YYMMDD> <HHMMSS>\n");
			}

			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				printf("No se ha podido leer el fichero grupos");
			}

			printf("\n231 Nuevos grupos desde %.6d %.6d\n", token3, token4);
			while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
			{
				separator = strtok(linea, " ");
				separator2 = separator;
				if (separator2 != NULL)
				{
					//nombre
					//printf("\n%s\n", separator2);
				}
				else
				{
					printf("El nombre del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//ultimo
					//printf("%s\n", separator);
				}
				else
				{
					printf("El numero del ultimo articulo del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//primero
					//printf("%s\n", separator);
				}
				else
				{
					printf("El numero del primer articulo del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//dia
					separator3 = atoi(separator);
					//printf("%d\n", separator3);
				}
				else
				{
					printf("El dia del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//hora
					separator4 = atoi(separator);
					//printf("%d\n", separator4);
				}
				else
				{
					printf("La hora del grupo esta vacio\n");
				}

				//si la fecha es mayor, me da igual la hora
				if (separator3 > token3)
				{
					printf("%s\n", separator2);
				}
				//si la fecha es la misma, compruebo la hora
				if (separator3 == token3 && separator4 >= token4)
				{
					printf("%s\n", separator2);
				}
			}
			printf(".\n");
			fclose(grupos);
		}
		//######## NEWNEWS ###########
		else if ((strncmp(comando, "NEWNEWS\r\n", 7) == 0) || (strncmp(comando, "newnews\r\n", 7) == 0))
		{
			strcpy(buf, "230\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			recv(s, comando, TAM_COMANDO, 0);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);

			//trtok(NULL, " ");
			//if (tokentoken = strtok(comando, " ");
			token = strtok(comando, " ");
			//TODO: no se compara el char * con el char
			//if ((strcmp(token, "NEWNEWS") == 0)||(strcmp(token, "newnews")))
			if (token != NULL)
			{
				//"newnews"
				token2 = token;
				//printf("\n%s\n", token2);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
			}
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				//grupo.subgrupo de noticias
				token5 = token;
				token6 = token;
				//printf("%s\n", token5);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
			}
			token = strtok(NULL, " ");
			if ((strlen(token) == 6))
			{
				//fecha
				token3 = atoi(token);
				//printf("%d\n", token3);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
			}
			token = strtok(NULL, " ");
			if ((strlen(token) == 6))
			{
				//hora
				token4 = atoi(token);
				//printf("%d\n", token4);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
			}
			grusub1 = strtok(token6, ".");
			if (grusub1 != NULL)
			{
				//grupo
				grupo1 = grusub1;
				printf("\nNOMBRE GRUPO:%s\n", grupo1);
			}
			else
			{
				printf("El nombre del grupo esta vacio\n");
			}
			grusub1 = strtok(NULL, ".");
			if (grusub1 != NULL)
			{
				//subgrupo
				subgrupo1 = grusub1;
				printf("\nNOMBRE SUBGRUPO:%s\n", subgrupo1);
			}
			else
			{
				printf("El nombre del subgrupo esta vacio\n");
			}

			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				printf("No se ha podido leer el fichero grupos");
			}

			while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
			{
				separator = strtok(linea, " ");
				if (separator != NULL)
				{
					//nombre
					separator2 = separator;
					separator5 = separator;
					printf("\nNOMBRE GRUSUB:%s\n", separator2);
				}
				else
				{
					printf("El nombre del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//ultimo
					separator3 = atoi(separator);
					printf("\nNUMERO DE ARTICULOS:%d\n", separator3);
				}
				else
				{
					printf("El numero del ultimo articulo del grupo esta vacio\n");
				}
				grusub = strtok(separator5, ".");
				if (grusub != NULL)
				{
					//grupo
					grupo = grusub;
					printf("\nNOMBRE GRUPO:%s\n", grupo);
				}
				else
				{
					printf("El nombre del grupo esta vacio\n");
				}
				grusub = strtok(NULL, ".");
				if (grusub != NULL)
				{
					//subgrupo
					subgrupo = grusub;
					printf("\nNOMBRE SUBGRUPO:%s\n", subgrupo);
				}
				else
				{
					printf("El nombre del subgrupo esta vacio\n");
				}
				//TODO: no va la comparacion de char *
				if ((strcmp(grupo, grupo1) == 0) && (strcmp(subgrupo, subgrupo1) == 0))
				{
					for (int i = 1; i < separator3; i++)
					{
						sprintf(ruta, "./noticias/articulos/%s/%s/%d", grupo, subgrupo, i);
						printf("\n\nhola %s\n", ruta);
						noticia = fopen(ruta, "rt");
						if (noticia == NULL)
						{
							printf("No se ha podido leer el fichero de la noticia");
						}

						while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
						{
							sepnoticia = strtok(lineanoticia, " ");
							if (sepnoticia != NULL)
							{
								//nombre
								sepnoticia2 = sepnoticia;
								//printf("\n%s\n", sepnoticia2);
								if ((strcmp(sepnoticia2, "Date:") == 0))
								{
									sepnoticia = strtok(NULL, " ");
									if (sepnoticia != NULL)
									{
										//fecha noticia
										fechanoticia = atoi(sepnoticia);
										printf("\nFecha del articulo:%d\n", fechanoticia);
									}
									else
									{
										printf("Fecha del articulo vacia\n");
									}
									sepnoticia = strtok(NULL, " ");
									if (sepnoticia != NULL)
									{
										//hora noticia
										horanoticia = atoi(sepnoticia);
										printf("\nHora del articulo:%d\n", horanoticia);
									}
									else
									{
										printf("Hora del articulo vacia\n");
									}
								}
							}
							else
							{
								printf("El fichero noticia esta vacio\n");
							}

							//si la fecha es mayor, me da igual la hora
							if (fechanoticia > token3)
							{
								printf("\nArticulo perteneciente a: %s.%s numero: %d", grupo, subgrupo, i);
							}
							//si la fecha es la misma, compruebo la hora
							if (fechanoticia == token3 && horanoticia >= token4)
							{
								printf("\nArticulo perteneciente a: %s.%s numero: %d", grupo, subgrupo, i);
							}
						}
					}
				}
				else
				{
					printf("\n411 no existe ese grupo de noticias\n");
				}
			}

			fclose(grupos);
		}
		//######## GROUP ###########
		else if ((strncmp(comando, "GROUP\r\n", 5) == 0) || (strncmp(comando, "group\r\n", 5) == 0))
		{
			strcpy(buf, "211\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			recv(s, comando, TAM_COMANDO, 0);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);

			token = strtok(comando, " ");
			//TODO: no se compara el char * con el char
			//if ((strcmp(token, "GROUP") == 0)||(strcmp(token, "group")))
			if (token != NULL)
			{
				//"group"
				token2 = token;
				//printf("\n%s\n", token2);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <group> <grupo_noticias>\n");
			}
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				//grupo.subgrupo de noticias
				token5 = token;
				token6 = token;
				//printf("%s\n", token5);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: Uso: <group> <grupo_noticias>\n");
			}
			grusub1 = strtok(token6, ".");
			if (grusub1 != NULL)
			{
				//grupo
				grupo1 = grusub1;
				printf("\nNOMBRE GRUPO:%s\n", grupo1);
			}
			else
			{
				printf("El nombre del grupo esta vacio\n");
			}
			grusub1 = strtok(NULL, ".");
			if (grusub1 != NULL)
			{
				//subgrupo
				subgrupo1 = grusub1;
				printf("\nNOMBRE SUBGRUPO:%s\n", subgrupo1);
			}
			else
			{
				printf("El nombre del subgrupo esta vacio\n");
			}
			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				printf("No se ha podido leer el fichero grupos");
			}

			while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
			{
				separator = strtok(linea, " ");
				if (separator != NULL)
				{
					//nombre
					separator2 = separator;
					separator5 = separator;
					printf("\nNOMBRE GRUSUB:%s\n", separator2);
				}
				else
				{
					printf("El nombre del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//ultimo
					separator3 = atoi(separator);
					printf("\nUltimo articulo:%d\n", separator3);
				}
				else
				{
					printf("El numero del ultimo articulo del grupo esta vacio\n");
				}
				separator = strtok(NULL, " ");
				if (separator != NULL)
				{
					//primero
					separator4 = atoi(separator);
					printf("\nPrimer articulo:%d\n", separator4);
				}
				else
				{
					printf("El numero del primer articulo del grupo esta vacio\n");
				}
				grusub = strtok(separator5, ".");
				if (grusub != NULL)
				{
					//grupo
					grupo = grusub;
					printf("\nNOMBRE GRUPO:%s\n", grupo);
				}
				else
				{
					printf("El nombre del grupo esta vacio\n");
				}
				grusub = strtok(NULL, ".");
				if (grusub != NULL)
				{
					//subgrupo
					subgrupo = grusub;
					printf("\nNOMBRE SUBGRUPO:%s\n", subgrupo);
				}
				else
				{
					printf("El nombre del subgrupo esta vacio\n");
				}
				//TODO: no va la comparacion de char *
				if ((strcmp(grupo, grupo1) == 0) && (strcmp(subgrupo, subgrupo1) == 0))
				{
					printf("211 %d %d %d %s.%s", separator3, separator3, separator4, grupo, subgrupo);
					printf("\n\t(hay %d articulos, del %d al %d, en %s.%s\n", separator3, separator4, separator3, grupo, subgrupo);
					flagGrupo = 1;
				}
				else
				{
					printf("\n411 no existe ese grupo de noticias\n");
				}
			}

			fclose(grupos);
		}
		//######## ARTICLE ###########
		else if ((strncmp(comando, "ARTICLE\r\n", 7) == 0) || (strncmp(comando, "article\r\n", 7) == 0))
		{
			strcpy(buf, "223\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			recv(s, comando, TAM_COMANDO, 0);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);
			token = strtok(comando, " ");
			//TODO: no se compara el char * con el char
			//if ((strcmp(token, "GROUP") == 0)||(strcmp(token, "group")))
			if (token != NULL)
			{
				//"article"
				token2 = token;
				//printf("\n%s\n", token2);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <article> <numero_articulo>\n");
			}
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				//numero articulo
				token3 = atoi(token);
				//printf("%d\n", token3);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: Uso: <article> <numero_articulo>\n");
			}

			if (flagGrupo == 0)
			{
				printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
			}
			else
			{
				sprintf(ruta, "./noticias/articulos/%s/%s/%d", grupo, subgrupo, token3);
				noticia = fopen(ruta, "rt");
				if (noticia == NULL)
				{
					printf("No se ha podido leer el fichero de la noticia");
				}

				while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
				{
					for (int j = 0; j <= TAM_COMANDO; j++)
					{
						lineanoticia[j] = lineanoticia2[j];
					}

					sepnoticia = strtok(lineanoticia2, " ");
					if (sepnoticia != NULL)
					{
						//nombre
						sepnoticia2 = sepnoticia;
						//printf("\n%s\n", sepnoticia2);
						if ((strcmp(sepnoticia2, "Message-ID:") == 0))
						{
							sepnoticia = strtok(NULL, " ");
							if (sepnoticia != NULL)
							{
								//<numero@nogal.usal.es>
								numeroId = sepnoticia;
								printf("\nIdentificador %s\n", numeroId);
							}
							else
							{
								printf("ID del articulo vacio\n");
							}
						}
					}
					else
					{
						printf("El fichero noticia esta vacio\n");
					}

					printf("\n223 %d %s articulo recuperado\n", token3, numeroId);
					printf("%s", lineanoticia);
				}
			}
		}
		//######## HEAD ###########
		else if ((strncmp(comando, "HEAD\r\n", 4) == 0) || (strncmp(comando, "head\r\n", 4) == 0))
		{
			strcpy(buf, "221\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			recv(s, comando, TAM_COMANDO, 0);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);
			token = strtok(comando, " ");
			//TODO: no se compara el char * con el char
			//if ((strcmp(token, "GROUP") == 0)||(strcmp(token, "group")))
			if (token != NULL)
			{
				//"head"
				token2 = token;
				//printf("\n%s\n", token2);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <head> <numero_articulo>\n");
			}
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				//numero articulo
				token3 = atoi(token);
				//printf("%d\n", token3);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: Uso: <head> <numero_articulo>\n");
			}

			if (flagGrupo == 0)
			{
				printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
			}
			else
			{
				sprintf(ruta, "./noticias/articulos/%s/%s/%d", grupo, subgrupo, token3);
				noticia = fopen(ruta, "rt");
				if (noticia == NULL)
				{
					printf("No se ha podido leer el fichero de la noticia");
				}

				while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
				{
					for (int j = 0; j <= TAM_COMANDO; j++)
					{
						lineanoticia[j] = lineanoticia2[j];
					}

					sepnoticia = strtok(lineanoticia2, " ");
					if (sepnoticia != NULL)
					{
						//nombre
						sepnoticia2 = sepnoticia;
						//printf("\n%s\n", sepnoticia2);
						if ((strcmp(sepnoticia2, "Message-ID:") == 0))
						{
							sepnoticia = strtok(NULL, " ");
							if (sepnoticia != NULL)
							{
								//<numero@nogal.usal.es>
								numeroId = sepnoticia;
								printf("\nIdentificador %s\n", numeroId);
							}
							else
							{
								printf("ID del articulo vacio\n");
							}
						}
					}
					else
					{
						printf("El fichero noticia esta vacio\n");
					}

					printf("\n221 %d %s cabecera del articulo recuperada\n", token3, numeroId);
					//TODO: no hay que mostrar todo el fichero, solo las 4 primeras lineas (la cabecera)
					printf("%s", lineanoticia);
				}
			}
		}
		//######## BODY ###########
		else if ((strncmp(comando, "BODY\r\n", 4) == 0) || (strncmp(comando, "body\r\n", 4) == 0))
		{
			strcpy(buf, "222\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			recv(s, comando, TAM_COMANDO, 0);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);
			token = strtok(comando, " ");
			//TODO: no se compara el char * con el char
			//if ((strcmp(token, "GROUP") == 0)||(strcmp(token, "group")))
			if (token != NULL)
			{
				//"body"
				token2 = token;
				//printf("\n%s\n", token2);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: <body> <numero_articulo>\n");
			}
			token = strtok(NULL, " ");
			if (token != NULL)
			{
				//numero articulo
				token3 = atoi(token);
				//printf("%d\n", token3);
			}
			else
			{
				printf("\n501 Error de sintaxis. ");
				printf("Uso: Uso: <body> <numero_articulo>\n");
			}

			if (flagGrupo == 0)
			{
				printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
			}
			else
			{
				sprintf(ruta, "./noticias/articulos/%s/%s/%d", grupo, subgrupo, token3);
				noticia = fopen(ruta, "rt");
				if (noticia == NULL)
				{
					printf("No se ha podido leer el fichero de la noticia");
				}

				while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
				{
					for (int j = 0; j <= TAM_COMANDO; j++)
					{
						lineanoticia[j] = lineanoticia2[j];
					}

					sepnoticia = strtok(lineanoticia2, " ");
					if (sepnoticia != NULL)
					{
						//nombre
						sepnoticia2 = sepnoticia;
						//printf("\n%s\n", sepnoticia2);
						if ((strcmp(sepnoticia2, "Message-ID:") == 0))
						{
							sepnoticia = strtok(NULL, " ");
							if (sepnoticia != NULL)
							{
								//<numero@nogal.usal.es>
								numeroId = sepnoticia;
								printf("\nIdentificador %s\n", numeroId);
							}
							else
							{
								printf("ID del articulo vacio\n");
							}
						}
					}
					else
					{
						printf("El fichero noticia esta vacio\n");
					}

					printf("\n222 %d %s cuerpo del articulo recuperado\n", token3, numeroId);
					//TODO: no hay que mostrar todo el articulo solo el body
					printf("%s", lineanoticia);
				}
			}
		}
		//######## POST ###########
		else if ((strcmp(comando, "POST\r\n") == 0) || (strcmp(comando, "post\r\n") == 0))
		{
			printf("Se ha recibido un POST\n");
			strcpy(buf, "340\r\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			num_lineas = 0; // Para controlar que no podamos recibir mas de 5 lineas de body (Si lo hacemos con memoria dinamica sobra.)

			/* Aqui tenemos que empezar a recibir el POST entero: HEADER Y BODY */
			//while (strncmp(comando, ".\r\n", 3) != 0)
			//{

			while (len = recv(s, comando, TAM_COMANDO, 0))
			{

				if (len == -1)
					errout(hostname); // error from recv

				while (len < TAM_COMANDO)
				{
					len1 = recv(s, &comando[len], TAM_COMANDO - len, 0);
					if (len1 == -1)
						errout(hostname);
					len += len1;
				}
				// Increment the request count.
				reqcnt++;
				comando[len] = '\0';
				// This sleep simulates the processing of the request that a real server might do.

				if (strncmp(comando, "\r\n", 2) == 0) // Si introducimos una linea en blanco querra decir que pasamos al body por lo que pasamos el flag de 0 a 1.
				{
					flag = 1;
				}
				if (flag == 0)
				{ // HEADER
					separator = strtok(comando, ":");
					if (separator == NULL)
					{
						printf("Error sintaxis.");
						// Enviar codigo de error al cliente.
					}
					if ((strncmp(separator, "NEWSGROUPS", strlen(separator)) == 0 || strncmp(separator, "newsgroups", strlen(separator)) == 0) && flagHeader == 0)
					{
						separator = strtok(NULL, ":"); // Avanzamos para ver el grupo que ha mandado el cliente.
						flagExisteGrupo = 0;

						grupos = fopen("./noticias/grupos", "rt"); // Leemos todos los grupos que existen.
						if (grupos == NULL)
						{
							printf("No se ha podido leer el fichero grupos");
							// Mandar mensaje de error al cliente.
						}
						while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
						{
							separator1 = strtok(linea, " "); // Grupo en el fichero
							fprintf(stdout, "Se esta comparando : (c)%s -- > (s)%s\n", separator, separator1);
							if (strncmp(separator, separator1, strlen(separator1)) == 0)
							{
								flagExisteGrupo = 1;
								break;
							}
						}
						fclose(grupos);
						if (flagExisteGrupo == 1)
						{
							strncat(header, comando, strlen(comando));
							flagHeader++;
						}
						else
						{
							printf("Grupo no encontrado\n");
						}
						
					}
					else if ((strncmp(comando, "SUBJECT", 7) == 0 || strncmp(comando, "subject", 7) == 0) && flagHeader == 1)
					{
						strncat(header, comando, strlen(comando));
						flagHeader++;
					}
				}
				else
				{ // Esto es el body

					num_lineas++;
					if (num_lineas <= 5)
					{
						strncat(body, comando, strlen(comando));
					}
					else
					{
						printf("Buffer del body lleno\n");
					}
					if (strncmp(comando, ".\r\n", 3) == 0)
					{
						num_lineas = 0;
						if (flagHeader != 2)
						{ // Quiere decir que el header no esta correcto por eso enviamos un 441. Habria que controlar en el cliente que pasa si recibe un 441
							strcpy(buf, "441\r\n");
							if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
								errout(hostname);
							
						}
						else
						{
							strcpy(buf, "240\r\n");
							if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
								errout(hostname);
							
						}
						flagHeader = 0;
					}
				}
			}
			//}
		} // ######### QUIT ########## Creo que es innecesario en el servidor.s
		else if ((strcmp(comando, "QUIT\r\n") == 0) || (strcmp(comando, "quit\r\n") == 0))
		{
			// Falta por implementar.
			// Enviar mensaje de salida del cliente.
		}
		else
		{
			strcpy(buf, "440\r\n");
			printf("500 Comando no reconocido\n");
			//printf("No se ha recibido un POST\n");
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);
		}
	}

	/* The loop has terminated, because there are no
		 * more requests to be serviced.  As mentioned above,
		 * this close will block until all of the sent replies
		 * have been received by the remote host.  The reason
		 * for lingering on the close is so that the server will
		 * have a better idea of when the remote has picked up
		 * all of the data.  This will allow the start and finish
		 * times printed in the log file to reflect more accurately
		 * the length of time this connection was used.
		 */
	close(s);

	/* Log a finishing message. */
	time(&timevar);
	/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
	printf("Completed %s port %u, %d requests, at %s\n",
		   hostname, ntohs(clientaddr_in.sin_port), reqcnt, (char *)ctime(&timevar));
}

/*
 *	This routine aborts the child process attending the client.
 */
void errout(char *hostname)
{
	printf("Connection with %s aborted on error\n", hostname);
	exit(1);
}

/*
 *				S E R V E R U D P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */
void serverUDP(int s, char *buffer, struct sockaddr_in clientaddr_in)
{
	struct in_addr reqaddr; /* for requested host's address */
	struct hostent *hp;		/* pointer to host info for requested host */
	int nc, errcode;

	struct addrinfo hints, *res;

	int addrlen;

	addrlen = sizeof(struct sockaddr_in);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	/* Treat the message as a string containing a hostname. */
	/* Esta funci�n es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta. */
	errcode = getaddrinfo(buffer, NULL, &hints, &res);
	if (errcode != 0)
	{
		/* Name was not found.  Return a
		 * special value signifying the error. */
		reqaddr.s_addr = ADDRNOTFOUND;
	}
	else
	{
		/* Copy address of host into the return buffer. */
		reqaddr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
	}
	freeaddrinfo(res);

	nc = sendto(s, &reqaddr, sizeof(struct in_addr),
				0, (struct sockaddr *)&clientaddr_in, addrlen);
	if (nc == -1)
	{
		perror("serverUDP");
		printf("%s: sendto error\n", "serverUDP");
		return;
	}
}
