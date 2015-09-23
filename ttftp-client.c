/*
** name: ttftp-client.c
**
** author:
** date:
** last modified:
**
*/

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<assert.h>
#include<unistd.h>

#include "ttftp.h"

int  ttftp_client( char * host, char * port, char * file ) {
	
    if (g_verbose) printf("Line %d(%s): client loop entered\n", __LINE__,  __FILE__ ) ;

	return 0 ;
}

