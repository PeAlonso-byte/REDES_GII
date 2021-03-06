/*
** Fichero: servidor.c
** Autores:
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

void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, char *buffer, struct sockaddr_in clientaddr_in);
void errout(char *); /* declare error out routine */

int FIN = 0; /* Para el cierre ordenado  */
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
		//fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
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
		//fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
		exit(1);
	}
	/* Initiate the listen on the socket so remote users
		 * can connect.  The listen backlog is set to 5, which
		 * is the largest currently supported.
		 */
	if (listen(ls_TCP, 5) == -1)
	{
		perror(argv[0]);
		//fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
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
		//fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
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
			//fprintf(stderr, "%s: unable to register the SIGCHLD signal\n", argv[0]);
			exit(1);
		}

		/* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
		vec.sa_handler = (void *)finalizar;
		vec.sa_flags = 0;
		if (sigaction(SIGTERM, &vec, (struct sigaction *)0) == -1)
		{
			perror(" sigaction(SIGTERM)");
			//fprintf(stderr, "%s: unable to register the SIGTERM signal\n", argv[0]);
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
	time_t now;
	int flagHeader = 0; // Tiene que valer 2 para que el header este completo.
	int flag = 0;		// 0 = HEADER; 1 = BODY
	int flagGrupo = 0;
	int flagN = 1;
	int flagB = 0;
	int reqcnt = 0;					/* keeps count of number of requests */
	char buf[TAM_BUFFER];			/* This example uses TAM_BUFFER byte messages. */
	char hostname[MAXHOST];			/* remote host's name string */
	char comando[TAM_COMANDO] = ""; // Para recibir comandos.
	int len, len1, status;
	struct hostent *hp; /* pointer to host info for remote host */
	long timevar;		/* contains time returned by time() */
	FILE *f, *grupos, *noticia;
	FILE *fLog;
	int num_lineas = 0;
	char linea[TAM_COMANDO];
	char lineanoticia[TAM_COMANDO], lineanoticia2[TAM_COMANDO], lineanoticia3[TAM_COMANDO];
	char header[TAM_COMANDO];
	char body[TAM_COMANDO * 5]; // Solo se van a poder enviar 5 lineas de body.
	struct linger linger;		/* allow a lingering, graceful close; */
								/* used when setting SO_LINGER */
	char *token, *token2, *token5, *token6;
	int token3, token4;
	char *separator, *separator1, *separator2, *separator5;
	int separator3, separator4;
	char *grusub, *grupo, *subgrupo, *gruponews;
	char ruta[100];
	char grupoaux[100];
	char grupoauxnews[100];
	char temaux[100];
	char *grusub1, *grupo1, *subgrupo1;
	char *sepnoticia, *tema, *numeroId;
	int fechanoticia, horanoticia;
	char comandoaux[TAM_COMANDO];
	char lineaux[TAM_COMANDO];
	char messageID[TAM_COMANDO];
	char grupo_aux[TAM_COMANDO];
	char num_aux[12];
	int flagExisteGrupo = 0;
	int contador = 0;
	int n;
	// NEWGROUPS
	int flagError = 0;
	char newFecha[10];
	char newHora[10];
	char grupoysub[100];
	char new_n_art[11];
	char new_n_art2[11];
	char n_fecha[11];
	char n_hora[11];
	char n_desc[100];
	int flagId = 0;
	char grupoaux2[100];
	int flagBody = 0;
	int flagNews = 0;
	int flagControl = 0;
	char *hora;

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

	fLog = fopen("nntpd.log", "a");
	if (fLog == NULL)
	{
		//fprintf(stdout, "Error al abrir el fichero nntpd.log\n");
		exit(1);
	}
	time(&timevar);
	hora = (char *)ctime(&timevar);
	hora[strlen(hora) - 1] = '\0';
	fprintf(fLog, "Startup from %s port %u at %s : TCP\n", hostname, ntohs(clientaddr_in.sin_port), hora);

	//printf("Startup from %s port %u at %s", hostname, ntohs(clientaddr_in.sin_port), hora);

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
		hora = (char *)ctime(&timevar);
		hora[strlen(hora) - 1] = '\0';
		fprintf(fLog, "C: %s %s", hora, comando);

		//######## LIST ###########
		if ((strcmp(comando, "LIST\r\n") == 0) || (strcmp(comando, "list\r\n") == 0))
		{
			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				//printf("No se ha podido leer el fichero grupos");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s No se ha podido leer el fichero grupos\n", hora);
				strcpy(buf, "100\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
			}
			else
			{
				strcpy(buf, "215\r\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: 215 %s listado de los grupos en formato <nombre> <ultimo> <primero> <fecha> <descripcion>\n", hora);
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);

				while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
				{
					if (send(s, linea, TAM_COMANDO, 0) != TAM_COMANDO)
						errout(hostname);

					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s %s\n", hora, linea);
				}
				if (send(s, ".\r\n", TAM_COMANDO, 0) != TAM_COMANDO) // Enviamos el . para indicar el fin de grupos.
					errout(hostname);
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s .\n", hora);
				fclose(grupos);
			}

		} //######## NEWGROUPS ###########
		else if ((strncmp(comando, "NEWGROUPS", 9) == 0) || (strncmp(comando, "newgroups", 9) == 0))
		{
			memset(grupoysub, '\0', sizeof(grupoysub));
			memset(n_desc, '\0', sizeof(n_desc));
			memset(new_n_art, '\0', sizeof(new_n_art));
			memset(new_n_art2, '\0', sizeof(new_n_art2));
			memset(n_fecha, '\0', sizeof(n_fecha));
			memset(n_hora, '\0', sizeof(n_hora));
			memset(comandoaux, '\0', sizeof(comandoaux));

			flagError = 0;
			time(&timevar);
			hora = (char *)ctime(&timevar);
			hora[strlen(hora) - 1] = '\0';
			fprintf(fLog, "S: %s %s", hora, comando);
			//fprintf(stdout, "Servidor recibe: %s\n", comando);
			int longitudComando = strlen(comando);
			comando[longitudComando - 1] = '\0';
			comando[longitudComando - 2] = '\0';
			comando[longitudComando] = '\0';

			token = strtok(comando, " ");
			if (token != NULL) //no hace falta comprobar el contenido porque si has entrado aqui ya sabes que es porque has introducido "newgroups"
			{
				token = strtok(NULL, " ");
				if ((strlen(token) == 6) && token != NULL)
				{
					//fecha
					strcpy(newFecha, token);
					token = strtok(NULL, " ");
					if ((strlen(token) == 6) && token != NULL) //ya que ahora lleva hora\n
					{
						//hora
						strcpy(newHora, token);
					}
					else
					{
						flagError = 1;
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newgroups> <YYMMDD> <HHMMSS>\n", hora);
					}
				}
				else
				{
					flagError = 1;
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newgroups> <YYMMDD> <HHMMSS>\n", hora);
				}
			}
			else
			{
				flagError = 1;
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newgroups> <YYMMDD> <HHMMSS>\n", hora);
			}

			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				strcpy(buf, "100\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				//printf("No se ha podido leer el fichero grupos");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s No se ha podido leer el fichero grupos\n", hora);
			}

			// Quiere decir que esta todo correcto .
			else if (flagError == 1) // Asi envia 1 sola vez el codigo de error.
			{
				strcpy(buf, "501\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
			}
			else
			{
				strcpy(buf, "231\r\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 231 Nuevos grupos desde dia %s %s\n", hora, newFecha, newHora);
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);

				sprintf(comandoaux, "231 Nuevos grupos desde %s %s\n", newFecha, newHora);
				if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
					errout(hostname);

				fprintf(fLog, "231 Nuevos grupos desde %s %s", newFecha, newHora);
				time(&timevar);

				while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
				{
					memset(lineaux, '\0', sizeof(lineaux));
					strcpy(lineaux, linea);

					separator = strtok(linea, " ");
					strcpy(grupoysub, separator); // Grupo y subgrupo

					separator = strtok(NULL, " ");
					strcpy(new_n_art, separator); // Ultimo

					separator = strtok(NULL, " ");
					strcpy(new_n_art2, separator); // primero

					separator = strtok(NULL, " ");
					strcpy(n_fecha, separator); // dia

					separator = strtok(NULL, " ");
					strcpy(n_hora, separator); // hora

					//si la fecha es mayor, da igual la hora
					if (atoi(n_fecha) > atoi(newFecha))
					{
						if (send(s, lineaux, TAM_COMANDO, 0) != TAM_COMANDO)
							errout(hostname);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s %s\n", hora, lineaux);
					}
					//si la fecha es la misma, compruebo la hora
					if (n_fecha == newFecha && n_hora >= newHora)
					{
						if (send(s, lineaux, TAM_COMANDO, 0) != TAM_COMANDO)
							errout(hostname);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s %s\n", hora, lineaux);
					}
				}
				//printf(".\n");
				if (send(s, ".\r\n", TAM_COMANDO, 0) != TAM_COMANDO)
					errout(hostname);

				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s .\n", hora);
				fclose(grupos);
			}
		}
		//######## NEWNEWS ###########
		else if ((strncmp(comando, "NEWNEWS", 7) == 0) || (strncmp(comando, "newnews", 7) == 0))
		{
			flagError = 0;
			flagNews = 0;
			flagControl = 0;
			memset(n_fecha, '\0', sizeof(n_fecha));
			memset(n_hora, '\0', sizeof(n_hora));
			memset(comandoaux, '\0', sizeof(comandoaux));
			int longitudComando = strlen(comando);
			comando[longitudComando] = '\0';
			comando[longitudComando - 1] = '\0';
			comando[longitudComando - 2] = '\0';
			token = strtok(comando, " ");
			if (token != NULL) //no hace falta comprobar el contenido porque si has entrado aqui ya sabes que es porque has introducido "newnews"
			{
				//"newnews"
				token2 = token;
				token = strtok(NULL, " ");
				if (token != NULL)
				{
					//grupo.subgrupo de noticias
					gruponews = token;
					strcpy(grupoauxnews, gruponews);

					token = strtok(NULL, " ");
					if ((strlen(token) == 6))
					{
						//fecha
						strcpy(n_fecha, token);
						token3 = atoi(token);
						//printf("%d\n", token3);
						token = strtok(NULL, " ");
						if ((strlen(token) == 6))
						{
							//hora
							strcpy(n_hora, token);
							token4 = atoi(token);
							//printf("%d\n", token4);
						}
						else
						{
							flagError = 1;
							//printf("\n501 Error de sintaxis en la hora. ");
							//printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
							//time(&timevar);
							//fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n", hora);
						}
					}
					else
					{
						flagError = 1;
						//printf("\n501 Error de sintaxis en la fecha. ");
						//printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
						//time(&timevar);
						//fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n", hora);
					}
				}
				else
				{
					flagError = 1;
					//printf("\n501 Error de sintaxis en el grupo de noticias. ");
					//printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
					//time(&timevar);
					//fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n", hora);
				}
			}
			else
			{
				flagError = 1;
				//printf("\n501 Error de sintaxis. ");
				//printf("Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n");
				//time(&timevar);
				//fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n", hora);
			}

			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				strcpy(buf, "100\r\n");
				//printf("No se ha podido leer el fichero grupos");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s No se ha podido leer el fichero grupos\n", hora);
			}
			else if (flagError == 1)
			{
				strcpy(buf, "501\r\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <newnews> <grupo_noticias> <YYMMDD> <HHMMSS>\n", hora);
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
			}
			else // Aqui ya hemos comprobado que no hay ningun error de sintaxis
			{
				while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
				{
					separator = strtok(linea, " ");
					if (separator != NULL)
					{
						//nombre
						grusub = separator;
						//printf("\nNombre grupo.subgrupo fichero:%s\n", separator2);
					}
					separator = strtok(NULL, " ");
					if (separator != NULL)
					{
						//ultimo
						separator3 = atoi(separator);
						//printf("\nNumero de articulos:%d\n", separator3);
					}

					if ((strcmp(gruponews, grusub) == 0))
					{
						flagNews = 1;
						break;
					}
				}
				fclose(grupos);

				if (flagNews == 1) // Quiere decir que existe el grupo
				{
					for (int i = 0; i < strlen(grusub); i++)
					{
						if (grusub[i] == '.')
						{
							grusub[i] = '/';
						}
					}

					strcpy(buf, "230\r\n");
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 230 Nuevos articulos desde dia %s %s\n", hora, n_fecha, n_hora); // Enviamos el primer comando
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);

					/*char aux[10];
					for (int i = 0, j = 0; i < strlen(n_fecha) + 2; i++, j++)
					{
						if (i % 2 == 0 && i != 0)
						{

							aux[j] = '/';
							i--;
						}
						aux[j] = n_fecha[i];
					}*/

					//fprintf(stdout, "La fecha es: %s", aux);
					sprintf(comandoaux, "230 Nuevos articulos desde %s %s\n", n_fecha, n_hora);
					fprintf(fLog, "S: %s %s", hora, comandoaux); // Enviamos el primer comando
					if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
						errout(hostname);

					for (int i = 1; i <= separator3; i++)
					{
						flagControl = 0;
						memset(lineanoticia, '\0', sizeof(lineanoticia));
						memset(temaux, '\0', sizeof(temaux));
						memset(comandoaux, '\0', sizeof(comandoaux));
						sprintf(ruta, "./noticias/articulos/%s/%d", grusub, i);

						//fprintf(stdout, "La ruta es : %s\n", ruta);
						noticia = fopen(ruta, "rt");
						if (noticia == NULL)
						{
							//printf("No se ha podido leer el fichero de la noticia");
							time(&timevar);
							hora = (char *)ctime(&timevar);
							hora[strlen(hora) - 1] = '\0';
							fprintf(fLog, "S: %s No se ha podido leer el fichero de la noticia\n", hora);
						}
						else
						{
							while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
							{
								sepnoticia = strtok(lineanoticia, " ");
								if (sepnoticia != NULL)
								{
									//subject /* Enviar al cliente linea por linea */
									if ((strcmp(sepnoticia, "Subject:") == 0))
									{
										sepnoticia = strtok(NULL, "\n");
										if (sepnoticia != NULL)
										{
											//tema
											tema = sepnoticia;
											strcpy(temaux, tema);
											//printf("\nTema del articulo: %s\n", tema);
											//time(&timevar);
											//fprintf(fLog, "S: %s Tema del articulo: %s\n", hora, tema);
											flagControl++;
										}
									}
									//fecha
									if ((strcmp(sepnoticia, "Date:") == 0))
									{
										sepnoticia = strtok(NULL, " ");
										if (sepnoticia != NULL)
										{
											//fecha noticia
											fechanoticia = atoi(sepnoticia);
											//printf("\nFecha del articulo:%d\n", fechanoticia);
											//time(&timevar);
											//fprintf(fLog, "S: %s Fecha del articulo:%d\n", hora, fechanoticia);
											flagControl++;
										}
										sepnoticia = strtok(NULL, " ");
										if (sepnoticia != NULL)
										{
											//hora noticia
											horanoticia = atoi(sepnoticia);
											//printf("\nHora del articulo:%d\n", horanoticia);
											//time(&timevar);
											//fprintf(fLog, "S: %s Hora del articulo:%d\n", hora, horanoticia);
											flagControl++;
										}
									}
									//id
									if ((strcmp(sepnoticia, "Message-ID:") == 0))
									{
										sepnoticia = strtok(NULL, " ");
										if (sepnoticia != NULL)
										{
											//id noticia
											numeroId = sepnoticia;
											//printf("\nID del articulo:%s\n", numeroId);
											//time(&timevar);
											//fprintf(fLog, "S: %s ID del articulo:%s\n", hora, numeroId);
											flagControl++;
											break;
										}
									}
								}
							}
							fclose(noticia);
							if (flagControl == 4)
							{
								//si la fecha es mayor, me da igual la hora
								if (fechanoticia > token3)
								{
									sprintf(comandoaux, "Articulo numero: %d tema: %s ID: %s", i, temaux, numeroId);
									if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
										errout(hostname);

									time(&timevar);
									hora = (char *)ctime(&timevar);
									hora[strlen(hora) - 1] = '\0';
									fprintf(fLog, "S: %s %s", hora, comandoaux);
								}
								//si la fecha es la misma, compruebo la hora
								if (fechanoticia == token3 && horanoticia >= token4)
								{
									sprintf(comandoaux, "Articulo numero: %d tema: %s ID: %s", i, temaux, numeroId);
									if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
										errout(hostname);

									time(&timevar);
									hora = (char *)ctime(&timevar);
									hora[strlen(hora) - 1] = '\0';
									fprintf(fLog, "S: %s %s", hora, comandoaux);
								}
							}
							else
							{
								//fprintf(stdout, "El flagControl ha fallado: %d\n", flagControl);
							}
						}
					}
					if (send(s, ".\r\n", TAM_COMANDO, 0) != TAM_COMANDO) // Para de enviar mensajes.
						errout(hostname);
				}
				else
				{
					strcpy(buf, "430\r\n");
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 430 No se encuentra el articulo.\n", hora); // Enviamos el primer comando
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);
				}
			}

			fclose(grupos);
		}
		//######## GROUP ###########
		else if ((strncmp(comando, "GROUP", 5) == 0) || (strncmp(comando, "group", 5) == 0))
		{
			memset(comandoaux, '\0', sizeof(comandoaux));
			memset(grupoaux, '\0', sizeof(grupoaux));
			memset(grupoaux2, '\0', sizeof(grupoaux2));
			flagError = 0;
			int longitudComando = strlen(comando);
			comando[longitudComando - 1] = '\0';
			comando[longitudComando - 2] = '\0';
			comando[longitudComando] = '\0';
			token = strtok(comando, " ");
			if (token != NULL)
			{
				//"group"
				token2 = token;
				//printf("\n%s\n", token2);

				token = strtok(NULL, " ");
				if (token != NULL)
				{
					grupo = token; // En grupo tengo grupo.subgrupo que recibe del cliente.
					strcpy(grupoaux, grupo);
				}
				else
				{
					flagError = 1;
					//printf("\n501 Error de sintaxis. ");
					//printf("Uso: Uso: <group> <grupo_noticias>\n");
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <group> <grupo_noticias>\n", hora);
				}
			}
			else
			{
				flagError = 1;
				//printf("\n501 Error de sintaxis. ");
				//printf("Uso: <group> <grupo_noticias>\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <group> <grupo_noticias>\n", hora);
			}

			grupos = fopen("./noticias/grupos", "rt");
			if (grupos == NULL)
			{
				strcpy(buf, "100\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				//printf("No se ha podido leer el fichero grupos");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s No se ha podido leer el fichero grupos\n", hora);
			}
			else if (flagError == 1)
			{
				strcpy(buf, "501\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <group> <grupo_noticias>\n", hora);
			}
			else
			{
				flagGrupo = 0;
				while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
				{
					memset(lineaux, '\0', sizeof(lineaux));
					strcpy(lineaux, linea);
					separator = strtok(linea, " ");
					if (separator != NULL)
					{
						//nombre
						grupo1 = separator;
					}
					/*else
					{
						printf("El nombre del grupo esta vacio\n");
						time(&timevar);
						fprintf(fLog, "S: %s El nombre del grupo esta vacio\n", hora);
					}*/
					if ((strcmp(grupo, grupo1) == 0))
					{
						flagGrupo = 1;
						break;
					}
				}
				if (flagGrupo == 0)
				{
					strcpy(buf, "441\r\n");
					time(&timevar);
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);
					fprintf(fLog, "S: %s %s %s is unknown\n", hora, buf, grupo);
				}
				else
				{
					strcpy(buf, "211\r\n");
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s %s", hora, buf);
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);

					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 211 %s", hora, lineaux);
					if (send(s, lineaux, TAM_COMANDO, 0) != TAM_COMANDO)
						errout(hostname);
				}
				fclose(grupos);
			}
		}
		//######## ARTICLE ###########
		else if ((strncmp(comando, "ARTICLE", 7) == 0) || (strncmp(comando, "article", 7) == 0))
		{
			flagError = 0;
			flagId = 0;
			memset(comandoaux, '\0', sizeof(comandoaux));
			memset(lineaux, '\0', sizeof(lineaux));
			memset(grupoaux2, '\0', sizeof(grupoaux2));

			token = strtok(comando, " ");
			if (token != NULL)
			{
				//"article"
				token2 = token;
				token = strtok(NULL, " ");
				if (token != NULL)
				{
					//numero articulo
					token3 = atoi(token);
					//printf("El token es : %d\n", token3);
				}
				else
				{
					//printf("\n501 Error de sintaxis. ");
					//printf("Uso: Uso: <article> <numero_articulo>\n");
					flagError = 1;
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <article> <numero_articulo>\n", hora);
				}
			}
			else
			{
				//printf("\n501 Error de sintaxis. ");
				//printf("Uso: <article> <numero_articulo>\n");
				flagError = 1;
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <article> <numero_articulo>\n", hora);
			}
			if (flagError == 1) // Si esta mal escrito.
			{
				strcpy(buf, "501\r\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <article> <numero_articulo>\n", hora);
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
			}
			else if (flagGrupo == 0) // Si no existe el articulo.
			{
				strcpy(buf, "430\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				//printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 430 No se encuentra ese articulo\n", hora);
			}
			else
			{
				strcpy(grupoaux2, grupoaux);
				grupo1 = strtok(grupoaux2, ".");
				grusub = grupo1;

				grupo1 = strtok(NULL, ".");
				grusub1 = grupo1;

				sprintf(ruta, "./noticias/articulos/%s/%s/%d", grusub, grusub1, token3);
				//printf("%s", ruta);
				noticia = fopen(ruta, "rt");

				if (noticia == NULL)
				{
					strcpy(buf, "423\r\n");
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);
					//printf("423 No existe ese articulo en %s.%s\n", grusub, grusub1);
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 423 No existe ese articulo en %s.%s\n", hora, grusub, grusub1);
				}
				else
				{
					while (fgets(linea, TAM_COMANDO, (FILE *)noticia))
					{
						strcpy(lineaux, linea);
						sepnoticia = strtok(linea, " ");
						if (sepnoticia != NULL)
						{
							if ((strcmp(sepnoticia, "Message-ID:") == 0))
							{
								sepnoticia = strtok(NULL, " ");
								if (sepnoticia != NULL)
								{
									//<numero@nogal.usal.es>
									flagId = 1;
									int longitudNoticia = strlen(sepnoticia);
									sepnoticia[longitudNoticia - 1] = '\0'; // quitar el /n
									sepnoticia[longitudNoticia - 2] = '\0'; // quitar el /r
									numeroId = sepnoticia;
									break;
								}
							}
						}
					}
					if (flagId == 1)
					{
						strcpy(buf, "223\r\n");
						if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
							errout(hostname);
						//printf("\n223 %0.10d %s articulo recuperado\n\n", token3, numeroId);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 223 %d %s recuperado \n", hora, token3, numeroId);
						sprintf(comandoaux, "223 %d %s articulo recuperado\n", token3, numeroId);
						if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
							errout(hostname);

						rewind(noticia);
						while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
						{
							if (send(s, lineanoticia, TAM_COMANDO, 0) != TAM_COMANDO)
								errout(hostname);

							time(&timevar);
							hora = (char *)ctime(&timevar);
							hora[strlen(hora) - 1] = '\0';
							fprintf(fLog, "S: %s %s\n", hora, lineanoticia);

							if (strcmp(lineanoticia, ".\r\n") == 0)
							{
								//break;
							}
						}
					}
					else
					{ // 430
						//printf("No has llamado a grupo en id\n");
						strcpy(buf, "430\r\n");
						if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
							errout(hostname);
						//printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 430 No se encuentra ese articulo\n", hora);
					}
					fclose(noticia);
					memset(comandoaux, '\0', sizeof(comandoaux));
				}
			}
		}
		//######## HEAD ###########
		else if ((strncmp(comando, "HEAD\r\n", 4) == 0) || (strncmp(comando, "head\r\n", 4) == 0))
		{
			flagError = 0;
			flagId = 0;
			memset(comandoaux, '\0', sizeof(comandoaux));
			memset(lineaux, '\0', sizeof(lineaux));
			memset(grupoaux2, '\0', sizeof(grupoaux2));

			token = strtok(comando, " ");
			if (token != NULL)
			{
				//"head"
				token2 = token;
				token = strtok(NULL, " ");
				if (token != NULL)
				{
					//numero articulo
					token3 = atoi(token);
					//printf("El token es : %d\n", token3);
				}
				else
				{
					//printf("\n501 Error de sintaxis. ");
					//printf("Uso: Uso: <article> <numero_articulo>\n");
					flagError = 1;
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <head> <numero_articulo>\n", hora);
				}
			}
			else
			{
				//printf("\n501 Error de sintaxis. ");
				//printf("Uso: <article> <numero_articulo>\n");
				flagError = 1;
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <head> <numero_articulo>\n", hora);
			}
			if (flagError == 1) // Si esta mal escrito.
			{
				strcpy(buf, "501\r\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <head> <numero_articulo>\n", hora);
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
			}
			else if (flagGrupo == 0) // Si no existe el articulo.
			{
				strcpy(buf, "430\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				//printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 430 No se encuentra ese articulo\n", hora);
			}
			else
			{
				strcpy(grupoaux2, grupoaux);
				grupo1 = strtok(grupoaux2, ".");
				grusub = grupo1;

				grupo1 = strtok(NULL, ".");
				grusub1 = grupo1;

				sprintf(ruta, "./noticias/articulos/%s/%s/%d", grusub, grusub1, token3);
				//printf("%s", ruta);
				noticia = fopen(ruta, "rt");

				if (noticia == NULL)
				{
					strcpy(buf, "423\r\n");
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);
					//printf("423 No existe ese articulo en %s.%s\n", grusub, grusub1);
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 423 No existe ese articulo en %s.%s\n", hora, grusub, grusub1);
				}
				else
				{
					while (fgets(linea, TAM_COMANDO, (FILE *)noticia))
					{
						strcpy(lineaux, linea);
						sepnoticia = strtok(linea, " ");
						if (sepnoticia != NULL)
						{
							if ((strcmp(sepnoticia, "Message-ID:") == 0))
							{
								sepnoticia = strtok(NULL, " ");
								if (sepnoticia != NULL)
								{
									//<numero@nogal.usal.es>
									flagId = 1;
									int longitudNoticia = strlen(sepnoticia);
									sepnoticia[longitudNoticia - 1] = '\0'; // quitar el /n
									sepnoticia[longitudNoticia - 2] = '\0'; // quitar el /r
									numeroId = sepnoticia;
									break;
								}
							}
						}
					}
					if (flagId == 1)
					{
						strcpy(buf, "221\r\n");
						if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
							errout(hostname);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 221 %d %s cabecera del articulo recuperada \n", hora, token3, numeroId);
						sprintf(comandoaux, "221 %d %s cabecera del articulo recuperada\n", token3, numeroId);
						if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
							errout(hostname);

						rewind(noticia);
						while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
						{
							if (send(s, lineanoticia, TAM_COMANDO, 0) != TAM_COMANDO)
								errout(hostname);

							time(&timevar);
							hora = (char *)ctime(&timevar);
							hora[strlen(hora) - 1] = '\0';
							fprintf(fLog, "S: %s %s\n", hora, lineanoticia);
							if (strncmp(lineanoticia, "\r\n", 2) == 0)
							{
								memset(lineanoticia, '\0', sizeof(lineanoticia));
								break;
							}
							memset(lineanoticia, '\0', sizeof(lineanoticia));
						}
					}
					else
					{ // 430
						//printf("No has llamado a grupo en id\n");
						strcpy(buf, "430\r\n");
						if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
							errout(hostname);
						//printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 430 No se encuentra ese articulo\n", hora);
					}
					fclose(noticia);
					memset(comandoaux, '\0', sizeof(comandoaux));
				}
			}
		}
		//######## BODY ###########
		else if ((strncmp(comando, "BODY\r\n", 4) == 0) || (strncmp(comando, "body\r\n", 4) == 0))
		{
			flagError = 0;
			flagId = 0;
			flagBody = 0;
			memset(comandoaux, '\0', sizeof(comandoaux));
			memset(lineaux, '\0', sizeof(lineaux));
			memset(grupoaux2, '\0', sizeof(grupoaux2));

			token = strtok(comando, " ");
			if (token != NULL)
			{
				//"head"
				token2 = token;
				token = strtok(NULL, " ");
				if (token != NULL)
				{
					//numero articulo
					token3 = atoi(token);
					//printf("El token es : %d\n", token3);
				}
				else
				{
					//printf("\n501 Error de sintaxis. ");
					//printf("Uso: Uso: <article> <numero_articulo>\n");
					flagError = 1;
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <body> <numero_articulo>\n", hora);
				}
			}
			else
			{
				//printf("\n501 Error de sintaxis. ");
				//printf("Uso: <article> <numero_articulo>\n");
				flagError = 1;
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <body> <numero_articulo>\n", hora);
			}
			if (flagError == 1) // Si esta mal escrito.
			{
				strcpy(buf, "501\r\n");
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <body> <numero_articulo>\n", hora);
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
			}
			else if (flagGrupo == 0) // Si no existe el articulo.
			{
				strcpy(buf, "430\r\n");
				if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
					errout(hostname);
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "S: %s 430 No se encuentra ese articulo\n", hora);
			}
			else
			{
				strcpy(grupoaux2, grupoaux);
				grupo1 = strtok(grupoaux2, ".");
				grusub = grupo1;

				grupo1 = strtok(NULL, ".");
				grusub1 = grupo1;

				sprintf(ruta, "./noticias/articulos/%s/%s/%d", grusub, grusub1, token3);
				//printf("%s", ruta);
				noticia = fopen(ruta, "rt");

				if (noticia == NULL)
				{
					strcpy(buf, "423\r\n");
					if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
						errout(hostname);
					//printf("423 No existe ese articulo en %s.%s\n", grusub, grusub1);
					time(&timevar);
					hora = (char *)ctime(&timevar);
					hora[strlen(hora) - 1] = '\0';
					fprintf(fLog, "S: %s 423 No existe ese articulo en %s.%s\n", hora, grusub, grusub1);
				}
				else
				{
					while (fgets(linea, TAM_COMANDO, (FILE *)noticia))
					{
						strcpy(lineaux, linea);
						sepnoticia = strtok(linea, " ");
						if (sepnoticia != NULL)
						{
							if ((strcmp(sepnoticia, "Message-ID:") == 0))
							{
								sepnoticia = strtok(NULL, " ");
								if (sepnoticia != NULL)
								{
									//<numero@nogal.usal.es>
									flagId = 1;
									int longitudNoticia = strlen(sepnoticia);
									sepnoticia[longitudNoticia - 1] = '\0'; // quitar el /n
									sepnoticia[longitudNoticia - 2] = '\0'; // quitar el /r
									numeroId = sepnoticia;
									break;
								}
							}
						}
					}
					if (flagId == 1)
					{
						strcpy(buf, "222\r\n");
						if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
							errout(hostname);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 222 %d %s cuerpo del articulo recuperado \n", hora, token3, numeroId);
						sprintf(comandoaux, "222 %d %s cuerpo del articulo recuperado\n", token3, numeroId);
						if (send(s, comandoaux, TAM_COMANDO, 0) != TAM_COMANDO)
							errout(hostname);

						rewind(noticia);
						while (fgets(lineanoticia, TAM_COMANDO, (FILE *)noticia))
						{

							if ((strcmp(lineanoticia, "\r\n") != 0) && flagBody == 0)
							{
								memset(lineanoticia, '\0', sizeof(lineanoticia));
								continue;
							}
							flagBody = 1;
							if (send(s, lineanoticia, TAM_COMANDO, 0) != TAM_COMANDO)
								errout(hostname);

							time(&timevar);
							hora = (char *)ctime(&timevar);
							hora[strlen(hora) - 1] = '\0';
							fprintf(fLog, "S: %s %s\n", hora, lineanoticia);

							memset(lineanoticia, '\0', sizeof(lineanoticia));
						}
					}
					else
					{ // 430
						//printf("No has llamado a grupo en id\n");
						strcpy(buf, "430\r\n");
						if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
							errout(hostname);
						//printf("\n423 El articulo %d no existe en el grupo de noticias\n", token3);
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 430 No se encuentra ese articulo\n", hora);
					}
					fclose(noticia);
					memset(comandoaux, '\0', sizeof(comandoaux));
				}
			}
		}
		//######## POST ###########
		else if ((strncmp(comando, "POST\r\n", 4) == 0) || (strncmp(comando, "post\r\n", 4) == 0))
		{
			flagExisteGrupo = 0;
			flagError = 0;
			flagNews = 0;
			flagControl = 0;
			n = 0;
			strcpy(buf, "340\r\n");
			time(&timevar);
			hora = (char *)ctime(&timevar);
			hora[strlen(hora) - 1] = '\0';
			fprintf(fLog, "S: %s 340 Subiendo un artículo; finalize con una línea que solo contenga un punto", hora);
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			memset(header, '\0', sizeof(header));
			memset(body, '\0', sizeof(body));
			memset(messageID, '\0', sizeof(messageID));
			memset(grupo_aux, '\0', sizeof(grupo_aux));
			memset(header, '\0', sizeof(header));
			memset(num_aux, '\0', sizeof(num_aux));
			memset(lineaux, '\0', sizeof(lineaux));
			memset(ruta, '\0', sizeof(ruta));
			memset(comandoaux, '\0', sizeof(comandoaux));

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
				time(&timevar);
				hora = (char *)ctime(&timevar);
				hora[strlen(hora) - 1] = '\0';
				fprintf(fLog, "C: %s %s", hora, comando);
				if (strncmp(comando, "\r\n", 2) == 0) // Si introducimos una linea en blanco querra decir que pasamos al body por lo que pasamos el flag de 0 a 1.
				{
					flag = 1;

					if (flagHeader == 2)
					{
						time(&now);
						char faux[200];
						char format_date[100];
						struct tm *local = localtime(&now);

						int hours = local->tm_hour;	 // get hours since midnight (0-23)
						int minutes = local->tm_min; // get minutes passed after the hour (0-59)
						int seconds = local->tm_sec; // get seconds passed after minute (0-59)

						int day = local->tm_mday;	   // get day of month (1 to 31)
						int month = local->tm_mon + 1; // get month of year (0 to 11)
						int year = local->tm_year + 1900;

						// Podemos cambiar el formato de la fecha si lo deseamos.
						strftime(format_date, 100, "%a, %d %b %Y %H:%M:%S (%Z)", local);
						sprintf(faux, "Date: %d%d%d %d%d%d %s\n", year, month, day, hours, minutes, seconds, format_date);
						strncat(header, faux, strlen(faux));
					}
				}
				if (flag == 0)
				{ // HEADER
					strcpy(comandoaux, comando);

					separator = strtok(comando, " ");
					if (separator == NULL)
					{
						time(&timevar);
						hora = (char *)ctime(&timevar);
						hora[strlen(hora) - 1] = '\0';
						fprintf(fLog, "S: %s 501 Error de sintaxis. Uso: <body> <numero_articulo>\n", hora);
						// Enviar codigo de error al cliente.
					}
					else if ((strncmp(separator, "Newsgroups:", strlen(separator)) == 0 || strncmp(separator, "newsgroups:", strlen(separator)) == 0) && flagHeader == 0)
					{
						separator = strtok(NULL, " "); // Avanzamos para ver el grupo que ha mandado el cliente.
						flagExisteGrupo = 0;

						grupos = fopen("./noticias/grupos", "rt"); // Leemos todos los grupos que existen.
						if (grupos == NULL)
						{
							//printf("No se ha podido leer el fichero grupos");
							fprintf(fLog, "S: %s No se ha podido leer el fichero grupos\n", hora);
							// Mandar mensaje de error al cliente.
						}
						while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
						{
							separator1 = strtok(linea, " "); // Grupo en el fichero

							if (strncmp(separator, separator1, strlen(separator1)) == 0)
							{
								strcpy(grupo_aux, separator1);
								flagExisteGrupo = 1;
								break;
							}
						}
						fclose(grupos);
						if (flagExisteGrupo == 1)
						{
							strncat(header, comandoaux, strlen(comandoaux));
							flagHeader++;
						}
						else
						{
							//printf("Grupo no encontrado\n");
						}
					}
					else if ((strncmp(comando, "Subject:", 8) == 0 || strncmp(comando, "subject:", 8) == 0) && flagHeader == 1)
					{

						strncat(header, comandoaux, strlen(comandoaux));
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
						//printf("Buffer del body lleno\n");
					}
					if (strncmp(comando, ".\r\n", 3) == 0)
					{

						num_lineas = 0;
						if (flagHeader != 2)
						{ // Quiere decir que el articulo no esta correcto por eso enviamos un 441. Habria que controlar en el cliente que pasa si recibe un 441

							strcpy(buf, "441\r\n");
							time(&timevar);
							hora = (char *)ctime(&timevar);
							hora[strlen(hora) - 1] = '\0';
							fprintf(fLog, "S: %s 441 No existe ese grupo de noticias", hora);
							if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
								errout(hostname);
						}
						else
						{
							/* Aqui tengo que ir a la ruta y escribir la noticia, tmb tengo que recoger el numero de n_articulos y aumentarlo. */
							/* Actualizamos n_articulos */

							f = fopen("./noticias/n_articulos", "r+");
							int num_art = 0;
							if (f == NULL)
							{
								//printf("Error al abrir el fichero.\n");
								fprintf(fLog, "S: %s Error al abrir el fichero.\n", hora);
							}
							fscanf(f, "%d", &num_art);
							num_art++;
							rewind(f);
							fprintf(f, "%d", num_art);
							fclose(f);

							/* Fin actualizar n_articulos.*/

							/* Añadimos el Message-ID: <X@nogal.usal.es> al header */

							sprintf(messageID, "Message-ID: <%d@%s>\n", num_art, hostname);
							strcat(header, messageID);

							/* Fin añadir Message-ID */

							/* Editar numero de articulos en el grupo y escribir */
							grupos = fopen("./noticias/grupos", "r+"); // Leemos todos los grupos que existen.
							if (grupos == NULL)
							{
								//printf("No se ha podido leer el fichero grupos\n");
								fprintf(fLog, "S: %s No se ha podido leer el fichero grupos\n", hora);
								// Mandar mensaje de error al cliente.
							}
							rewind(grupos);

							while (fgets(linea, TAM_COMANDO, (FILE *)grupos))
							{
								strcpy(lineaux, linea);

								separator1 = strtok(linea, " ");							 // Grupo en el fichero
								if (strncmp(grupo_aux, separator1, strlen(separator1)) == 0) // Estamos en la linea que queremos.
								{
									separator1 = strtok(NULL, " ");
									n = atoi(separator1);

									n++;
									sprintf(num_aux, "%.10d", n);

									int flagCopia = 0;
									for (int i = 0; i < strlen(lineaux); i++)
									{
										if (lineaux[i] == ' ' && flagCopia == 0)
										{
											for (int j = 0; j < strlen(num_aux); j++)
											{
												i++;
												lineaux[i] = num_aux[j];
											}

											flagCopia = 1;
										}
									}

									break;
								}
							}
							fseek(grupos, -(strlen(lineaux)), SEEK_CUR);
							fprintf(grupos, "%s", lineaux);
							fclose(grupos);
							/* Fin de editar numero de articulos y escribir.*/

							/* Creacion del articulo */
							for (int i = 0; i < strlen(grupo_aux); i++)
							{
								if (grupo_aux[i] == '.')
								{
									grupo_aux[i] = '/';
								}
							}
							sprintf(ruta, "./noticias/articulos/%s/%d", grupo_aux, n);

							f = fopen(ruta, "w");
							if (f == NULL)
							{
								//printf("Error al crear el fichero\n");
								fprintf(fLog, "S: %s Error al crear el fichero\n", hora);
							}
							fprintf(f, "%s%s", header, body);
							fclose(f);
							/* Fin de creacion del articulo */
							strcpy(buf, "240\r\n");
							time(&timevar);
							hora = (char *)ctime(&timevar);
							hora[strlen(hora) - 1] = '\0';
							fprintf(fLog, "S: %s 240 Articulo recibido correctamente\n", hora);
							if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
								errout(hostname);
						}
						flagHeader = 0;
						flag = 0;
						break;
					}
				}
			}
		} // ######### QUIT ##########
		else if ((strcmp(comando, "QUIT\r\n") == 0) || (strcmp(comando, "quit\r\n") == 0))
		{
			strcpy(buf, "205\r\n");
			time(&timevar);
			hora = (char *)ctime(&timevar);
			hora[strlen(hora) - 1] = '\0';
			fprintf(fLog, "S: %s 205 ADIOS!\n", hora);
			if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER)
				errout(hostname);

			break;
		}
		else
		{
			strcpy(buf, "500\r\n");
			time(&timevar);
			hora = (char *)ctime(&timevar);
			hora[strlen(hora) - 1] = '\0';
			fprintf(fLog, "S: %s %s", hora, buf);
			//printf("500 Comando no reconocido\n");
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
	//printf("Completed %s port %u, %d requests, at %s\n",
	//hostname, ntohs(clientaddr_in.sin_port), reqcnt, hora);

	fprintf(fLog, "Completed %s port %u, %d requests, at %s\n\n\n\n", hostname, ntohs(clientaddr_in.sin_port), reqcnt, hora);
	fclose(fLog);
}

/*
 *	This routine aborts the child process attending the client.
 */
void errout(char *hostname)
{
	//printf("Connection with %s aborted on error\n", hostname);
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
		//printf("%s: sendto error\n", "serverUDP");
		return;
	}
}
