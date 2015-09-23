/*
** name: mynetfunctions.c
**
** created: 31 jan 2015 by bjr
** last modified: 
**      18 feb 2015, to adjust include file name -bjr
**
*/


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<assert.h>

#define h_addr h_addr_list[0]

#include "ttftp.h"

int create_server_datagram_socket(short myport, struct sockaddr_in * my_addr ) {
	int sockfd ;
	
	if ((sockfd=socket(AF_INET,SOCK_DGRAM,0))==-1) {
		perror("socket") ;
		return -1 ;
	}
	
	my_addr->sin_family = AF_INET ;
	my_addr->sin_port = htons(myport) ;
	my_addr->sin_addr.s_addr = INADDR_ANY ;
	memset(&(my_addr->sin_zero),'\0',8) ;

	if (bind(sockfd, (struct sockaddr *) my_addr, 
		sizeof(struct sockaddr)) == -1 ) {
		perror("bind") ;
		return -1 ;
	}
	return sockfd ;
}

int create_client_datagram_socket(short myport, char * hostname, struct sockaddr_in * their_addr ) {
  int sockfd ;
  struct hostent * he ;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) {
    perror("socket") ;
    return -1 ;
  }

  if ((he=gethostbyname(hostname))==NULL) {
    perror("gethostbyname") ;
    return -1 ;
  }

  their_addr->sin_family = AF_INET ;
  their_addr->sin_port = htons(myport) ;
  their_addr->sin_addr = *((struct in_addr *)he->h_addr) ;
  memset(&(their_addr->sin_zero), '\0', 8 ) ;
  return sockfd;
}

int create_bound_client_datagram_socket(short myport, char * hostname, struct sockaddr_in * their_addr, struct sockaddr_in *my_addr ) {
	int sockfd ;
	struct hostent * he ;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) {
		perror("socket") ;
		return -1 ;
	}
		
	if ((he=gethostbyname(hostname))==NULL) {
		perror("gethostbyname") ;
		return -1 ;
	}

	their_addr->sin_family = AF_INET ;
	their_addr->sin_port = htons(myport) ;
	their_addr->sin_addr = *((struct in_addr *)he->h_addr) ;
	memset(&(their_addr->sin_zero), '\0', 8 ) ;
	
	// bind the client socket to a port so it can listen as well
	my_addr->sin_family = AF_INET ;
        my_addr->sin_port = 0;
        my_addr->sin_addr.s_addr = INADDR_ANY ;
        memset(&(my_addr->sin_zero),'\0',8) ;

        if (bind(sockfd, (struct sockaddr *) my_addr,
		 sizeof(struct sockaddr)) == -1 ) {
	  perror("bind") ;
	  return -1 ;
        }

	return sockfd ;
	
}

void printbuffer(char * t, char * b, int n) {
    int i ;
    unsigned char * bu = (unsigned char *) b ; /* not really needed by the compiler was complaining */
    printf("Info %s (printbuffer):",t) ;
    for (i=0;i<n;i++) {
        if ( !(i%32) ) printf("\n\t") ;
        else if ( !(i%8) ) printf("| ") ;
        printf("%02x ",bu[i]) ;
    }
    printf("\n") ;
}

