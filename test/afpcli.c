#include "afpclient.h"

#define UNICODE(a) (a->afp_version >= 30)

#define kTextEncodingUTF8 0x08000103

/* -------------------------------------------- */
int OpenClientSocket(char* host,int port)
{
int sock;
struct sockaddr_in server;
struct hostent* hp;
int attr;
 
	server.sin_family=AF_INET;
	server.sin_port=htons((unsigned short)port);
 
	hp=gethostbyname(host);
 	if(!hp) {
    unsigned long int addr=inet_addr(host);
    	if(addr!= (unsigned)-1)
       		hp=gethostbyaddr((char*)addr,sizeof(addr),AF_INET);
 
    	if(!hp) {
       		fprintf(stderr,"Unknown host '%s' for server \n.",host);
       		return(-1);
      	}
   	}
 
 	memcpy((char*)&server.sin_addr,(char*)hp->h_addr,sizeof(server.sin_addr));
 
 	sock=socket(PF_INET,SOCK_STREAM,0);
 	if(sock==-1) {
    	fprintf(stderr,"Failed to create client sockets.\n");
    	return(-1);
   	}
    attr = 1;
    setsockopt(sock, SOL_TCP, TCP_NODELAY, &attr, sizeof(attr));
 
 	if(connect(sock ,(struct sockaddr*)&server,sizeof(server))==-1) {
    	close(sock);
    	sock=-1;
    	fprintf(stderr,"Failed to connect socket.\n");
   	}
 
	 return(sock);
}
/* -------------------------------------------- */
int CloseClientSocket(int fd)
{
	return close(fd);
}

/* -------------------------------------------------------*/
/* read raw data. return actual bytes read. this will wait until
 * it gets length bytes */
size_t my_dsi_stream_read(DSI *dsi, void *data, const size_t length)
{
  size_t stored;
  ssize_t len;
 
  stored = 0;
  while (stored < length) {
    if ((len = read(dsi->socket, (u_int8_t *) data + stored,
                    length - stored)) == -1 && errno == EINTR)
      continue;
 
    if (len > 0)
      stored += len;
    else {/* eof or error */
      fprintf(stderr, "dsi_stream_read(%d): %s\n", len, (len < 0)?strerror(errno):"EOF");
      break;
    }
  }
 
  dsi->read_count += stored;
  return stored;
}

/* -------------------------------------------------------*/
/* read data. function on success. 0 on failure. data length gets
 * stored in length variable. this should really use size_t's, but
 * that would require changes elsewhere. */
int my_dsi_stream_receive(DSI *dsi, void *buf, const int ilength,
                       int *rlength)
{
  char block[DSI_BLOCKSIZ];
 
  /* read in the header */
  if (my_dsi_stream_read(dsi, block, sizeof(block)) != sizeof(block))
    return 0;
 
  dsi->header.dsi_flags = block[0];
  dsi->header.dsi_command = block[1];
  memcpy(&dsi->header.dsi_requestID, block + 2, sizeof(dsi->header.dsi_requestID));
  memcpy(&dsi->header.dsi_code, block + 4, sizeof(dsi->header.dsi_code));
  memcpy(&dsi->header.dsi_len, block + 8, sizeof(dsi->header.dsi_len));
  memcpy(&dsi->header.dsi_reserved, block + 12,sizeof(dsi->header.dsi_reserved));
  dsi->serverID = ntohs(dsi->header.dsi_requestID);
 
  /* make sure we don't over-write our buffers. */
  *rlength = min(ntohl(dsi->header.dsi_len), ilength);
  if (my_dsi_stream_read(dsi, buf, *rlength) != *rlength)
    return 0;
 
  return block[1];
}

/* ======================================================= */
size_t my_dsi_stream_write(DSI *dsi, void *data, const size_t length)
{
size_t written;
ssize_t len;

  written = 0;
  while (written < length) {
    if (((len = write(dsi->socket, (u_int8_t *) data + written,
		      length - written)) == -1 && errno == EINTR) ||
	!len)
      continue;

    if (len < 0) {
      fprintf(stderr, "dsi_stream_write: %s\n", strerror(errno));
      break;
    }
    
    written += len;
  }

  dsi->write_count += written;
  return written;
}

/* --------------------------------------------------- */
/* write data. 0 on failure. this assumes that dsi_len will never
 * cause an overflow in the data buffer. */
static int use_writev = 1;
int my_dsi_stream_send(DSI *dsi, void *buf, size_t length)
{
  char block[DSI_BLOCKSIZ];
  struct iovec iov[2];
  size_t towrite;
  ssize_t len;
      
  block[0] = dsi->header.dsi_flags;
  block[1] = dsi->header.dsi_command;

  memcpy(block + 2, &dsi->header.dsi_requestID, sizeof(dsi->header.dsi_requestID));
  memcpy(block + 4, &dsi->header.dsi_code, sizeof(dsi->header.dsi_code));
  memcpy(block + 8, &dsi->header.dsi_len, sizeof(dsi->header.dsi_len));
  memcpy(block + 12, &dsi->header.dsi_reserved, sizeof(dsi->header.dsi_reserved));

  if (!length) { /* just write the header */
    length = (my_dsi_stream_write(dsi, block, sizeof(block)) == sizeof(block));
    return length; /* really 0 on failure, 1 on success */
  }
  if (use_writev) {
  	iov[0].iov_base = block;
  	iov[0].iov_len = sizeof(block);
  	iov[1].iov_base = buf;
  	iov[1].iov_len = length;
 
  	towrite = sizeof(block) + length;
  	dsi->write_count += towrite;
  	while (towrite > 0) {
    		if (((len = writev(dsi->socket, iov, 2)) == -1 && errno == EINTR) || !len)
      		continue;
 
	    	if (len == towrite) /* wrote everything out */
      			break;
	    	else if (len < 0) { /* error */
      			fprintf(stderr, "my_dsi_stream_send: %s", strerror(errno));
      			return 0;
	    	}
 
    		towrite -= len;
	    	if (towrite > length) { /* skip part of header */
      			iov[0].iov_base = (char *) iov[0].iov_base + len;
      			iov[0].iov_len -= len;
	    	} else { /* skip to data */
      			if (iov[0].iov_len) {
        			len -= iov[0].iov_len;
        			iov[0].iov_len = 0;
	      		}
      			iov[1].iov_base = (char *) iov[1].iov_base + len;
      			iov[1].iov_len -= len;
	    	}
	  }
  }  
  else {
  	/* write the header then data */
  	if ((my_dsi_stream_write(dsi, block, sizeof(block)) != sizeof(block)) ||
      		(my_dsi_stream_write(dsi, buf, length) != length)) {
    	    return 0;
	}
  }

  return 1;
}

/* ------------------------------------- */
void my_dsi_tickle(DSI *dsi)
{
char block[DSI_BLOCKSIZ];
u_int16_t id;
 
	id = htons(dsi_clientID(dsi));
 
  	memset(block, 0, sizeof(block));
  	block[0] = DSIFL_REQUEST;
  	block[1] = DSIFUNC_TICKLE;
  	memcpy(block + 2, &id, sizeof(id));
  	/* code = len = reserved = 0 */
 
  	my_dsi_stream_write(dsi, block, DSI_BLOCKSIZ);
}

/* ------------------------------------- */
int my_dsi_receive(DSI *x)
{
int ret;

	while (1) {
		ret = my_dsi_stream_receive(x, x->commands, DSI_CMDSIZ, &x->cmdlen);
		if (ret == DSIFUNC_ATTN) {
			continue;
		}
		else if (ret == DSIFUNC_TICKLE) {
			my_dsi_tickle(x);
		}
		else
			break;
	}
	return ret;
}

/* ------------------------------------- */
int my_dsi_data_receive(DSI *x)
{
int ret;

	while (1) {
		ret = my_dsi_stream_receive(x, x->data, DSI_DATASIZ, &x->datalen);
		if (ret == DSIFUNC_ATTN) {
			continue;
		}
		else if (ret == DSIFUNC_TICKLE) {
			my_dsi_tickle(x);
		}
		else
			break;
	}
	return ret;
}

/* ------------------------------- */
static void SendInit(DSI *dsi)
{
	memset(dsi->commands, 0, DSI_CMDSIZ);
	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_CMD;
	dsi->header.dsi_requestID = htons(dsi_clientID(dsi));
}

/* ------------------------------- */
static int  SendCmd(DSI *dsi, int cmd)
{
int ofs;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = cmd;

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	return my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
}

/* ------------------------------- */
static int  SendCmdWithU16(DSI *dsi, int cmd, u_int16_t param)
{
int ofs;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = cmd;
	dsi->commands[ofs++] = 0;
	memcpy(dsi->commands +ofs, &param, sizeof(param));
	ofs += sizeof(param);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	return my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
}

/* ------------------------- */
static int SendCmdVolDidCname(CONN *conn, int cmd, u_int16_t vol, int did , char *name)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = cmd;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &vol, sizeof(vol));
	ofs += sizeof(vol);
	
	memcpy(dsi->commands +ofs, &did, sizeof(did));
	ofs += sizeof(did);

    ofs = FPset_name(conn, ofs, name);
		
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);

	return(dsi->header.dsi_code);
}

/* -----------------------------------------------
   Open a new session 
*/
int DSIOpenSession(CONN *conn)
{
DSI *dsi;
u_int32_t i = 0; 

	dsi = &conn->dsi;
	/* DSIOpenSession */
	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_OPEN;
	dsi->header.dsi_requestID = htons(dsi_clientID(dsi));

	dsi->cmdlen = 2 + sizeof(i);
	dsi->commands[0] = DSIOPT_ATTNQUANT;
  	dsi->commands[1] = sizeof(i);
  	i = htonl(DSI_DEFQUANT);
  	memcpy(dsi->commands + 2, &i, sizeof(i));	    
	my_dsi_send(dsi);
	my_dsi_receive(dsi);

#if 0
	dump_header(dsi);
	if (dsi->header.dsi_command == DSIFUNC_OPEN) {
		dump_open(dsi);
	}
#endif		

	if (!dsi->header.dsi_code) {
		memcpy(&i, dsi->commands +2,sizeof(i));
		i = ntohl(i);
		dsi->server_quantum = i;
	}
	else {
		dsi->server_quantum = 0;
	}
	return dsi->header.dsi_code;
}

/* -----------------------------------------------
   GetStatus
*/
int DSIGetStatus(CONN *conn)
{
DSI *dsi;

	dsi = &conn->dsi;

	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_STAT;
	dsi->header.dsi_requestID = htons(dsi_clientID(dsi));

	dsi->cmdlen = 0;
	my_dsi_send(dsi);
	my_dsi_receive(dsi);

	return dsi->header.dsi_code;
}

/* -----------------------------------------------
   Close Session
   no reply
*/
int DSICloseSession(CONN *conn)
{
DSI *dsi;

	dsi = &conn->dsi;

	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_CLOSE;
	dsi->header.dsi_requestID = htons(dsi_clientID(dsi));

	dsi->cmdlen = 0;
	my_dsi_send(dsi);
	return 0;
}

/* ==============================================
	spec violation in netatalk 
	FPlogout ==> dsiclose
*/
int AFPopenLogin(CONN *conn, char *vers, char *uam, char *usr, char *pwd)
{
u_int8_t len;
int ofs;
DSI *dsi = &conn->dsi;

	if (DSIOpenSession(conn)) {
		return dsi->header.dsi_code;
	}
	/* -------------- */
	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_LOGIN;

	len = strlen(vers);
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], vers, len);
	ofs += len;
		
	len = strlen(uam);
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], uam, len);
	ofs += len;

	len = strlen(usr);
	if (len) {
		dsi->commands[ofs++] = len;
		strncpy(&dsi->commands[ofs], usr, len);
		ofs += len;

		len = strlen(pwd);
		if (ofs & 1) {
			dsi->commands[ofs++] = 0;
		}

		strncpy(&dsi->commands[ofs], pwd, len);
		ofs += PASSWDLEN;
	}
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ---------------------------- */
int AFPopenLoginExt(CONN *conn, char *vers, char *uam, char *usr, char *pwd)
{
u_int8_t len;
u_int16_t len16;
u_int16_t temp16;
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	if (DSIOpenSession(conn)) {
		return dsi->header.dsi_code;
	}
	/* -------------- */
	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_LOGIN_EXT;
	dsi->commands[ofs++] = 0; /* pad */

	temp16 = 0;		/* flag */
	memcpy(&dsi->commands[ofs], &temp16, sizeof(temp16));
	ofs += sizeof(temp16);

	len = strlen(vers);
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], vers, len);
	ofs += len;
		
	len = strlen(uam);
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], uam, len);
	ofs += len;


	len16 = strlen(usr);
	/* user name */
	dsi->commands[ofs++] = 3;
	temp16 = htons(len16);
	memcpy(&dsi->commands[ofs], &temp16, sizeof(temp16));
	ofs += sizeof(temp16);
	strncpy(&dsi->commands[ofs], usr, len16);
	ofs += len16;

	/* directory service */
	dsi->commands[ofs++] = 3;
	temp16 = 0;
	memcpy(&dsi->commands[ofs], &temp16, sizeof(temp16));
	ofs += sizeof(temp16);

	if (ofs & 1) {
		dsi->commands[ofs++] = 0;
	}
	
	/* now the uam parts */
#if 0		
	len = len16;
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], usr, len);
	ofs += len;
#endif
	/* HACK */
	if (len16) {
		len = strlen(pwd);
		strncpy(&dsi->commands[ofs], pwd, len);
		ofs += PASSWDLEN;
	}
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* --------------------------- */
int AFPChangePW(CONN *conn, char *uam, char *usr, char *opwd, char *pwd)
{
u_int8_t len;
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_CHANGEPW;
	dsi->commands[ofs++] = 0;

	len = strlen(uam);
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], uam, len);
	ofs += len;
	if (ofs & 1) {
		dsi->commands[ofs++] = 0;
	}

	len = strlen(usr);
	if (len) {
		dsi->commands[ofs++] = len;
		strncpy(&dsi->commands[ofs], usr, len);
		ofs += len;

		len = strlen(opwd);
		if (ofs & 1) {
			dsi->commands[ofs++] = 0;
		}

		strncpy(&dsi->commands[ofs], opwd, len);
		ofs += PASSWDLEN;

		len = strlen(pwd);

		strncpy(&dsi->commands[ofs], pwd, len);
		ofs += PASSWDLEN;
	}
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}
/* ------------------------------- */
int AFPLogOut(CONN *conn)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmd(dsi,AFP_LOGOUT);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPGetSrvrInfo(CONN *conn)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmd(dsi,AFP_GETSRVINFO);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPGetSrvrParms(CONN *conn)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmd(dsi,AFP_GETSRVPARAM);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPGetSrvrMsg(CONN *conn, u_int16_t type, u_int16_t bitmap)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;
	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_GETSRVRMSG;
	dsi->commands[ofs++] = 0;

	type = htons(type);
	memcpy(dsi->commands +ofs, &type, sizeof(type));
	ofs += sizeof(type);

	bitmap = htons(bitmap);
	memcpy(dsi->commands +ofs, &bitmap, sizeof(bitmap));
	ofs += sizeof(bitmap);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);

	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* -------------------------------  */
int AFPCloseVol(CONN *conn, u_int16_t vol)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmdWithU16(dsi, AFP_CLOSEVOL, vol);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* -------------------------------  */
int AFPCloseDT(CONN *conn, u_int16_t vol)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmdWithU16(dsi, AFP_CLOSEDT, vol);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPCloseFork(CONN *conn, u_int16_t fork)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmdWithU16(dsi, AFP_CLOSEFORK, fork);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPByteLock(CONN *conn, u_int16_t fork, int end, int mode, int offset, int size )
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_BYTELOCK;
	dsi->commands[ofs++] = (end?0x80:0)| (mode?1:0);

	memcpy(dsi->commands +ofs, &fork, sizeof(fork));
	ofs += sizeof(fork);

	offset = htonl(offset);
	memcpy(dsi->commands +ofs, &offset, sizeof(offset));  
	ofs += sizeof(offset);

	size = htonl(size);
	memcpy(dsi->commands +ofs, &size, sizeof(size));  
	ofs += sizeof(size);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);

	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ---------------------------- */
static off_t get_off_t(ibuf, is64)
char    **ibuf;
int     is64;
{
    u_int32_t             temp;
    off_t                 ret;
 
    ret = 0;
    memcpy(&temp, *ibuf, sizeof( temp ));
    ret = ntohl(temp); /* ntohl is unsigned */
    *ibuf += sizeof(temp);
 
    if (is64) {
        memcpy(&temp, *ibuf, sizeof( temp ));
        *ibuf += sizeof(temp);
        ret = ntohl(temp)| (ret << 32);
    }
    else {
        ret = (int)ret; /* sign extend */
    }
    return ret;
}

/* ------------------------------- */
static int set_off_t(offset, rbuf, is64)
off_t   offset;
char    *rbuf;
int     is64;
{
	u_int32_t  temp;
        int        ret;
         
	ret = 0;
    if (is64) {
    	temp = htonl(offset >> 32);
        memcpy(rbuf, &temp, sizeof( temp ));
        rbuf += sizeof( temp );
        ret = sizeof( temp );
        offset &= 0xffffffff;
	}
    temp = htonl(offset);
    memcpy(rbuf, &temp, sizeof( temp ));
    ret += sizeof( temp );
    return ret;
}

/* ------------------------------- */
int AFPByteLock_ext(CONN *conn, u_int16_t fork, int end, int mode, off_t offset, off_t size )
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_BYTELOCK_EXT;
	dsi->commands[ofs++] = (end?0x80:0)| (mode?1:0);

	memcpy(dsi->commands +ofs, &fork, sizeof(fork));
	ofs += sizeof(fork);

	ofs += set_off_t(offset, dsi->commands +ofs,1);

	ofs += set_off_t(size, dsi->commands +ofs,1);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);

	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
unsigned int AFPSetForkParam(CONN *conn, u_int16_t fork,  u_int16_t bitmap, off_t size)
{
int ofs;
DSI *dsi;
int is64 = bitmap & ((1 << FILPBIT_EXTDFLEN) | (1 << FILPBIT_EXTRFLEN));

	dsi = &conn->dsi;
	memset(dsi->commands, 0, DSI_CMDSIZ);
	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_CMD;
	dsi->header.dsi_requestID = dsi_clientID(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_SETFORKPARAM;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &fork, sizeof(fork));	/* fork num */
	ofs += sizeof(fork);

	bitmap = htons(bitmap);
	memcpy(dsi->commands +ofs, &bitmap, sizeof(bitmap));  /* bitmap */
	ofs += sizeof(bitmap);

	ofs += set_off_t(size, dsi->commands +ofs, is64);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return dsi->header.dsi_code;
}

/* ------------------------------- */
int AFPFlush(CONN *conn, u_int16_t vol)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmdWithU16(dsi, AFP_FLUSH, vol);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* -------------------------------  */
int AFPFlushFork(CONN *conn, u_int16_t vol)
{
DSI *dsi;

	dsi = &conn->dsi;
	SendCmdWithU16(dsi, AFP_FLUSHFORK, vol);
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* -------------------------------  */
u_int16_t AFPOpenVol(CONN *conn, char *vol, u_int16_t bitmap)
{
int ofs;
u_int16_t result, volID = 0;
int len;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_OPENVOL;
	dsi->commands[ofs++] = 0;

 	bitmap = htons( bitmap);
  	memcpy(dsi->commands + ofs, &bitmap, sizeof(bitmap));	    
	ofs += 2;

	len = strlen(vol);
	dsi->commands[ofs++] = len;
	strncpy(&dsi->commands[ofs], vol, len);
	ofs += len;
    if ((len + 1) & 1) /* pad to an even boundary */
		ofs++;

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	if (!dsi->header.dsi_code) {
		ofs = 0;
		/* copy of bitmap */
		memcpy(&result, dsi->commands, sizeof(result));
		ofs += sizeof(result);
		/* volume attribute  */
		memcpy(&result, dsi->commands, sizeof(result));
		ofs += sizeof(result);
		/*  volume ID */
		memcpy(&result, dsi->commands +ofs, sizeof(result));
		volID = result;
	}
	return(volID);
}

/* -------------------------------  */
/* Converts Pascal string to C (null-terminated) string. */
int strp2c(char *cstr, char *pstr) 
{
    int i;

    for (i = 0; i < pstr[0]; i++)
        cstr[i] = pstr[i+1];
    cstr[i] = '\0';
    return i;
}

/* -------------------------------  */
/* Converts null-terminated C string to Pascal form */
int strc2p(char *pstr, char *cstr) 
{
    int i;

    for (i = 0; cstr[i] != 0; i++)
        pstr[i+1] = cstr[i];
    pstr[0] = i;
    return i;
}

/* -------------------------------  */
/* Our malloc wrapper. It zeroes allocated memory. */
void *fp_malloc(size_t size)
{
    void *ret = malloc(size);
    if (ret == NULL) {
        fprintf(stderr, "fp_malloc: out of memory !\n");
        abort();
    }
    memset(ret, 0, size);
    return ret;
}

/* -------------------------------  */
char *strp2cdup(char *src) 
{
char *r = fp_malloc(*src +1);

	strp2c(r, src);
	return r;
}

/* -------------------------------  */
void *fp_realloc(void *ptr, size_t size)
{
    void *ret = realloc(ptr, size);
    if (ret == NULL) {
        fprintf(stderr, "fp_realloc: out of memory !\n");
        abort();
    }
    return ret;
}

/* -------------------------------  */
/* Our free wrapper. It does nothing special at the moment. */
void fp_free(void *ptr) 
{
    if (ptr != NULL)
        free(ptr);
}

/* -------------------------------  */
void afp_volume_unpack(struct afp_volume_parms *parms, char *b, u_int16_t rbitmap)
{
    u_int16_t i;
    u_int32_t l;

    if (rbitmap & (1 << VOLPBIT_ATTR)) {
        memcpy(&i, b, sizeof(i)); b += sizeof(i);
        parms->attr = ntohs(i);
    }
    if (rbitmap & (1 << VOLPBIT_SIG)) {
        memcpy(&i, b, sizeof(i)); b += sizeof(i);
        parms->sig = ntohs(i);
    }
    if (rbitmap & (1 << VOLPBIT_CDATE)) {
        memcpy(&l, b, sizeof(l)); b += sizeof(l);
        parms->cdate = ntohl(l);
    }
    if (rbitmap & (1 << VOLPBIT_MDATE)) {
        memcpy(&l, b, sizeof(l)); b += sizeof(l);
        parms->mdate = ntohl(l);
    }
    if (rbitmap & (1 << VOLPBIT_BDATE)) {
        memcpy(&l, b, sizeof(l)); b += sizeof(l);
        parms->bdate = ntohl(l);
    }
    if (rbitmap & (1 << VOLPBIT_VID)) {
        memcpy(&i, b, sizeof(i)); b += sizeof(i);
        parms->vid = i;
    }
    if (rbitmap & (1 << VOLPBIT_BFREE)) {
        memcpy(&l, b, sizeof(l)); b += sizeof(l);
        parms->bfree = ntohl(l);
    }
    if (rbitmap & (1 << VOLPBIT_BTOTAL)) {
        memcpy(&l, b, sizeof(l)); b += sizeof(l);
        parms->btotal = ntohl(l);
    }
    if (rbitmap & (1 << VOLPBIT_NAME)) {
        parms->name = (char*)malloc(b[0]+1);
        l = strp2c(parms->name, b) + 1;
        b += l;
    }
    if (rbitmap & (1 << VOLPBIT_XBFREE)) {
        // FIXME
    }
    if (rbitmap & (1 << VOLPBIT_XBTOTAL)) {
        // FIXME
    }
    if (rbitmap & (1 << VOLPBIT_BSIZE)) {
        memcpy(&l, b, sizeof(l)); b += sizeof(l);
        parms->bsize = ntohl(l);
    }
} 

/* -------------------------------  
 * Only backup date is valid.
*/
int afp_volume_pack(char *b, struct afp_volume_parms *parms, u_int16_t bitmap)
{
    u_int16_t i;
    u_int32_t l;
    int bit = 0;
	char *beg = b;

	while (bitmap != 0) {
		while (( bitmap & 1 ) == 0 ) {
			bitmap = bitmap>>1;
	        bit++;
	    }
	    switch ( bit ) { /* Common parameters */
    	case VOLPBIT_ATTR:
	        memcpy(&i, b, sizeof(i)); b += sizeof(i);
    	    parms->attr = ntohs(i);
        	break;
    	case VOLPBIT_SIG:
	        // FIXME
    	    break;
    	case VOLPBIT_CDATE:
        	l = htonl(parms->cdate);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
	    	break;
    	case VOLPBIT_MDATE:
        	l = htonl(parms->mdate);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
    		break;
    	case VOLPBIT_BDATE:
        	l = htonl(parms->bdate);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
    		break;
    	case VOLPBIT_VID:
	    	break;
		case VOLPBIT_BFREE:
			break;
    	case VOLPBIT_BTOTAL:
    		break;
    	case VOLPBIT_NAME:
	    	break;
    	case VOLPBIT_XBFREE:
    		break;
    	case VOLPBIT_XBTOTAL:
    		break;
    	case VOLPBIT_BSIZE:
	    	break;
        }
        bitmap = bitmap>>1;
        bit++;
    }
    return (b -beg);
}

/* -------------------------------------- */
// FIXME: redundant bitmap parameters !
// FIXME: some of those parameters are not tested.
void afp_filedir_unpack(struct afp_filedir_parms *filedir, char *b, u_int16_t rfbitmap, u_int16_t rdbitmap)
{
    char isdir;
    u_int16_t i;
    u_int32_t l;
    int r;
    int bit = 0;
	u_int16_t bitmap;
	char *beg = b;

    isdir = filedir->isdir;
	bitmap = (isdir)?rdbitmap:rfbitmap;
	while (bitmap != 0) {
		while (( bitmap & 1 ) == 0 ) {
			bitmap = bitmap>>1;
	        bit++;
	    }
	    switch ( bit ) { /* Common parameters */
	    case FILPBIT_ATTR: /* DIRPBIT_ATTR */
        	memcpy(&i, b, sizeof(i)); b += sizeof(i);
        	filedir->attr = ntohs(i);
        	break;
        case FILPBIT_PDID: /* DIRPBIT_PDID */
        	memcpy(&l, b, sizeof(l)); b += sizeof(l);
        	filedir->pdid = ntohl(l);
        	break;
        case FILPBIT_CDATE: /* DIRPBIT_CDATE */
        	memcpy(&l, b, sizeof(l)); b += sizeof(l);
        	filedir->cdate = ntohl(l);
        	break;
        case FILPBIT_MDATE: /* DIRPBIT_MDATE */
        	memcpy(&l, b, sizeof(l)); b += sizeof(l);
        	filedir->mdate = ntohl(l);
        	break;
    	case FILPBIT_BDATE: /* DIRPBIT_BDATE */
        	memcpy(&l, b, sizeof(l)); b += sizeof(l);
        	filedir->bdate = ntohl(l);
        	break;
    	case FILPBIT_FINFO: /* DIRPBIT_FINFO */
        	memcpy(filedir->finder_info, b, 32); 
        	b += 32;
        	break;
		case FILPBIT_LNAME: /* DIRPBIT_LNAME */
        	memcpy(&i, b, sizeof(i)); b += sizeof(i);
        	i = ntohs(i);
        	if (i != 0) {
            	filedir->lname = fp_malloc((beg+i)[0]+1);
            	r = strp2c(filedir->lname, beg+i) + 1;
            }
            break;
        case FILPBIT_SNAME:	/* DIRPBIT_SNAME */
        	memcpy(&i, b, sizeof(i)); b += sizeof(i);
        	i = ntohs(i);
        	if (i != 0) {
            	filedir->sname = fp_malloc((beg+i)[0]+1);
            	r = strp2c(filedir->sname, beg+i) + 1;
        	}
        	break;
        case FILPBIT_FNUM: /* DIRPBIT_DID */
        	memcpy(&l, b, sizeof(l)); b += sizeof(l);
        	filedir->did = l;
        	break;
        case FILPBIT_PDINFO: /* utf8 name */
        	memcpy(filedir->pdinfo, b, sizeof(filedir->pdinfo));
        	b += sizeof(filedir->pdinfo);
        	break;
        default: /* File specific parameters */
        	if (!isdir) switch (bit) {
        		case FILPBIT_DFLEN:
        			memcpy(&l, b, sizeof(l)); b += sizeof(l);
        			filedir->dflen = ntohl(l);
        			break;
        		case FILPBIT_RFLEN:
        			memcpy(&l, b, sizeof(l)); b += sizeof(l);
        			filedir->rflen = ntohl(l);
        			break;
        	}
        	else switch (bit) { /* Directory specific parametrs */
        		case DIRPBIT_OFFCNT:
        			memcpy(&i, b, sizeof(i)); b += sizeof(i);
        			filedir->offcnt = ntohs(i);
        			break;
        		case DIRPBIT_UID:
			        memcpy(&l, b, sizeof(l)); b += sizeof(l);
			        filedir->uid = ntohl(l);
			        break;
			    case DIRPBIT_GID:
        			memcpy(&l, b, sizeof(l)); b += sizeof(l);
        			filedir->gid = ntohl(l);
        			break;
				case DIRPBIT_ACCESS:
        			memcpy(filedir->access, b, sizeof(filedir->access));
        			b += sizeof(filedir->access);
        			break;
        	}
        	break;
        }
        bitmap = bitmap>>1;
        bit++;
    }
} 

/* ---------------------- */
int afp_filedir_pack(char *b, struct afp_filedir_parms *filedir, u_int16_t rfbitmap, u_int16_t rdbitmap)
{
    char isdir;
    u_int16_t i;
    u_int32_t l;
    int bit = 0;
	u_int16_t bitmap;
	char *beg = b;

    isdir = filedir->isdir;
	bitmap = (isdir)?rdbitmap:rfbitmap;
	while (bitmap != 0) {
		while (( bitmap & 1 ) == 0 ) {
			bitmap = bitmap>>1;
	        bit++;
	    }
	    switch ( bit ) { /* Common parameters */
	    case FILPBIT_ATTR: /* DIRPBIT_ATTR */
	    	i = htons(filedir->attr);
        	memcpy(b, &i, sizeof(i)); b += sizeof(i);
        	break;
        case FILPBIT_PDID: /* DIRPBIT_PDID */
        	l = htonl(filedir->pdid);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
        	break;
        case FILPBIT_CDATE: /* DIRPBIT_CDATE */
        	l = htonl(filedir->cdate);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
        	break;
        case FILPBIT_MDATE: /* DIRPBIT_MDATE */
        	l = htonl(filedir->mdate);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
        	break;
    	case FILPBIT_BDATE: /* DIRPBIT_BDATE */
        	l = htonl(filedir->bdate);
        	memcpy(b, &l, sizeof(l)); b += sizeof(l);
        	break;
    	case FILPBIT_FINFO: /* DIRPBIT_FINFO */
        	memcpy(b, filedir->finder_info, 32); 
        	b += 32;
        	break;
		case FILPBIT_LNAME: /* DIRPBIT_LNAME */
        case FILPBIT_SNAME:	/* DIRPBIT_SNAME */
        case FILPBIT_FNUM: /* DIRPBIT_DID */
        case FILPBIT_PDINFO: /* utf8 name */
            /* error */
        	break;
        default: /* File specific parameters */
        	if (!isdir) switch (bit) {
        		case FILPBIT_DFLEN:
        		case FILPBIT_RFLEN:
        			/* error */
        			break;
        	}
        	else switch (bit) { /* Directory specific parametrs */
        		case DIRPBIT_OFFCNT:
        			break;
        		case DIRPBIT_UID:
        			l = htonl(filedir->uid);
        			memcpy(b, &l, sizeof(l)); b += sizeof(l);
			        break;
			    case DIRPBIT_GID:
        			l = htonl(filedir->gid);
        			memcpy(b, &l, sizeof(l)); b += sizeof(l);
        			break;
				case DIRPBIT_ACCESS:
        			memcpy(b, filedir->access, sizeof(filedir->access));
        			b += sizeof(filedir->access);
        			break;
        	}
        	break;
        }
        bitmap = bitmap>>1;
        bit++;
    }
    return (b -beg);
} 

/* -------------------------------  */
int AFPGetVolParam(CONN *conn, u_int16_t vol, u_int16_t bitmap)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_GETVOLPARAM;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &vol, sizeof(vol));	/* volume */
	ofs += sizeof(vol);

 	bitmap = htons(bitmap);
  	memcpy(dsi->commands + ofs, &bitmap, sizeof(bitmap));	    
	ofs += 2;

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* -------------------------------  */
int AFPSetVolParam(CONN *conn, u_int16_t vol, u_int16_t bitmap, struct afp_volume_parms *parms)
{
int ofs;
DSI *dsi;
u_int16_t tp;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_SETVOLPARAM;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &vol, sizeof(vol));	/* volume */
	ofs += sizeof(vol);

 	tp = htons(bitmap);
  	memcpy(dsi->commands + ofs, &tp, sizeof(tp));	    
	ofs += sizeof(tp);
	
	ofs += afp_volume_pack(dsi->commands + ofs, parms, bitmap);
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ---------------------
	FIXME for now, replace only / with 0. Should deal with ..
*/
void u2mac(char *dst, char *name, int len)
{
	while (len) {
		if (*name == '/') {
			*dst = 0;
		}
		else if (*name == '!') {
			*dst = '/';
		}
		else {
			*dst = *name;
		}
		dst++;
		name++;
		len--;
	}
}

/* ------------------------- */
int FPset_name(CONN *conn, int ofs, char *name)
{
int len;
u_int16_t len16;
u_int32_t hint = htonl(kTextEncodingUTF8);
DSI *dsi;

	dsi = &conn->dsi;

	if (UNICODE(conn)) {
		dsi->commands[ofs++] = 3;		/* long name */
		memcpy(&dsi->commands[ofs], &hint, sizeof(hint));
		ofs += sizeof(hint);

		len = strlen(name);
		len16 = htons(len);
		memcpy(&dsi->commands[ofs], &len16, sizeof(len16));
		ofs += sizeof(len16);

		u2mac(&dsi->commands[ofs], name, len);
		ofs += len;
	}
	else {
		dsi->commands[ofs++] = 2;		/* long name */
		len = strlen(name);
		dsi->commands[ofs++] = len;
		u2mac(&dsi->commands[ofs], name, len);
		ofs += len;
    }
    return ofs;
}

/* ------------------------- */
unsigned int  AFPCreateFile(CONN *conn, u_int16_t vol, char type, int did , char *name)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_CREATEFILE;
	dsi->commands[ofs++] = type?0x80:0;
	
	memcpy(dsi->commands +ofs, &vol, sizeof(vol));	/* volume */
	ofs += sizeof(vol);

	memcpy(dsi->commands +ofs, &did, sizeof(did));  /* directory did */
	ofs += sizeof(did);

    ofs = FPset_name(conn, ofs, name);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPWrite(CONN *conn, u_int16_t fork, int offset, int size, char *data, char whence)
{
int ofs;
int rsize;
DSI *dsi;
u_int32_t last;
u_int32_t temp;

	dsi = &conn->dsi;

	memset(dsi->commands, 0, DSI_CMDSIZ);
	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_WRITE;
	dsi->header.dsi_requestID = htons(dsi_clientID(dsi));

	ofs = 0;
	dsi->commands[ofs++] = AFP_WRITE;
	dsi->commands[ofs++] = whence;			/* 0 SEEK_SET, 0x80 SEEK_END */
	
	memcpy(dsi->commands +ofs, &fork, sizeof(fork));	/* fork num */
	ofs += sizeof(fork);

	temp = htonl(offset);
	memcpy(dsi->commands +ofs, &temp, sizeof(temp));  
	ofs += sizeof(temp);

	rsize = htonl(size);
	memcpy(dsi->commands +ofs, &rsize, sizeof(rsize));  
	ofs += sizeof(rsize);

	dsi->datalen = ofs;		/* 12*/
	dsi->header.dsi_len = htonl(dsi->datalen +size);
	dsi->header.dsi_code = htonl(ofs); 
 
   	my_dsi_stream_send(dsi, dsi->commands, ofs);
	my_dsi_stream_write(dsi, data, size);
	my_dsi_receive(dsi);
	if (!dsi->header.dsi_code) {
		memcpy(&last, dsi->commands, sizeof(last));
		last = ntohl(last);
		if (whence) {
			/* we don't know the size! */
			return (last >= size +offset)?0:-1;
		}
		return (last == size +offset)?0:-1;
	}
	return dsi->header.dsi_code;
}

/* ------------------------------- */
int AFPWrite_ext(CONN *conn, u_int16_t fork, off_t offset, off_t size, char *data, char whence)
{
int ofs;
DSI *dsi;
off_t last;
char *ptr;
	dsi = &conn->dsi;

	memset(dsi->commands, 0, DSI_CMDSIZ);
	dsi->header.dsi_flags = DSIFL_REQUEST;     
	dsi->header.dsi_command = DSIFUNC_WRITE;
	dsi->header.dsi_requestID = htons(dsi_clientID(dsi));
	ofs = 0;
	dsi->commands[ofs++] = AFP_WRITE_EXT;
	dsi->commands[ofs++] = whence;			/* 0 SEEK_SET, 0x80 SEEK_END */
	
	memcpy(dsi->commands +ofs, &fork, sizeof(fork));	/* fork num */
	ofs += sizeof(fork);

	ofs += set_off_t(offset, dsi->commands +ofs,1);

	ofs += set_off_t(size, dsi->commands +ofs,1);

	dsi->datalen = ofs;		
	dsi->header.dsi_len = htonl(dsi->datalen +size);
	dsi->header.dsi_code = htonl(ofs); 
 
   	my_dsi_stream_send(dsi, dsi->commands, ofs);
	my_dsi_stream_write(dsi, data, size);
	my_dsi_receive(dsi);
	if (!dsi->header.dsi_code) {
		ptr = dsi->commands;
		last = get_off_t(&ptr, 1);
		if (whence) {
			/* we don't know the size! */
			return (last >= size +offset)?0:-1;
		}
		return (last == size +offset)?0:-1;
	}
	return dsi->header.dsi_code;
}


/* ------------------------------- 
	type : ressource or data
*/
u_int16_t  AFPOpenFork(CONN *conn, u_int16_t vol, char type, u_int16_t bitmap, int did , char *name,u_int16_t access)
{
int ofs;
u_int16_t ofork = 0, result;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_OPENFORK;
	dsi->commands[ofs++] = type;
	
	memcpy(dsi->commands +ofs, &vol, sizeof(vol));	/* volume */
	ofs += sizeof(vol);

	memcpy(dsi->commands +ofs, &did, sizeof(did));  /* directory did */
	ofs += sizeof(did);

	bitmap = htons(bitmap);
	memcpy(dsi->commands +ofs, &bitmap, sizeof(bitmap));  /* bitmap */
	ofs += sizeof(bitmap);

	access =  htons (access  );
	memcpy(dsi->commands +ofs, &access, sizeof(access));  /* access right */
	ofs += sizeof(access);

    ofs = FPset_name(conn, ofs, name);
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	if (!dsi->header.dsi_code) {
		ofs = 0;
		memcpy(&result, dsi->commands, sizeof(result));			/* bitmap */
		ofs += sizeof(result);
		memcpy(&result, dsi->commands +ofs, sizeof(result));
		ofork = result;
	}
	return ofork;
}

/* ------------------------------- */
unsigned int AFPDelete(CONN *conn, u_int16_t vol, int did , char *name)
{
	return  SendCmdVolDidCname(conn,AFP_DELETE , vol, did , name);
}

/* ------------------------------- */
int AFPGetComment(CONN *conn, u_int16_t vol, int did , char *name)
{
	return  SendCmdVolDidCname(conn,AFP_GETCMT , vol, did , name);
}

/* ------------------------------- */
int AFPRemoveComment(CONN *conn, u_int16_t vol, int did , char *name)
{
	return  SendCmdVolDidCname(conn,AFP_RMVCMT , vol, did , name);
}

/* ------------------------------- */
int AFPAddComment(CONN *conn, u_int16_t vol, int did , char *name, char *cmt)
{
int ofs;
int len;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_ADDCMT;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &vol, sizeof(vol));
	ofs += sizeof(vol);
	
	memcpy(dsi->commands +ofs, &did, sizeof(did));
	ofs += sizeof(did);

    ofs = FPset_name(conn, ofs, name);
	if ((ofs & 1))
	    ofs++;
	len = strlen(cmt);
	dsi->commands[ofs++] = len;
	memcpy(dsi->commands +ofs, cmt, len);
	ofs += len;
		    
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);

	return(dsi->header.dsi_code);

}

/* ------------------------------- */
int AFPGetSessionToken(CONN *conn, int type)
{
int ofs;
u_int16_t tp = htons(type);
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_GETSESSTOKEN;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &tp, sizeof(tp));
	ofs += sizeof(tp);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_data_receive(dsi);

	return(dsi->header.dsi_code);

}

/* ------------------------------- */
int AFPGetUserInfo(CONN *conn, char flag, int id, u_int16_t bitmap)
{
int ofs;
u_int16_t type = htons(bitmap);
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = 37; /* AFP_GETUSERINFO;*/

	memcpy(dsi->commands +ofs, &flag, sizeof(flag));
	ofs++;
	id = htonl(id);
	memcpy(dsi->commands +ofs, &id, sizeof(id));
	ofs += sizeof(id);

	memcpy(dsi->commands +ofs, &type, sizeof(type));
	ofs += sizeof(type);
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);

	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPMapID(CONN *conn, char fn, int id)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_MAPID;

	memcpy(dsi->commands +ofs, &fn, sizeof(fn));
	ofs++;
	id = htonl(id);
	memcpy(dsi->commands +ofs, &id, sizeof(id));
	ofs += sizeof(id);
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);

	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPMapName(CONN *conn, char fn, char *name )
{
int ofs;
u_int16_t len,l;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_MAPNAME;
	memcpy(dsi->commands +ofs, &fn, sizeof(fn));
	ofs++;
	len = strlen(name);
	switch(fn) {
	case 1:
	case 2:
		l = htons(len);
		memcpy(dsi->commands +ofs, &l, sizeof(l));
		ofs += sizeof(l);
		break;
	case 3:
	case 4:
	default:
		dsi->commands[ofs++] = len;
		break;
	}
	memcpy(dsi->commands +ofs, name, len);
	ofs += len;
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);

	return(dsi->header.dsi_code);
}

/* ------------------------------- */
int AFPBadPacket(CONN *conn, char fn, char *name )
{
int ofs;
u_int16_t len,l;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_MAPNAME;
	memcpy(dsi->commands +ofs, &fn, sizeof(fn));
	ofs++;
	len = 1300;
	memcpy(dsi->commands +ofs, &l, sizeof(l));
	ofs += sizeof(l);
	memcpy(dsi->commands +ofs, name, len);
	ofs += len;
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0;
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);

	return(dsi->header.dsi_code);
}


/* ------------------------------- */
int AFPRead(CONN *conn, u_int16_t fork, int offset, int size, char *data)
{
int ofs;
int rsize;
DSI *dsi;
u_int32_t  temp;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_READ;
	dsi->commands[ofs++] = 0;
	
	memcpy(dsi->commands +ofs, &fork, sizeof(fork));	/* fork num */
	ofs += sizeof(fork);

	offset = htonl(offset);
	memcpy(dsi->commands +ofs, &offset, sizeof(offset));  
	ofs += sizeof(offset);

	temp = htonl(size);
	memcpy(dsi->commands +ofs, &temp, sizeof(temp));  
	ofs += sizeof(temp);
	dsi->commands[ofs++] = 0;	/* NewLineMask */
	dsi->commands[ofs++] = 0;	/* NewLineChar */

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
#if 0	
	my_dsi_stream_receive(dsi, dsi->data, DSI_DATASIZ, &dsi->datalen);
	dump_header(dsi);
	rsize =  ntohl(dsi->header.dsi_len);
	rsize -= dsi->datalen;
	while (rsize > 0) {
	int len = min(rsize, DSI_DATASIZ);
		if (my_dsi_stream_read(dsi, dsi->data, len) != len) {
			break;
		}	
		rsize -= len;
	}	
#endif
	my_dsi_receive(dsi);
	memcpy(data, dsi->commands, dsi->cmdlen);
	rsize =  ntohl(dsi->header.dsi_len);
	rsize -= dsi->cmdlen;
	my_dsi_stream_read(dsi, data +dsi->cmdlen, rsize);
	return dsi->header.dsi_code?dsi->header.dsi_code:(rsize +dsi->cmdlen== size)?0:-1;
}

/* ---------------------- 
 * Assume size < 2GB
*/
int AFPRead_ext(CONN *conn, u_int16_t fork, off_t offset, off_t size, char *data)
{
int ofs;
int rsize;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_READ_EXT;
	dsi->commands[ofs++] = 0;
	
	memcpy(dsi->commands +ofs, &fork, sizeof(fork));	/* fork num */
	ofs += sizeof(fork);

	ofs += set_off_t(offset, dsi->commands +ofs,1);

	ofs += set_off_t(size, dsi->commands +ofs,1);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
#if 0	
	my_dsi_stream_receive(dsi, dsi->data, DSI_DATASIZ, &dsi->datalen);
	dump_header(dsi);
	rsize =  ntohl(dsi->header.dsi_len);
	rsize -= dsi->datalen;
	while (rsize > 0) {
	int len = min(rsize, DSI_DATASIZ);
		if (my_dsi_stream_read(dsi, dsi->data, len) != len) {
			break;
		}	
		rsize -= len;
	}	
#endif
	my_dsi_receive(dsi);
	memcpy(data, dsi->commands, dsi->cmdlen);
	rsize =  ntohl(dsi->header.dsi_len);
	rsize -= dsi->cmdlen;
	my_dsi_stream_read(dsi, data +dsi->cmdlen, rsize);
	return dsi->header.dsi_code?dsi->header.dsi_code:(rsize +dsi->cmdlen== size)?0:-1;
}

/* -------------------------------- */
int  AFPCreateDir(CONN *conn, u_int16_t vol, int did , char *name)
{
int ofs;
int dir = 0;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_CREATEDIR;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &vol, sizeof(vol));
	ofs += sizeof(vol);
	
	memcpy(dsi->commands +ofs, &did, sizeof(did));
	ofs += sizeof(did);

    ofs = FPset_name(conn, ofs, name);
		
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_receive(dsi);
	if (!dsi->header.dsi_code) {
		ofs = 0;
		memcpy(&dir, dsi->commands, sizeof(dir));			/* did */
		ofs += sizeof(dir);
		fprintf(stderr,"directory ID 0x%x\n", ntohl(dir));
	}
	return(dir);
}

/* ------------------------------- */
int AFPGetForkParam(CONN *conn, u_int16_t fork, u_int16_t bitmap)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_GETFORKPARAM;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &fork, sizeof(fork));	/* fork num */
	ofs += sizeof(fork);

	bitmap = htons(bitmap);
	memcpy(dsi->commands +ofs, &bitmap, sizeof(bitmap));  /* bitmap */
	ofs += sizeof(bitmap);

	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_data_receive(dsi);
	return dsi->header.dsi_code;
}

/* ------------------------------- 
*/
int AFPGetAPPL(CONN *conn, u_int16_t dt, char *name, u_int16_t index, u_int16_t f_bitmap)
{
int ofs;
u_int16_t bitmap;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_GETAPPL;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &dt, sizeof(dt));
	ofs += sizeof(dt);
	
	memcpy(dsi->commands +ofs, name, 4);
	ofs += 4;

	bitmap = htons(index);
	memcpy(dsi->commands +ofs, &bitmap, sizeof(bitmap));
	ofs += sizeof(bitmap);
	
	bitmap = htons(f_bitmap);
	memcpy(dsi->commands +ofs, &bitmap, sizeof(bitmap));
	ofs += sizeof(bitmap);
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_data_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- 
*/
int AFPAddAPPL(CONN *conn, u_int16_t dt, int did, char *creator, u_int32_t tag, char *name)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_ADDAPPL;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &dt, sizeof(dt));
	ofs += sizeof(dt);

	memcpy(dsi->commands +ofs, &did, sizeof(did));
	ofs += sizeof(did);
	memcpy(dsi->commands +ofs, creator, 4);
	ofs += 4;

	memcpy(dsi->commands +ofs, &tag, sizeof(tag));
	ofs += sizeof(tag);

	ofs = FPset_name(conn, ofs, name);
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_data_receive(dsi);
	return(dsi->header.dsi_code);
}

/* ------------------------------- 
*/
int AFPRemoveAPPL(CONN *conn, u_int16_t dt, int did, char *creator, char *name)
{
int ofs;
DSI *dsi;

	dsi = &conn->dsi;

	SendInit(dsi);
	ofs = 0;
	dsi->commands[ofs++] = AFP_RMVAPPL;
	dsi->commands[ofs++] = 0;

	memcpy(dsi->commands +ofs, &dt, sizeof(dt));
	ofs += sizeof(dt);

	memcpy(dsi->commands +ofs, &did, sizeof(did));
	ofs += sizeof(did);

	memcpy(dsi->commands +ofs, creator, 4);
	ofs += 4;

	ofs = FPset_name(conn, ofs, name);
	
	dsi->datalen = ofs;
	dsi->header.dsi_len = htonl(dsi->datalen);
	dsi->header.dsi_code = 0; 
 
   	my_dsi_stream_send(dsi, dsi->commands, dsi->datalen);
	/* ------------------ */
	my_dsi_data_receive(dsi);
	return(dsi->header.dsi_code);
}

