#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define BUFSIZE		4096

int connectsock( char *host, char *service, char *protocol );

/*
**	Client
*/

// Global variables to use further
char		buf[BUFSIZE];
char		ans[BUFSIZE];
char		*service;
char		*host = "localhost";
int		cc;
int		csock;
int status;
pthread_t threads[2]; // 1 for writing, 1 for reading
pthread_mutex_t	 mutex; // purpose: sometimes does not work without it

void *writeThread ( void *arg ) {
	// 	Start the loop for writing to the server
	while ( fgets( buf, BUFSIZE, stdin ) != NULL )
	{
		// If user types 'q' or 'Q', end the connection
		if ( buf[0] == 'q' || buf[0] == 'Q' )
		{
			close(csock);
			exit(-1);
		}
		 //Process before sending
		 int lastIndex = strlen(buf)-1;
		 buf[lastIndex] = '\r';
		 buf[lastIndex+1] = '\n';
		 buf[lastIndex+2] = '\0';
		// Send to the server
		printf("--%s--\n", buf);
		if ( write( csock, buf, strlen(buf) ) < 0 )
		{
			fprintf( stderr, "client write: %s\n", strerror(errno) );
			exit( -1 );
		}
		buf[0] = '\0';
	}

	pthread_exit( NULL );
}

void *readThread ( void *arg ) {
	// 	Start the loop for reading from the server
	for(;;)
	{
		// Process before sending
		int lastIndex = strlen(ans)-1;
		ans[lastIndex] = '\r';
		ans[lastIndex+1] = '\n';

		// Read the echo and print it out to the screen
		if ( (cc = read( csock, ans, BUFSIZE )) <= 0 ) {
			printf( "The server has gone.\n" );
			close(csock);
			exit(-1);
		}
		else {
			// Everything is OK (User is still online)
			ans[cc] = '\0';
			printf("%s", ans);
		}
	}

	pthread_exit( NULL );
}

int
main( int argc, char *argv[] )
{

	switch( argc )
	{
		case    2:
			service = argv[1];
			break;
		case    3:
			host = argv[1];
			service = argv[2];
			break;
		default:
			fprintf( stderr, "usage: chat [host] port\n" );
			exit(-1);
	}

	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}

	printf( "The server is ready, please start sending to the server.\n" );
	printf( "Type q or Q to quit.\n" );
	fflush( stdout );

	pthread_mutex_init( &mutex, NULL );

	// create thread for writing
	status = pthread_create( &threads[0], NULL, writeThread, NULL );
	if ( status != 0 )
	{
		printf( "pthread_create error %d.\n", status );
		exit( -1 );
	}

	// create thread for reading
	status = pthread_create( &threads[1], NULL, readThread, NULL );
	if ( status != 0 )
	{
		printf( "pthread_create error %d.\n", status );
		exit( -1 );
	}

	int j;
	for ( j = 0; j < 2; j++ )
		pthread_join( threads[j], NULL );
	pthread_exit(0);

	close( csock );

}
