/*
** name: ttftp.c
**
** author: Roshan Krishnan
** date: 2/25/15
** last modified: 4/24/15
**
** from template created 18 feb 2015 by bjr
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
#include<time.h>

#include "aes.h"
#include "ttftp.h"

#define MAXMSGLEN 512
#define MAXFNAMELEN 256
#define MAXERRLEN 128
#define SUBBLOCKLEN 16
#define CONST_RB 0x87
#define USAGE_MESSAGE "usage: ttftp -l [-vd] [-t timeout] port\n       ttftp [-v] [-f savefile [-w lingertime] host port file"
#define FNAME_MESSAGE "Invalid filename: either too long or includes a path"

int g_debug = 0 ;
int g_verbose = 0 ;

int main(int argc, char * argv[]) {
	int ch ;
	int is_server = 0 ;
	int timeout = 4;
	int wait_time = 12;
	char *mode = NULL;
	char *savefile;
	char key[SUBBLOCKLEN];
	int len;
	int is_savefile = 0;
	int is_daemon = 0;
	
	while ((ch = getopt(argc, argv, "ldvf:t:w:k:D:")) != -1) {
		switch(ch) {
		case 'D':
			g_debug = atoi(optarg) ;
			break ;
		case 'v':
			g_verbose = 1 ;
			break ;
		case 'l':
			is_server = 1 ;
			break ;
		case 'f':
		        is_savefile = 1;
		        savefile = optarg;
			break;
		case 'd':
		        is_daemon = 1;
			break;
		case 't':
		        timeout = atoi(optarg);
			break;
		case 'w':
		        wait_time = atoi(optarg);
			break ;
		case 'k':
		        len = strlen(optarg);
		        memcpy(key, optarg, len);
			memset(key+len, 0x0, SUBBLOCKLEN-len);
			mode = "AES128";
			break;
		case '?':
		default:
			printf("%s\n",USAGE_MESSAGE) ;
			return 0 ;
		}
	}
	argc -= optind;
    argv += optind;
    
    if ( (is_server && argc!=1) || (!is_server && argc!=3) ) {
    	printf("%s\n",USAGE_MESSAGE) ;
    	exit(0) ;
    }

    // if we're not encrypting, mode is octet.
    if(!mode){
      mode = "octet";
    }

    if (! is_server ) {
    	/* is client */
    	assert( argc==3 ) ;  /* true because we checked it, see above */
    	// get the command line arguments.
	char *to_host = argv[0];
	int to_port = atoi(argv[1]);
	char *filename = argv[2];

	// check for valid filename.
	if(strlen(filename) > MAXFNAMELEN || strchr(filename, '/') != NULL){
	  printf("%s\n", FNAME_MESSAGE);
	  exit(0);
	}

	// declare some variables.
	struct sockaddr_in their_addr;
	struct sockaddr_in my_client_addr;
	int sockfd;
	int numbytes;
	struct TftpReq *readReq;
	short rrOpcode = TFTP_RRQ ; //htons(TFTP_RRQ);
	int iv = (int)(((long int)time(NULL)) % 1000000000);
	char initialVector[10];
	int clientsockfd;

	// create the client socket.
	sockfd = create_bound_client_datagram_socket(to_port, to_host, 
						     &their_addr, 
						     &my_client_addr);
	if(sockfd == -1){
	  exit(1);
	}
	
	// find out the client port
	socklen_t len = sizeof(my_client_addr);
	if(getsockname(sockfd, 
		       (struct sockaddr *)&my_client_addr, &len) == -1){
	  perror("getsockname");
	  exit(1);
	}
 
	// format the datagram.
	size_t mSize = sizeof(struct TftpReq) + strlen(filename)
	  + strlen(mode) + 2*sizeof(char);
	// if we're encoding, we'll need to make room for the IV.
	if(!strcmp(mode, "AES128"))
	  mSize += 10;
	readReq = malloc(mSize);
	readReq->opcode[0] = (rrOpcode >> 8) & 0xff;
	readReq->opcode[1] = rrOpcode & 0xff;
	strcpy(readReq->filename_and_mode, filename);
	strcpy(readReq->filename_and_mode+strlen(filename)+1, mode);
	if(!strcmp(mode, "AES128")){
	  // add the IV on to the end of the RRQ
	  sprintf(initialVector, "%d", iv);
	  strcpy(readReq->filename_and_mode+strlen(filename)
		 +strlen(mode)+2, initialVector);
	}
	 
	// send it away!
	numbytes = sendto(sockfd, (void *)readReq, mSize, 0,
			  (struct sockaddr *)&their_addr,
			  sizeof(struct sockaddr));
	if(numbytes == -1){
	  perror("sendto");
	  exit(1);
	}
	clientsockfd = sockfd;
	// free the allocated memory.
	free(readReq);
	printf("Sent it!\n");
	// START LOOP OF LISTENING FOR DATA
	int loop = 1;
	while(loop){
	  printf("Listening...\n");
	  // now listen for a new packet.
	  struct TftpData *buf;
	  short opcode;
	  int fromlen = sizeof(struct sockaddr);
	  // max length of a data packet
	  mSize = sizeof(struct TftpData) + MAXMSGLEN;
	  buf = malloc(mSize);

	  // listen
	  numbytes = recvfrom(sockfd, (struct TftpData *)buf, mSize, 0,
			      (struct sockaddr *)&their_addr,
			      &fromlen);
	  if(numbytes == -1){
	    perror("recvfrom");
	    exit(1);
	  }
	  printf("Got a packet\n");
	  // get opcode to make sure we have a data packet
	  opcode = buf->opcode[0] << 8 | buf->opcode[1];
	  opcode = ntohs(opcode);
	  if(opcode == TFTP_DATA){
	    // we have a data packet, woo!
	    // send an ACK.
	    printf("We got some data!\n");
	    struct TftpAck *ack = malloc(sizeof(struct TftpAck));
	    int numbytessent;
	    short ackOpcode = htons(TFTP_ACK);
	    ack->opcode[0] = (ackOpcode >> 8) & 0xff;
	    ack->opcode[1] = ackOpcode & 0xff;

	    // same block number
	    ack->block_num[0] = buf->block_num[0];
	    ack->block_num[1] = buf->block_num[1];

	    // send 'er away
	    numbytessent = sendto(sockfd, (void *)ack, sizeof(ack), 0,
			      (struct sockaddr *)&their_addr,
			      sizeof(struct sockaddr));
	    if(numbytessent == -1){
	      perror("sendto");
	      exit(1);
	    }

	    // free ack data
	    free(ack);

	    if(!strcmp(mode, "octet")){
	      // we didn't encrypt, so just print the data.
	      fwrite(buf->data, 1, numbytes-5, stdout);
	    
	      // if we have a savefile, save it.
	      if(is_savefile){
		FILE *fp;
		fp = fopen(savefile, "a");
		fwrite(buf->data, 1, numbytes-5, fp);
		fclose(fp);
	      }

	      // check to see if we stop looping
	      if(numbytes-5 < MAXMSGLEN){
		// wait to see if it's retransmitted
		//sleep(wait_time);
		// finish up
		loop = 0;
		//free(buf);   
		close(sockfd);
	      }

	    }else{
	      // DECRYPTION
	      char subBlkCtr = 1;
	      unsigned short client_port = my_client_addr.sin_port;
	      unsigned short server_port = to_port;
	      //printf("Client: CP %d\n", client_port);
	      int printedCount = 0;
	      int lastPadded = 0;
	      for(subBlkCtr = 1; subBlkCtr <= 32; subBlkCtr++){
		// field to hold the current sub-block
		char subBlock[SUBBLOCKLEN];

		// encryption data block
		char encryptionData[SUBBLOCKLEN];

		// encrypted output
		char eOut[SUBBLOCKLEN];

		// first 2 bytes are the block number
		encryptionData[0] = buf->block_num[0];
		encryptionData[1] = buf->block_num[1];

		// sub-block counter
		encryptionData[2] = subBlkCtr;

		// client port
		encryptionData[3] = (htons(client_port) >> 8) & 0xff;
		encryptionData[4] = htons(client_port) & 0xff;
		
		// server port
		encryptionData[5] = (htons(server_port) >> 8) & 0xff;
		encryptionData[6] = htons(server_port) & 0xff;

		// IV
		memcpy(encryptionData+7, initialVector, 9);

		// get the current sub-block
		memcpy(subBlock, buf->data+((subBlkCtr-1)*SUBBLOCKLEN),
		       SUBBLOCKLEN);

		// do AES
		//printbuffer("encryptionData",encryptionData,16) ;
		//printbuffer("key",key,16) ; 

		AES128_ECB_encrypt(encryptionData, key, eOut);
		
		// test to see if it's the MAC                                 
                char macTest[SUBBLOCKLEN];
		memcpy(macTest,key, SUBBLOCKLEN);

		if(lastPadded){
		  int i;
		  for(i = 0; i < SUBBLOCKLEN; i++)
		    macTest[i] ^= CONST_RB;
		}

		int macCtr;
		for(macCtr = 0; macCtr < SUBBLOCKLEN; macCtr++)
		  macTest[macCtr] ^= eOut[macCtr];

		if(lastPadded)
		  macTest[0] &= ~0x80;
		else
		  macTest[0] |= 0x80;

		if(!memcmp(macTest, subBlock, SUBBLOCKLEN)){
		  // we have the MAC. break the loop
		  //printf("Mac Received\n");
		  loop = 0;
		  close(sockfd);
		  break;
		}

		// decrypt by XORing
		int ctr;
		for(ctr = 0; ctr < SUBBLOCKLEN; ctr++)
		  subBlock[ctr] ^= eOut[ctr];
		
		// check for padding
		char *pad = memchr(subBlock, 0xff, SUBBLOCKLEN);
		int writeSize;
		if(pad){
		  lastPadded = 1;
		  writeSize = (int)(pad - subBlock);
		}else{
		  lastPadded = 0;
		  writeSize = SUBBLOCKLEN;
		}

		// write out the data
		fwrite(subBlock, 1, writeSize, stdout);

		// if we have a savefile, save it.
		if(is_savefile){
		  FILE *fp;
		  fp = open(savefile, "a");
		  fwrite(subBlock, 1, writeSize, fp);
		  fclose(fp);
		}

		// increment printedCount
		printedCount += SUBBLOCKLEN;

		if(printedCount >= numbytes - 5)
		  break;
	      }
	      /*if(numbytes < MAXMSGLEN){
		loop = 0;
		close(sockfd);
		}*/
	    }
	  }else{
	    // it's an error packet.
	    struct TftpError *errPack = (struct TftpError *)buf;
	    short errorCode = 
	      errPack->error_code[0] << 8 | errPack->error_code[1];
	    errorCode = ntohs(errorCode);
	    printf("Error %d: %s\n", errorCode, errPack->error_msg);
	    free(buf);
	    close(sockfd);
	    exit(1);
	  }
	  free(buf);
	}
    }
    else {
    	/* is server */
    	
    	if (g_verbose) printf("Line %d: server loop entered\n", __LINE__ ) ;
    	// get argument
	int listen_port = atoi(argv[0]);
	
	// create initial variables
	int sockfd;
	int daemonMode = 1;
	struct sockaddr_in my_addr;

	// create a socket for our listen port
	sockfd = create_server_datagram_socket(listen_port, &my_addr);
	if(sockfd == -1){
	  exit(1);
	}

	while(daemonMode){
	  int sockfd2;
	  struct TftpReq *recvReq;
	  struct sockaddr_in from_addr;
	  struct sockaddr_in my_addr2;
	  int numbytes;
	  int addr_len;
	  int errorCode = -1;
	  char *filename;
	  char *reqMode;
	  char *iv;
	  short blocknum = 0;
	  FILE *fp;
	  int filelen = 0;
	  int readIndex = 0;

	// allocate enough memory to store our datagram
	size_t mSize = sizeof(struct TftpReq) + MAXFNAMELEN + strlen(mode)
	  + 2*sizeof(char);
	if(!strcmp(mode, "AES128"))
	   mSize += 10;
	recvReq = malloc(mSize);

	// call recvfrom expecting a read request
	addr_len = sizeof(struct sockaddr);
	numbytes = recvfrom(sockfd, (struct TftpReq *)recvReq, mSize, 0,
			    (struct sockaddr *)&from_addr, &addr_len);

	// check for errors
	if(numbytes == -1){
	  perror("recvfrom");
	  exit(1);
	}

	// get the info
	filename = recvReq->filename_and_mode;
	reqMode = recvReq->filename_and_mode + strlen(filename) + 1;
	if(!strcmp(reqMode, "AES128"))
	  iv = recvReq->filename_and_mode+strlen(filename)+strlen(reqMode)+2;

	// start concurrency.
	// child process handles transfer
	// parent skips everything and goes back to listening on listen port
	pid_t childpid;
	childpid = fork();
	if(childpid < 0){
	  // fork failed, bail out
	  perror("fork");
	  exit(1);
	}else if(childpid != 0){
	  // parent process, do nothing until next loop iteration
	}else{
	// child process, do transfer
	// create a new socket to use for the file transfer
	// 0 will bind to a random port
	sockfd2 = create_server_datagram_socket(0, &my_addr2);
	if(sockfd2 == -1){
          exit(1);
	}

	// open file and get length
	fp = fopen(filename, "rb");
	if(fp != NULL){
	  fseek(fp, 0, SEEK_END);
	  filelen = ftell(fp);
	  rewind(fp);
	}else{
	  errorCode = 1;
	}

	if(strcmp(reqMode, "octet") && strcmp(reqMode, "AES128"))
	  errorCode = 4;

	if(!strcmp(reqMode, "AES128") && !key)
	  errorCode = 8;

	// check for errors
	if(errorCode >= 0){
	  // send error packet
	  struct TftpError *errPack;
	  int numbytes;
	  short eOpcode = htons(TFTP_ERROR);
	  size_t mSize = sizeof(struct TftpError) + MAXERRLEN;
	  errPack = malloc(mSize);

	  // set opcode
	  errPack->opcode[0] = (eOpcode >> 8) & 0xff;
	  errPack->opcode[1] = eOpcode & 0xff;

	  // set error code
	  short netErrorCode = htons(errorCode);
	  errPack->error_code[0] = (netErrorCode >> 8) & 0xff;
	  errPack->error_code[1] = netErrorCode & 0xff;

	  // error message
	  if(errorCode == 1){
	    char *msg = "File not found.";
	    strcpy(errPack->error_msg, msg);
	  }else if(errorCode == 4){
	    char *msg = "Illegal TFTP operation.";
	    strcpy(errPack->error_msg, msg);
	  }else if(errorCode == 8){
	    char *msg = "Encryption requested but no key.";
	    strcpy(errPack->error_msg, msg);
	  }

	  // send it
	  numbytes = sendto(sockfd2, (void *)errPack, mSize, 0,
			    (struct sockaddr *)&from_addr,
			    sizeof(struct sockaddr));
	  if(numbytes == -1){
	    perror("sendto");
	    exit(1);
	  }

	  // exit since we failed.
	  free(errPack);
	  exit(0);
	}else{
	  // LOOP TO SEND DATA
	  int loop = 1;
	  FILE *fp;
	  int filelen = 0;
	  int readIndex = 0;

	  // open file and get length
	  fp = fopen(filename, "rb");
	  if(fp == NULL){
	    printf("Error opening file!\n");
	    exit(1);
	  }
	  fseek(fp, 0, SEEK_END);
	  filelen = ftell(fp);
	  rewind(fp);
	  
	  // create these variables outside the loop
	  // handles edge case where MAC begins new TFTP message
	  int isMac = 0;
	  int isPadded = 0;
	  while(loop){
	    // create the data by reading from the file
	    struct TftpData *fileData;
	    struct TftpAck *ack;
	    short dOpcode = htons(TFTP_DATA);
	    size_t mSize;
	    int numbytes;
	    int addr_len = sizeof(struct sockaddr);
	    
	    // increment the block number
	    blocknum++;
	    
	    // allocate some space
	    mSize = sizeof(struct TftpData) + MAXMSGLEN;
	    fileData = malloc(mSize);
	    
	    // store opcode
	    fileData->opcode[0] = (dOpcode >> 8) & 0xff;
	    fileData->opcode[1] = dOpcode & 0xff;
	    
	    // store the block number
	    fileData->block_num[0] = (htons(blocknum) >> 8) & 0xff;
	    fileData->block_num[1] = htons(blocknum) & 0xff;
	    
	    // NO ENCRYPTION
	    if(!strcmp(reqMode, "octet")){
	      // read from file
	      if(readIndex+MAXMSGLEN <= filelen){
		//printf("Not last packet\n");
		fread(fileData->data, MAXMSGLEN, 1, fp);
		//fileData->data[MAXMSGLEN] = '\0';
		readIndex += MAXMSGLEN;
		fseek(fp, readIndex, SEEK_SET);
	      }else{
		if(readIndex == filelen + MAXMSGLEN){
		  //printf("Empty\n");
		  char *empty = "";
		  strcpy(fileData->data, empty);
		  mSize = sizeof(struct TftpData);
		  fileData = realloc(fileData, mSize);
		}else{
		  //printf("Last packet\n");
		  int len = filelen - readIndex;
		  fread(fileData->data, len, 1, fp);
		  //fileData->data[len] = '\0';
		  mSize = sizeof(struct TftpData) + len;
		  fileData = realloc(fileData, mSize);
		}
		loop = 0;
	      }
	    }else{
	      // ENCRYPTION
	      char subBlkCtr = 1;
	      int subLoop = 1;
	      unsigned short client_port = from_addr.sin_port;
	      unsigned short server_port = listen_port;
	      //printf("Server: CP %d\n", client_port);
	      mSize = sizeof(struct TftpData);
	      while(subBlkCtr <= 32 && subLoop){
		// create data field to be encrypted
		char encryptionData[SUBBLOCKLEN];

		// create field to hold sub-block
		char subBlock[SUBBLOCKLEN];

		// create field to hold encrypted output
		char eOut[SUBBLOCKLEN];

		// first 2 bytes are overall blocknum
		encryptionData[0] = (htons(blocknum) >> 8) & 0xff;
		encryptionData[1] = htons(blocknum) & 0xff;

		// 3rd byte is sub-block counter
		encryptionData[2] = subBlkCtr;
		
		// next 2 bytes are client port number
		encryptionData[3] = (htons(client_port) >> 8) & 0xff;
		encryptionData[4] = htons(client_port) & 0xff;

		// next 2 bytes are server port number
		encryptionData[5] = (htons(server_port) >> 8) & 0xff;
		encryptionData[6] = htons(server_port) & 0xff;

		// the last 9 bytes are the IV
		memcpy(encryptionData+7, iv, 9);

		// do AES encryption
		//printbuffer("encryptionData",encryptionData,16) ;
		//printbuffer("key",key,16) ; 

		AES128_ECB_encrypt(encryptionData, key, eOut);
		
		int getOut = 0;
		if(isMac){
		  char macKey[SUBBLOCKLEN];
		  memcpy(macKey, key, SUBBLOCKLEN);
		  if(isPadded){
		    // XOR with const_rb
		    int i;
		    for(i = 0; i < SUBBLOCKLEN; i++)
		      macKey[i] ^= CONST_RB;
		  }

		  // XOR encryption data with key
		  int ctr;
		  for(ctr = 0; ctr < SUBBLOCKLEN; ctr++)
		    macKey[ctr] ^= eOut[ctr];

		  // clear most significant bit if padded, set if not
		  if(isPadded)
		    macKey[0] &= ~0x80;
		  else
		    macKey[0] |= 0x80;

		  // put it in the TFTP message
		  memcpy(fileData->data+((subBlkCtr-1)*SUBBLOCKLEN),
			 macKey, SUBBLOCKLEN);
		  
		  // increment mSize
		  mSize += SUBBLOCKLEN;

		  // we want to get out of our loop
		  getOut = 1;

		  // this should be the last iteration of the parent loop
		  loop = 0;
		}
		
		// if we just did the MAC, get out of this loop
		if(getOut)
		  break;

		if(!isMac){
		  // read from file
		  if(readIndex + SUBBLOCKLEN <= filelen){
		    fread(subBlock, SUBBLOCKLEN, 1, fp);
		    readIndex += SUBBLOCKLEN;
		    fseek(fp, readIndex, SEEK_SET);
		  }else{
		    int datalen = filelen - readIndex;
		    int rem = SUBBLOCKLEN - datalen;
		    fread(subBlock, datalen, 1, fp);
		    // do padding
		    memset(subBlock+datalen, 0xff, 1);
		    memset(subBlock+datalen+1, 0x00, rem - 1);
		    isMac = 1;
		    isPadded = 1;
		    //loop = 0;
		    //subLoop = 0;
		  }
		
		  // XOR encryption data with plaintext
		  int ctr;
		  for(ctr = 0; ctr < SUBBLOCKLEN; ctr++)
		    subBlock[ctr] ^= eOut[ctr];
		
		  // put it in the TFTP message
		  memcpy(fileData->data+((subBlkCtr-1)*SUBBLOCKLEN),
			 subBlock, SUBBLOCKLEN);
		}

		// increment the sub-block counter
		subBlkCtr++;

		// check if we need to do MAC for no-padding case
		if(readIndex >= filelen)
		  isMac = 1;

		// increment mSize
		mSize += SUBBLOCKLEN;
	      }
	    }
	    
	    int rv = 0; // select() return value
	    int resendCounter = 0; // count # of resends
	    while(rv == 0){
	      fd_set readfds; // socket FDs to be read by select()
	      struct timeval tv; // timeout time struct
	      int n;

	      // send the data
	      numbytes = sendto(sockfd2, (void *)fileData, mSize, 0,
				(struct sockaddr *)&from_addr,
				sizeof(struct sockaddr));
	      if(numbytes == -1){
		perror("sendto");
		exit(1);
	      }

	      // set up and call select()
	      FD_ZERO(&readfds);
	      FD_SET(sockfd2, &readfds);
	      n = sockfd2 + 1;
	      tv.tv_sec = timeout;
	      tv.tv_usec = 0;
	      rv = select(n, &readfds, NULL, NULL, &tv);

	      if(rv == -1){
		// we had an error in select()
		perror("select");
		exit(1);
	      }else if(rv == 0){
		// timeout, we'll need to resend
		resendCounter++;
	      }else{
		// receive the ACK
		ack = malloc(sizeof(struct TftpAck));
		numbytes = recvfrom(sockfd2, (struct TftpAck *)ack, 
				    sizeof(struct TftpAck), 0,
				    (struct sockaddr *)&from_addr, &addr_len);
		free(fileData);
		free(ack);
	      }
	      if(resendCounter >= 7){
		// we have to send an error packet
		struct TftpError *timeoutPack;
		int timeoutBytes;
		char *timeoutMsg = "Timeout";
		short tCode = 7;
		short tOpcode = htons(TFTP_ERROR);
		size_t tmSize = sizeof(struct TftpError) + MAXERRLEN;
		timeoutPack = malloc(mSize);

		// set opcode
		timeoutPack->opcode[0] = (tOpcode >> 8) & 0xff;
		timeoutPack->opcode[1] = tOpcode & 0xff;

		// set error code
		short netTimeoutCode = htons(tCode);
		timeoutPack->error_code[0] = (netTimeoutCode >> 8) & 0xff;
		timeoutPack->error_code[1] = netTimeoutCode & 0xff;

		// copy the message in
		strcpy(timeoutPack->error_msg, timeoutMsg);

		// send it
		timeoutBytes = sendto(sockfd2, (void *)timeoutPack, tmSize, 0,
				  (struct sockaddr *)&from_addr,
				  sizeof(struct sockaddr));
		if(timeoutBytes == -1){
		  perror("sendto");
		  exit(1);
		}

		// exit since we failed
		free(fileData);
		free(timeoutPack);
		exit(0);		
	      }
	    }
	  }
	  free(recvReq);
	  close(sockfd2);
	}

	}
	
	if(!is_daemon){
	  close(sockfd);
	  daemonMode = 0;
	}

	}
    }
	return 0 ;
}

