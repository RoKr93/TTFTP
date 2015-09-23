/*
** name: ttftp.h
**
** author:
** date:
** last modified:
**
** from template created 31 jan 2015 by bjr
**
*/



extern int g_debug ;
extern int g_verbose ;

#define TFTP_RRQ 1
#define TFTP_WRQ 2
#define TFTP_DATA 3
#define TFTP_ACK 4
#define TFTP_ERROR 5

struct TftpReq {
        char opcode[2] ;
	char filename_and_mode[1] ;
} ;

struct TftpData {
	char opcode[2] ;
	char block_num[2] ;
	char data[1] ; /* could be zero */
} ;

struct TftpAck {
	char opcode[2] ;
	char block_num[2] ;
} ;

struct TftpError {
	char opcode[2] ;
	char error_code[2] ;
	char error_msg[1] ;
} ;


int  ttftp_client( char * host, char * port, char * file ) ;


/* Given a port, and a sockaddr structure, returns a newly created server socket 
 * listening on that port. Returns -1 of there is a problem */
int create_server_datagram_socket(short myport, struct sockaddr_in * my_addr ) ;


/* Given a port, a hostname and a sockaddr structure, returns a newly created client 
 * socket that will send to that host:port. Fills in their_addr for use in send.
 * Returns -1 if there is a problem. */
int create_client_datagram_socket(short myport, char * hostname, struct sockaddr_in * their_addr ) ;

void printbuffer(char *title, char * b, int n) ;
