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
** YOU MAY WANT TO VISIT MY REPO FOR THIS PROJECT (GITHUB):
** https://github.com/ardulat/Operating-Systems
*/

struct Client {
	int fd;
	bool all;
	struct Tag* tag;
	struct Client* next;
};

struct Tag {
	char* tagName;
	struct Tag* next;
};

struct Read {
	int fd;
	int cc;
	char *original;
};

struct Write {
	int fd;
	long size;
	char *ibuffer;
	char *original;
};

struct Client* users;
int usersCount;
int nfds;
fd_set afds;
fd_set rfds;

void printUsers() {
	struct Client* temp = users;
	printf("%d users online:\n", usersCount);
	while(temp != NULL) {
		printf("User%d\n", temp->fd);
		temp = temp->next;
	}
	printf("\n");
}

void insertTag(int fd, char* tagName) {
	struct Tag* temp = (struct Tag*) malloc (sizeof(struct Tag*));
	struct Client* user = users;
	int i;
	for(i = 0; i < usersCount; i++) {
		if(user->fd == fd) {
			temp->tagName = tagName;
			temp->next = user->tag;
			user->tag = temp;
			break;
		}
		user = user->next;
	}
}

void deleteTag(int fd, char* tagName) {
	struct Tag* temp = (struct Tag*) malloc (sizeof(struct Tag*));
	struct Client* user = users;
	int i;
	for(i = 0; i < usersCount; i++) {
		if(user->fd == fd && user->tag != NULL) {
			temp = user->tag;
			if(temp->next == NULL) {
				user->tag = temp->next; // = NULL
				free(temp);
				return;
			}
			struct Tag* temp1 = temp->next;
			while(temp != NULL) {
				if(strcmp(temp1->tagName, tagName) == 0) {
					temp->next = temp1->next;
					free(temp1);
					break;
				}
				temp = temp->next;
				temp1 = temp->next;
			}
			break;
		}
		user = user->next;
	}
}

void registerUser(int fd) {
	struct Client* temp = users;
	int i;
	for (i = 0; i < usersCount; i++) {
		if (temp->fd == fd) {
			temp->all = true;
		}
		// register all tags to this user (fd)
		struct Tag* tag = temp->tag;
		if(tag != NULL) {
			while(tag != NULL) {
				insertTag(fd, tag->tagName);
				tag = tag->next;
			}
		}
		temp = temp->next;
	}
}

void deregisterUser(int fd) {
	struct Client* temp = users;
	int i;
	for (i = 0; i < usersCount; i++) {
		if (temp->fd == fd) {
			temp->all = false;
		}
		// deregister all tags to this user (fd)
		struct Tag* tag = temp->tag;
		if(tag != NULL) {
			while(tag != NULL) {
				deleteTag(fd, tag->tagName);
				tag = tag->next;
			}
		}
		temp = temp->next;
	}
}

// function for checking tags for particular fd
void printTags(int fd) {
	struct Client* user = users;
	int i;
	printf("User%d has tags:\n", fd);
	for(i = 0; i < usersCount; i++) {
		if(user->fd == fd) {
			struct Tag* temp = user->tag;
			while(temp != NULL) {
					printf("%s\n", temp->tagName);
					temp = temp->next;
			}
			break;
		}
		user = user->next;
	}
}

char *removeTagFrom(char *str) {
	int size = strlen(str);
	printf("Original: %s\n", str);
	printf("original size is %d\n", size);
	int i = 0, j = 0, tagSize = 0, length;
	char *temp;
	while (str[i] != '#')
		i++;
	while (str[i] != ' '){
		i++;
		tagSize++;
	}
	length = size - tagSize;
	printf("without tag length is %d\n", length);
	temp = (char *) malloc (sizeof(char) * length);
	i = 0;
	while (str[i] != '#') {
		temp[j] = str[i];
		i++;
		j++;
	}
	while (str[i] != ' ') {
		i++;
	}
	i++;
	while (str[i] != '\0') {
		temp[j] = str[i];
		i++;
		j++;
	}

	temp[length] = '\0';

	printf("temp is %s\n", temp);

	return temp;
}

pthread_mutex_t *mutexes;

void *writeThread(void *ign) {
	struct Write arg = *((struct Write *) ign);
	int fd = arg.fd;
	long size = arg.size;
	char *original = arg.original;
	char *ibuffer = arg.ibuffer;

	pthread_mutex_lock(&mutexes[fd]);

	write(fd, original, strlen(original));
	sleep(1);
	write(fd, ibuffer, size+2);

	pthread_mutex_unlock(&mutexes[fd]);

	pthread_exit(NULL);
}

pthread_t *writingThreads;
pthread_mutex_t rmutex;

void *readThread(void *ign) {
	struct Read arg = *((struct Read *) ign);
	int fd = arg.fd;
	int cc = arg.cc;
	printf("fd = %d\n", fd);
	char *original = arg.original;
	printf("HEADER: %s\n", original);
	// MARK: --Reading the image
	int i = 0, j = 0, header;
	long size;
	char len[1024], newTag[1024];
	while (original[i] != ' ')
		i++;
	i++;
	if (original[i] == '#') {
		while (original[i] != ' ') {
			newTag[j] = original[i];
			i++;
			j++;
		}
		i++;
	}
	j = 0;
	while (original[i] != '/') {
		len[j] = original[i];
		i++;
		j++;
	}
	printf("ORIGINAL: %s\n", original);
	len[j] = '\0';
	size = atol(len);
	printf("Size is %ld\n", size);
	char *ibuffer = (char *) malloc (sizeof(char) * (size+1));
	long count = 0;
	while (count < size) {
		int many = read(fd, ibuffer+count, BUFSIZE);
		if (many <= 0)
			printf("Dropped byte.\n");
		count += many;
	}
	printf("\nStarting to send...\n");

	// MARK: --Writing the image
	int k=0;
	char send[cc];
	while(original[k]!='/' ){
		send[k]=original[k];
		k++;
	}
	original[k]='/';
	original[k+1]='\0';
	printf("Tag is --%s--\n", newTag);
	if (newTag[0] == '#') {
		struct Client *user = users;
		original = removeTagFrom(original);
		printf("%s\n", original);
		while (user != NULL) {
			struct Tag *Tag = user->tag;
			while (Tag != NULL) {
				if (strcmp(Tag->tagName, newTag) == 0) {
					// threads
					struct Write toWrite;
					toWrite.fd = user->fd;
					toWrite.original = original;
					toWrite.ibuffer = ibuffer;
					int status = pthread_create(&writingThreads[user->fd], NULL, &writeThread, &toWrite);
					if (status != 0)
						printf( "pthread_create error %d.\n", status );
				}
				Tag = Tag->next;
			}
			user = user->next;
		}
	} else {
		struct Client *user = users;
		while (user != NULL) {
			if (user->all == true) {
				// threads
				struct Write toWrite;
				toWrite.fd = user->fd;
				toWrite.original = original;
				toWrite.ibuffer = ibuffer;
				toWrite.size = size;
				int status = pthread_create(&writingThreads[user->fd], NULL, writeThread, &toWrite);
				if (status != 0)
					printf( "pthread_create error %d.\n", status );
			}
			user = user->next;
		}
	}

	// pthread_join
	for ( j = 0; j < nfds; j++ )
		if (FD_ISSET(j, &rfds))
			pthread_join( writingThreads[j], NULL );
	pthread_mutex_lock(&rmutex);
	FD_SET(fd, &afds);
	pthread_mutex_unlock(&rmutex);

	printf("Completed.\n");
	free(ibuffer);
	pthread_exit(NULL);
}

int
main( int argc, char *argv[] )
{
	char			buf[BUFSIZE];
	char			*service;
	struct sockaddr_in	fsin;
	int			msock;
	int			ssock;
	int			alen;
	int			fd;
	int			rport = 0;
	int			cc;
	int i;

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
	pthread_t readingThreads[nfds];
	writingThreads = (pthread_t *) malloc (sizeof(pthread_t) * nfds);
	mutexes = (pthread_mutex_t *) malloc (sizeof(pthread_mutex_t) * nfds);
	for (i = 0; i < nfds; i++)
		pthread_mutex_init(&mutexes[i], NULL);

	FD_ZERO(&afds);
	FD_SET( msock, &afds );

	for (;;)
	{
		struct timeval time;
		time.tv_sec = 1;

		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				&time) < 0)
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
			printf("Guy is %d\n", ssock);

		}

		/*	Handle the participants requests  */
		for ( fd = 0; fd < nfds; fd++ )
		{
			if (fd != msock && FD_ISSET(fd, &rfds))
			{
				// add to users linked list
				struct Client* temp = (struct Client*) malloc (sizeof(struct Client*));
				bool userExists = false;
				int j;
				struct Client* temp1 = (struct Client*) malloc (sizeof(struct Client*));
				temp1 = users;
				for(j = 0; j < usersCount; j++) {
					if(temp1->fd == fd) {
						userExists = true;
						break;
					}
					temp1 = temp1->next;
				}
				if (!userExists) {
					temp->fd = fd;
					temp->all = false;
					temp->next = users;
					temp->tag = NULL;
					users = temp;
					usersCount++;
				}
				if ( (cc = read( fd, buf, BUFSIZE )) <= 0 )
				{
					printf( "The User%d has gone.\n", fd );
					(void) close(fd);
					FD_CLR( fd, &afds );

				}
				else
				{
					buf[cc] = '\0';
					printf( "The User%d says: %s\n", fd, buf );
					char *original = (char *) malloc (sizeof(char) * strlen(buf));
					strcpy(original, buf);
					strcat(original, "\n");

					char *response, *tag;
					char *cmd = strtok(buf, " ");

					/* Understand what that message means */
					if (strcmp(cmd, "REGISTERALL\r\n") == 0) {
						registerUser(fd);
					}
					else if (strcmp(cmd, "DEREGISTERALL\r\n") == 0) {
						deregisterUser(fd);
					}
					else if (strcmp(cmd, "REGISTER") == 0) {
						tag = strtok(NULL, " \n\r\n");
						char *newTag = (char*) malloc (sizeof(char)*strlen(tag));
						strcpy(newTag, tag);
						insertTag(fd, newTag);
						printTags(fd);
					}
					else if (strcmp(cmd, "DEREGISTER") == 0) {
						tag = strtok(NULL, "\r\n");
						char *newTag = (char*) malloc (sizeof(char)*strlen(tag));
						strcpy(newTag, tag);
						deleteTag(fd, newTag);
						printTags(fd);
					}
					else if (strcmp(cmd, "MSG") == 0) {
						response = strtok(NULL, "\r\n");
						if (response[0] == '#') {
							tag = strtok(response, " ");
							char *newTag = (char*) malloc (sizeof(char)*strlen(tag));
							strcpy(newTag, tag);
							response = strtok(NULL, "\r\n");
							response = strcat(response, "\n");
							original = removeTagFrom(original);
							// find all registered users
							struct Client* temp = users;
							int i;
							for(i = 0; i < usersCount; i++) {
								struct Tag* Tag = temp->tag;
								while(Tag != NULL) {
									if(strcmp(Tag->tagName,newTag) == 0) {
										write(temp->fd, original, strlen(original));
									}
									Tag = Tag->next;
								}
								temp = temp->next;
							}
						}
						else {
							response = strcat(response, "\n");
							// find all registered users
							struct Client* temp = users;
							int i;
							for(i = 0; i < usersCount; i++) {
								if(temp->all == true) {
									write(temp->fd, original, strlen(original));
								}
								temp = temp->next;
							}
						}
					}
					else if (strcmp(cmd, "MSGE") == 0) {
						response = strtok(NULL, "\0");
						if (response[0] == '#') {
							tag = strtok(response, " ");
							char *newTag = (char*) malloc (sizeof(char)*strlen(tag));
							strcpy(newTag, tag);
							// find all registered users
							struct Client* temp = users;
							int i;
							for(i = 0; i < usersCount; i++) {
								struct Tag* Tag = temp->tag;
								while(Tag != NULL) {
									if(strcmp(Tag->tagName,newTag) == 0) {
										write(temp->fd, original, strlen(original));
									}
									Tag = Tag->next;
								}
								temp = temp->next;
							}
						}
						else {
							// find all registered users
							struct Client* temp = users;
							int i;
							for(i = 0; i < usersCount; i++) {
								if(temp->all == true) {
									write(temp->fd, original, strlen(original));
								}
								temp = temp->next;
							}
						}
					}
					else if (strcmp(cmd, "IMAGE") == 0) {
						// arg
						struct Read arg;
						arg.fd = fd;
						arg.original = original;
						arg.cc = cc;
						FD_CLR(fd, &afds);
						int st = pthread_create(&readingThreads[fd], NULL, &readThread, &arg);
						if (st != 0) {
							printf("Error creating a thread.\n");
							FD_SET(fd, &afds);
						}
						pthread_join(readingThreads[fd],NULL);
					}
					//else {
					//	response = "Oops! Sorry, I can't understand you.\n";
					//	write(fd, response, strlen(response));
				//	}
				}
			}
		}
	}
	pthread_exit(0);
}
