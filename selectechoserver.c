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
typedef int bool;
#define true 1
#define false 0

int passivesock( char *service, char *protocol, int qlen, int *rport );

/*
**	The server ...
*/

struct Client {
	int fd;
	bool all;
	struct Client* next;
};

struct Tag {
	char* tagName;
 	struct Client* user;
	struct Tag* next;
};

struct Client* users;
int usersCount;
struct Tag* tags;
int tagsCount;

void registerUser(int fd) {
	struct Client* temp = (struct Client*) malloc (sizeof(struct Client*));
	temp->fd = fd;
	temp->all = true;
	temp->next = users;
	users = temp;
	usersCount++;
}

void deregisterUser(int fd) {
	struct Client* temp = users;
	if(usersCount == 0) {
		printf("No users.\n");
		return;
	}
	int i;
	for (i = 0; i < usersCount; i++) {
		if (temp->fd == fd) {
			temp->all = false;
			break;
		}
		temp = temp->next;
	}
}

void insertTag(int fd, char* tagName) {
	struct Tag* temp = (struct Tag*) malloc (sizeof(struct Tag*));
	temp->tagName = tagName;
	temp->user->fd = fd;
	temp->next = tags;
	tags = temp;
	tagsCount++; // increment the size of the list (added one tag)
}

void deleteTag(int fd, char* tagName) {
	struct Tag* temp = tags;
	if(tagsCount == 0) {
		printf("List is empty.\n");
		return;
	}
	struct Tag* temp1 = temp->next;
	int i;
	for(i = 0; i < tagsCount; i++) {
		if (temp1->user->fd == fd && temp->tagName == tagName) {
			temp->next = temp1->next;
			free(temp1);
			tagsCount--;
		}
		temp = temp->next;
		temp1 = temp->next;
	}
}
void deleteFd(int fd) {
	struct Tag* temp = tags;
	if(tagsCount == 0) {
		printf("List is empty.\n");
		return;
	}
	struct Tag* temp1 = temp->next;
	int i;
	for(i = 0; i < tagsCount; i++) {
		if(temp1->user->fd == fd) {
			temp->next = temp1->next;
			free(temp1);
			tagsCount--;
		}
		temp = temp->next;
		temp1 = temp->next;
	}
}

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
					printf( "The client%d has gone.\n", fd );
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
						registerUser(fd);
						response = "Ok! Registered all.\n";
						write(fd, response, strlen(response));
					}
					else if (strcmp(cmd, "DEREGISTERALL\r\n") == 0) {
						deregisterUser(fd);
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
							// find all registered users
							struct Client* temp = users;
							int i;
							for(i = 0; i < usersCount; i++) {
								if(temp->all == true) {
									write(temp->fd, response, strlen(response));
								}
								temp = temp->next;
							}
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
