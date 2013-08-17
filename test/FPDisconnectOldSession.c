/* ----------------------------------------------
*/
#include "specs.h"

extern char  *Server;
extern int     Port;
extern char    *Password;
extern char *vers;
extern char *uam; 

static volatile int sigp = 0;

static void pipe_handler()
{
        sigp = 1;
}


/* ------------------------- */
STATIC void test222()
{
char *name = "t222 file";
u_int16_t vol = VolID,vol2;
unsigned int ret;
char *token;
u_int32_t len;
CONN *conn2;
DSI *dsi3;
int sock;
int fork = 0, fork1;
struct sigaction action;    

	enter_test();
    fprintf(stdout,"===================\n");
    fprintf(stdout,"FPDisconnectOldSession :test222: AFP 3.x disconnect old session\n");

	if (Conn->afp_version < 30) {
		test_skipped(T_AFP3);
		goto test_exit;
	}
	if (Locking) {
		test_skipped(T_LOCKING);
		goto test_exit;
	}
	fprintf(stdout,"\tSKIPPED, FIXME need to recheck GetSessionToken 0\n");
	skipped_nomsg();
	goto test_exit;
	
	ret = FPGetSessionToken(Conn, 0, 0, 0, NULL);
	if (ret) {
		failed();
		goto test_exit;
	}
    if ((conn2 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
    	nottested();
		goto test_exit;
    }
    conn2->type = 0;
    dsi3 = &conn2->dsi;
	sock = OpenClientSocket(Server, Port);
    if ( sock < 0) {
    	nottested();
		goto test_exit;
    }
    dsi3->protocol = DSI_TCPIP; 
	dsi3->socket = sock;
	ret = FPopenLoginExt(conn2, vers, uam, User, Password);
	if (ret) {
    	nottested();
		goto test_exit;
	}
	conn2->afp_version = Conn->afp_version;

	FAIL (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) 

	vol2  = FPOpenVol(conn2, Vol);
	if (vol2 == 0xffff) {
    	nottested();
		goto fin;
	}
	fork = FPOpenFork(conn2, vol2, OPENFORK_RSCS , 0 ,DIRDID_ROOT, name, /* OPENACC_WR |OPENACC_RD |*/
	OPENACC_DWR| OPENACC_DRD);
	if (!fork) {
    	nottested();
		goto fin;
	}

	fork1 = FPOpenFork(Conn, vol, OPENFORK_RSCS , 0 ,DIRDID_ROOT, name, OPENACC_WR |OPENACC_RD| OPENACC_DWR| OPENACC_DRD);
	if (fork1) {
		FAIL (FPCloseFork(Conn,fork1))
    	nottested();
		goto fin;
	}
	
	ret = FPGetSessionToken(conn2, 0, 0, 0, NULL);
	if (ret) {
		failed();
		goto fin;
	}
	memcpy(&len, dsi3->data, sizeof(u_int32_t)); 
	len = ntohl(len);
	if (!len) {
		failed();
		goto fin;
	}
	if (!(token = malloc(len +4))) {
		fprintf(stdout, "\tFAILED malloc(%x) %s\n", len, strerror(errno));
		failed_nomsg();
		goto fin;
	}

    action.sa_handler =  SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_ONESHOT;
    if (sigaction(SIGPIPE, &action, NULL) < 0) {
    	failed();
    	goto fin;
    }

	memcpy(token, dsi3->data + sizeof(u_int32_t), len);
	/* wrong token */
	ret =  FPDisconnectOldSession(Conn, 0, len +4, token);
	if (ret != htonl(AFPERR_MISC)) {
		failed();
	}

	ret =  FPDisconnectOldSession(Conn, 0, len, token);

	if (ret != htonl(AFPERR_SESSCLOS)) {
		failed();
		goto fin;
	}
	sleep(2);

	fork1 = FPOpenFork(Conn, vol, OPENFORK_RSCS , 0 ,DIRDID_ROOT, name, OPENACC_WR |OPENACC_RD| OPENACC_DWR| OPENACC_DRD);
	if (!fork1) {
	    /* arg we are there */
		failed();
		FAIL (FPCloseFork(conn2,fork))
		goto fin;
	}
	FAIL (FPCloseFork(Conn,fork1))
	
fin:
    action.sa_handler =  SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGPIPE, &action, NULL) < 0) {
    	failed();
    }
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, name))
test_exit:
	exit_test("test222");
	    
}

/* ------------------------- */
STATIC void test338()
{
char *name = "t338 file";
u_int16_t vol, vol2;
unsigned int ret;
char *token;
u_int32_t len;
CONN *loc_conn1;
CONN *loc_conn2;
DSI *loc_dsi1, *loc_dsi2;
int sock1, sock2;
int fork = 0, fork1;
struct sigaction action;
char *id0="testsuite-test338-0";
char *id1="testsuite-test338-1";
u_int32_t time= 12345;


	enter_test();
    fprintf(stdout,"===================\n");
    fprintf(stdout,"FPDisconnectOldSession :test338: AFP 3.x disconnect old session\n");
    
	if (Conn->afp_version < 30) {
    	test_skipped(T_AFP3);
		goto test_exit;
	}
    if (Locking) {
    	test_skipped(T_LOCKING);
		goto test_exit;
	}

    /* setup 2 new connections for testing */

    /* connection 1 */
    if ((loc_conn1 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        nottested();
		goto test_exit;
    }
    loc_conn1->type = 0;
    loc_dsi1 = &loc_conn1->dsi;
    sock1 = OpenClientSocket(Server, Port);
    if ( sock1 < 0) {
        nottested();
		goto test_exit;
    }
    loc_dsi1->protocol = DSI_TCPIP;
    loc_dsi1->socket = sock1;
    ret = FPopenLoginExt(loc_conn1, vers, uam, User, Password);
    if (ret) {
        nottested();
		goto test_exit;
    }
    loc_conn1->afp_version = Conn->afp_version;


    ret = FPGetSessionToken(loc_conn1, 3, time, strlen(id0), id0);
    if (ret) {
        failed();
        goto fin;
    }

    memcpy(&len, loc_dsi1->data, sizeof(u_int32_t));
    len = ntohl(len);
    if (!len) {
        failed();
        goto fin;
    }
    if (!(token = malloc(len + 4))) {
        fprintf(stdout, "\tFAILED malloc(%x) %s\n", len, strerror(errno));
        failed_nomsg();
        goto fin;
    }
    memcpy(token, loc_dsi1->data + sizeof(u_int32_t), len);

    if (0xffff == (vol = FPOpenVol(loc_conn1, Vol))) {
        nottested();
		goto test_exit;
    }
    FAIL(FPCreateFile(loc_conn1, vol,  0, DIRDID_ROOT , name));
    fork = FPOpenFork(loc_conn1, vol, OPENFORK_DATA , 0, DIRDID_ROOT, name, OPENACC_WR |OPENACC_RD);
    if (!fork)
        failed();

    /* done connection 1 */

    /* --------------------------------- */
    /* connection 2 */
    if ((loc_conn2 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        nottested();
		goto test_exit;
    }

    loc_conn2->type = 0;
    loc_dsi2 = &loc_conn2->dsi;
    sock2 = OpenClientSocket(Server, Port);
    if ( sock2 < 0) {
        nottested();
        goto fin;
    }

    loc_dsi2->protocol = DSI_TCPIP;
    loc_dsi2->socket = sock2;
    ret = FPopenLoginExt(loc_conn2, vers, uam, User, Password);
    if (ret) {
        nottested();
		goto test_exit;
    }
    loc_conn2->afp_version = Conn->afp_version;

    FAIL (FPDisconnectOldSession(loc_conn2, 0, len, token))
    
    sleep(1);
    ret = FPGetSessionToken(loc_conn2, 4, time, strlen(id1), id1);
    sleep(1);

    FAIL (FPCloseFork(loc_conn2, fork))

    FAIL (FPLogOut(loc_conn2))
fin:
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, name))
test_exit:
	exit_test("test338");

}

/* ------------------------- */
STATIC void test339()
{
char *name = "t339 file";
char *ndir = "t339 dir";
char *no_user_uam = "No User Authent";

u_int16_t vol1, vol2;
unsigned int ret;
char *token;
u_int32_t len;
CONN *loc_conn1;
CONN *loc_conn2;
DSI *dsi;
DSI *loc_dsi1, *loc_dsi2;
int sock1, sock2;
int fork = 0, fork1;
struct sigaction action;
char *id0="testsuite-test339-0";
char *id1="testsuite-test338-1";
u_int32_t time= 12345;
u_int16_t vol = VolID;
int dir;
int  ofs =  3 * sizeof( u_int16_t );
u_int16_t bitmap =  (1 << DIRPBIT_ACCESS);
struct afp_filedir_parms filedir;


	enter_test();
    fprintf(stdout,"===================\n");
    fprintf(stdout,"FPDisconnectOldSession :test339: AFP 3.x No auth disconnect old session\n");

	if (Conn->afp_version < 30) {
    	test_skipped(T_AFP3);
		goto test_exit;
	}
    if (Locking) {
    	test_skipped(T_LOCKING);
		goto test_exit;
	}
    /* setup 2 new connections for testing */

    if ((loc_conn1 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        nottested();
		goto test_exit;
    }

    loc_conn1->type = 0;
    loc_dsi1 = &loc_conn1->dsi;
    sock1 = OpenClientSocket(Server, Port);
    if ( sock1 < 0) {
        nottested();
		goto test_exit;
    }

    loc_dsi1->protocol = DSI_TCPIP;
    loc_dsi1->socket = sock1;
    ret = FPopenLoginExt(loc_conn1, vers, no_user_uam, "", "");
    if (ret) {
        nottested();
		goto test_exit;
    }
    loc_conn1->afp_version = Conn->afp_version;

    ret = FPGetSessionToken(loc_conn1, 3, time, strlen(id0), id0);
    if (ret) {
        nottested();
		goto test_exit;
    }

    dsi = &Conn->dsi;
    memcpy(&len, loc_dsi1->data, sizeof(u_int32_t));
    len = ntohl(len);
    if (!len) {
        failed();
		goto test_exit;
    }
    if (!(token = malloc(len +4))) {
        fprintf(stdout, "\tNOT TESTED malloc(%x) %s\n", len, strerror(errno));
        nottested();
		goto test_exit;
    }
    memcpy(token, loc_dsi1->data + sizeof(u_int32_t), len);

    if (0xffff == (vol1 = FPOpenVol(loc_conn1, Vol))) {
        nottested();
		goto test_exit;
    }
    
    if (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , ndir))) {
		nottested();
		goto test_exit;
	}
	if (FPGetFileDirParams(Conn, vol,  dir , "", 0,bitmap )) {
		nottested();
		goto fin;
	}
	filedir.isdir = 1;
	afp_filedir_unpack(&filedir, dsi->data +ofs, 0, bitmap);
    filedir.access[0] = 0; 
    filedir.access[1] = 7; 
    filedir.access[2] = 7; 
    filedir.access[3] = 7; 
 	if (FPSetDirParms(Conn, vol, dir , "", bitmap, &filedir)) {
		nottested();
		goto fin;
	}
    FAIL(FPCreateFile(loc_conn1, vol1,  0, dir , name));

    /* --------------------------------- */
    if ((loc_conn2 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        nottested();
		goto test_exit;
    }

    loc_conn2->type = 0;
    loc_dsi2 = &loc_conn2->dsi;
    sock2 = OpenClientSocket(Server, Port);
    if ( sock2 < 0) {
        nottested();
        goto fin;
    }

    loc_dsi2->protocol = DSI_TCPIP;
    loc_dsi2->socket = sock2;
    ret = FPopenLoginExt(loc_conn2, vers, no_user_uam, "", "");
    if (ret) {
        nottested();
		goto test_exit;
    }
    loc_conn2->afp_version = Conn->afp_version;

    action.sa_handler = pipe_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if ((sigaction(SIGPIPE, &action, NULL) < 0)) {
		nottested();
		goto fin;
    }

    ret = FPDisconnectOldSession(loc_conn2, 0, len, token);
    if (ret == AFP_OK) {
        sleep(1);
        FAIL( FPGetSessionToken(loc_conn2, 4, time, strlen(id1), id1) );
    } else {
        failed();
        FAIL (FPLogOut(loc_conn1))
    }

    FAIL( FPLogOut(loc_conn2) );
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if ((sigaction(SIGPIPE, &action, NULL) < 0)) {
		nottested();
    }
fin:
    FAIL (FPDelete(Conn, vol,  dir, name))
    FAIL (FPDelete(Conn, vol,  dir, ""))
test_exit:
	exit_test("test339");
}

/* ------------------------- */
STATIC void test370()
{
char *name = "t370 file";
char *ndir = "t370 dir";
char *no_user_uam = "No User Authent";
u_int16_t vol1;
unsigned int ret;
char *token;
u_int32_t len;
CONN *loc_conn1;
CONN *loc_conn2;
DSI *dsi;
DSI *loc_dsi1, *loc_dsi2;
int sock1, sock2;
int fork = 0;
struct sigaction action;
char *id0="testsuite-test370-0";
char *id1="testsuite-test370-1";
u_int32_t time= 12345;
u_int16_t vol = VolID;
int dir;
int  ofs =  3 * sizeof( u_int16_t );
u_int16_t bitmap =  (1 << DIRPBIT_ACCESS);
struct afp_filedir_parms filedir;


	enter_test();
    fprintf(stdout,"===================\n");
    fprintf(stdout,"FPDisconnectOldSession :test370: AFP 3.x disconnect different user\n");

	if (Conn->afp_version < 30) {
    	test_skipped(T_AFP3);
		goto test_exit;
	}
    if (Locking) {
    	test_skipped(T_LOCKING);
		goto test_exit;
	}

	fprintf(stdout,"\tSKIPPED, FIXME need to recheck GetSessionToken 0\n");
	skipped_nomsg();
	goto test_exit;
    /* setup 2 new connections for testing */

    if ((loc_conn1 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        nottested();
		goto test_exit;
    }

    loc_conn1->type = 0;
    loc_dsi1 = &loc_conn1->dsi;
    sock1 = OpenClientSocket(Server, Port);
    if ( sock1 < 0) {
        nottested();
		goto test_exit;
    }

    loc_dsi1->protocol = DSI_TCPIP;
    loc_dsi1->socket = sock1;
    ret = FPopenLoginExt(loc_conn1, vers, uam, User, Password);
    if (ret) {
        nottested();
		goto test_exit;
    }
    loc_conn1->afp_version = Conn->afp_version;

    ret = FPGetSessionToken(loc_conn1, 3, time, strlen(id0), id0);
    if (ret) {
        nottested();
		goto test_exit;
    }

    dsi = &Conn->dsi;
    memcpy(&len, loc_dsi1->data, sizeof(u_int32_t));
    len = ntohl(len);
    if (!len) {
        failed();
		goto test_exit;
    }
    if (!(token = malloc(len +4))) {
        fprintf(stdout, "\tNOT TESTED malloc(%x) %s\n", len, strerror(errno));
        nottested();
		goto test_exit;
    }
    memcpy(token, loc_dsi1->data + sizeof(u_int32_t), len);

    if (0xffff == (vol1 = FPOpenVol(loc_conn1, Vol))) {
        nottested();
		goto test_exit;
    }
    
    if (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , ndir))) {
		nottested();
		goto test_exit;
	}
	if (FPGetFileDirParams(Conn, vol,  dir , "", 0,bitmap )) {
		nottested();
		goto fin;
	}
	filedir.isdir = 1;
	afp_filedir_unpack(&filedir, dsi->data +ofs, 0, bitmap);
    filedir.access[0] = 0; 
    filedir.access[1] = 7; 
    filedir.access[2] = 7; 
    filedir.access[3] = 7; 
 	if (FPSetDirParms(Conn, vol, dir , "", bitmap, &filedir)) {
		nottested();
		goto fin;
	}
    FAIL(FPCreateFile(loc_conn1, vol1,  0, dir , name));

    /* --------------------------------- */
    if ((loc_conn2 = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        nottested();
		goto test_exit;
    }

    loc_conn2->type = 0;
    loc_dsi2 = &loc_conn2->dsi;
    sock2 = OpenClientSocket(Server, Port);
    if ( sock2 < 0) {
        nottested();
        goto fin;
    }

    loc_dsi2->protocol = DSI_TCPIP;
    loc_dsi2->socket = sock2;
    ret = FPopenLoginExt(loc_conn2, vers, no_user_uam, "", "");
    if (ret) {
        nottested();
		goto test_exit;
    }
    loc_conn2->afp_version = Conn->afp_version;

    action.sa_handler = pipe_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if ((sigaction(SIGPIPE, &action, NULL) < 0)) {
		nottested();
		goto fin;
    }
    FAIL(ntohl(AFPERR_SESSCLOS) != FPDisconnectOldSession(loc_conn2, 0, len, token))
    sleep(1);
    ret = FPGetSessionToken(loc_conn2, 4, time, strlen(id1), id1);
    sleep(1);

	fork = FPOpenFork(loc_conn1, vol1, OPENFORK_RSCS , 0 , dir, name, OPENACC_WR |OPENACC_RD |OPENACC_DWR| OPENACC_DRD);
    if ( fork ) {
		FAIL (FPCloseFork(loc_conn1, fork))
    	failed();
    }
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if ((sigaction(SIGPIPE, &action, NULL) < 0)) {
		nottested();
    }
fin:
    FAIL (FPDelete(Conn, vol,  dir, name))
    FAIL (FPDelete(Conn, vol,  dir, ""))
test_exit:
	exit_test("test370");

}

/* ----------- */
void FPDisconnectOldSession_test()
{
    fprintf(stdout,"===================\n");
    fprintf(stdout,"FPDisconnectOldSession page 148\n");
    test222();
    test338();
    test339();
    test370();
}

