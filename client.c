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
void ksa(unsigned char state[], unsigned char key[], int len);
void prga(unsigned char state[], unsigned char out[], int len);

/*
**	Client
** YOU MAY WANT TO VISIT MY REPO FOR THIS PROJECT (GITHUB):
** https://github.com/ardulat/Operating-Systems
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
char key[BUFSIZE];

/* The Itoa code is in the public domain */
char* Itoa(int value, char* str, int radix) {
    static char dig[] =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz";
    int n = 0, neg = 0;
    unsigned int v;
    char* p, *q;
    char c;
    if (radix == 10 && value < 0) {
        value = -value;
        neg = 1;
    }
    v = value;
    do {
        str[n++] = dig[v%radix];
        v /= radix;
    } while (v);
    if (neg)
        str[n++] = '-';
    str[n] = '\0';
    for (p = str, q = p + (n-1); p < q; ++p, --q)
        c = *p, *p = *q, *q = c;
    return str;
}

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
    buf[strlen(buf)] = '\0';
		unsigned char *cmd, *temp, *rest, *stream;
    stream = (unsigned char*) malloc(sizeof(unsigned char) * 1024);
		int i;
		temp = (unsigned char*) malloc (sizeof(unsigned char) * strlen(buf));
		for (i = 0; i < strlen(buf); i++)
			temp[i] = buf[i];
		cmd = strtok(temp, " ");
		if ( strcmp(cmd, "MSGE" ) == 0) {
			unsigned char state[256];
			rest = strtok(NULL, "\n");
			char *tag;
			// tag or not
			if ( rest[0] == '#' ) {
				// it is tag
				tag = strtok(rest, " ");
				char *newTag = (char*) malloc (sizeof(char)*strlen(tag));
				strcpy(newTag, tag);
				strcat(temp, " ");
				strcat(temp, newTag);
				rest = strtok(NULL, "\0");
        free(newTag);
			}
      rest[strlen(rest)] = '\0';
      int j = 0;
      while (rest[j] != '\0') {
        printf("%c\n", rest[j]);
        j++;
      }
      int len = j;
      ksa(state, key, strlen(key));
			prga(state, stream, len);
			char *encrypted;
      stream[len]='\0';
			encrypted = (char *) malloc (sizeof(char) * len);
			for(i = 0; i < len; i++)
				encrypted[i] = rest[i] ^ stream[i];
      encrypted[i]='\0';
      sprintf(buf,"MSGE %d/",len);
      int tempInt=strlen(buf);
      for(i=0;i<len;i++){
        buf[tempInt+i]=encrypted[i];
      }
      buf[tempInt+len]='\0';
		} else {
			 //Process before sending
			 int lastIndex = strlen(buf)-1;
			 buf[lastIndex] = '\r';
			 buf[lastIndex+1] = '\n';
			 buf[lastIndex+2] = '\0';
	 }

		// Send to the server
		if ( write( csock, buf, strlen(buf) ) < 0 )
		{
			fprintf( stderr, "client write: %s\n", strerror(errno) );
			exit( -1 );
		}
    free(stream);
    free(temp);
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
		ans[lastIndex+2] = '\0';

		// Read the echo and print it out to the screen
		if ( (cc = read( csock, ans, BUFSIZE )) <= 0 ) {
			printf( "The server has gone.\n" );
			close(csock);
			exit(-1);
		}
		else {
      ans[cc] = '\0';
			// Everything is OK (User is still online)
      char *cmd, *original;
      original = (char *) malloc (sizeof(char) * strlen(ans));
      int i;
      for(i = 0; i < strlen(ans); i++)
        original[i] = ans[i];
      cmd = strtok(ans, " ");
      if (strcmp(cmd, "MSGE") == 0) {
        char *msg = strtok(NULL, "\0");
        if (msg[0] == '#') {
          char *tag = strtok(msg, " ");
          msg = strtok(NULL, "\0");
        }
        unsigned char *temp, *rest, length[BUFSIZE], stream[1024];
  			int i, j, len;
  			temp = (unsigned char*) malloc (sizeof(unsigned char) * strlen(msg));
  			for (i = 0; i < strlen(msg); i++)
  				temp[i] = msg[i];
  			i = 0;
  			while (temp[i] != '/') {
  				length[i] = temp[i];
          i++;
  			}
        length[i] = '\0';
  			len = atoi(length);
  			rest = (unsigned char*) malloc (sizeof(unsigned char) * len);
  			i++;
  			for (j = 0; j < len; j++, i++)
  				rest[j] = temp[i];
  			unsigned char state[256];
  			ksa(state, key, strlen(key));
  			prga(state, stream, len);
  			char *decrypted;
  			decrypted = (char *) malloc (sizeof(char) * len);
  			for(i = 0; i < len; i++)
  				decrypted[i] = rest[i] ^ stream[i];
        decrypted[len]='\0';
      }
      else {
        printf("%s", original);
      }
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
  printf( "Please enter the password for encrypted messages: " );
  fgets( key, BUFSIZE, stdin );
  key[strlen(key)-1] = '\0';
  printf( "Type q or Q to quit.\n" );
	fflush( stdout );

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
