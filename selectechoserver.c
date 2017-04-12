#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>

#define	QLEN			5
#define	BUFSIZE			2048

int passivesock( char *service, char *protocol, int qlen, int *rport );

/*
**	The server ... 
*/

typedef struct {
	char *tagName;
	int userfd;
}tag;

int
main( int argc, char *argv[] )
{
	char			buf[BUFSIZE];
	char			*service;
	struct sockaddr_in	fsin;
	int			msock;
	int			ssock;
	fd_set			rfds;
	fd_set			afds;
	int			alen;
	int			fd;
	int			nfds;
	int			rport = 0;
	int			cc;
	tag *tags; // Why can't I use just an array, instead of linked-list?
	
	switch (argc) 
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	
	nfds = getdtablesize();

	FD_ZERO(&afds);
	FD_SET( msock, &afds );
	for (;;)
	{
		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}

		/*	Handle the main socket - a new guy has checked in  */
		if (FD_ISSET( msock, &rfds)) 
		{
			int	ssock;

			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}

			/* start listening to this guy */
			FD_SET( ssock, &afds );
		}

		/*	Handle the participants requests  */
		for ( fd = 0; fd < nfds; fd++ )
		{
			if (fd != msock && FD_ISSET(fd, &rfds))
			{
				if ( (cc = read( fd, buf, BUFSIZE )) <= 0 )
				{
					printf( "The client has gone.\n" );
					(void) close(fd);
					FD_CLR( fd, &afds );

				}
				else
				{
					buf[cc] = '\0';
					printf( "The client%d says: %s\n", fd, buf );
					
					char *response, *tag;
					char *cmd = strtok(buf, " ");

					/* Understand what that message means */
					if (strcmp(cmd, "REGISTERALL\r\n") == 0) {
						response = "Ok! Registered all.\n";
						write(fd, response, strlen(response));
					}
					else if (strcmp(cmd, "DEREGISTERALL\r\n") == 0) {
						response = "Ok! Deregistered all.\n";
						write(fd, response, strlen(response));
					}
					else if (strcmp(cmd, "REGISTER") == 0) {
						tag = strtok(NULL, " ");
						response = strcat(tag, " registered.\n");
						write(fd, response, strlen(response));
					}
					else if (strcmp(cmd, "DEREGISTER") == 0) {
						tag = strtok(NULL, " ");
						response = strcat(tag, " deregistered.\n");
						write(fd, response, strlen(response));
					}
					else if (strcmp(cmd, "MSG") == 0) {
						response = strtok(NULL, "\r\n");
						if (response[0] == '#') {
							tag = strtok(response, " ");
							printf("%s\n", tag);
							fflush(stdout);
							response = strtok(NULL, "\r\n");
							response = strcat(response, "\n");
							write(fd, response, strlen(response));
						} else {
							response = strcat(response, "\n");
							write(fd, response, strlen(response));
						}
					}
					else {
						response = "Oops! Sorry, I can't understand you.\n";
						write(fd, response, strlen(response));
					}
				}
			}
		}
	}
}


