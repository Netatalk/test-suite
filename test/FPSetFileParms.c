/* ----------------------------------------------
*/
#include "specs.h"

STATIC void test83()
{
char *name = "t83 test file setfilparam";
char *name1 = "t83 test enoent file";
char *ndir = "t83 dir";
int  dir;
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<FILPBIT_ATTR) | (1<<FILPBIT_FINFO)| (1<<FILPBIT_CDATE) | 
					(1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE);
u_int16_t vol = VolID;
DSI *dsi;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"t83: test set file setfilparam\n");

	if (!(dir =FPCreateDir(Conn,vol, DIRDID_ROOT , ndir))) {
		nottested();
		goto test_exit;
	}

	
	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		goto fin;
	}

	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name, bitmap,0)) {
		failed();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
 		FAIL (FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir)) 
 		FAIL (htonl(AFPERR_NOOBJ) != FPSetFileParams(Conn, vol, DIRDID_ROOT , name1, bitmap, &filedir)) 
		FAIL (htonl(AFPERR_BADTYPE) != FPSetFileParams(Conn, vol, DIRDID_ROOT , ndir, bitmap, &filedir))
	}
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name)) 
fin:
	FAIL (FPDelete(Conn, vol,  dir , "")) 
test_exit:
	exit_test("test83");
}

/* ------------------------- */
STATIC void test96()
{
char *name = "t96 invisible file";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_MDATE);
u_int16_t vol = VolID;
DSI *dsi;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t96: test file's invisible bit\n");

	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		goto test_exit;
	}

	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , "", 0,bitmap )) {
		failed();
		goto end;
	}
	filedir.isdir = 1;
	afp_filedir_unpack(&filedir, dsi->data +ofs, 0, bitmap);
	fprintf(stderr,"Modif date parent %x\n", filedir.mdate);
	sleep(4);
	
	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name,bitmap, 0 )) {
		failed();
		goto end;
	}
	filedir.isdir = 0;
	afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
	fprintf(stderr,"Modif date file %x\n", filedir.mdate);
	sleep(5);
			
	filedir.attr = ATTRBIT_INVISIBLE | ATTRBIT_SETCLR ;
	bitmap = (1<<DIRPBIT_ATTR);
 	FAIL (FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir)) 

	bitmap = (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_MDATE);
	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name, bitmap,0 )) {
		failed();
		goto end;
	}
	filedir.isdir = 0;
	afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
	fprintf(stderr,"Modif date file %x\n", filedir.mdate);

	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , "", 0,bitmap )) {
		failed();
		goto end;
	}
	filedir.isdir = 1;
	afp_filedir_unpack(&filedir, dsi->data +ofs, 0, bitmap);
	fprintf(stderr,"Modif date parent %x\n", filedir.mdate);
end:
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
test_exit:
	exit_test("test96");
}

/* ------------------------- */
STATIC void test118()
{
char *name = "t118 no delete file";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<FILPBIT_ATTR);
u_int16_t vol = VolID;
DSI *dsi;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t118: test file no delete attribute\n");

	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		goto test_exit;
	}
	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name, bitmap,0 )) {
		nottested();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
		filedir.attr = ATTRBIT_NODELETE | ATTRBIT_SETCLR ;
 		FAIL (FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir)) 
		FAIL (ntohl(AFPERR_OLOCK) != FPDelete(Conn, vol,  DIRDID_ROOT , name)) 
		filedir.attr = ATTRBIT_NODELETE;
 		FAIL (FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir)) 
	}
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name)) 
test_exit:
	exit_test("test118");
}

/* ------------------------- */
STATIC void test122()
{
char *name = "t122 setfilparam open fork";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
int fork;
int fork1;
int ret;
int type = OPENFORK_RSCS;

u_int16_t bitmap =  (1<<FILPBIT_ATTR) | (1<<FILPBIT_FINFO)| (1<<FILPBIT_CDATE) | 
					(1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE);
u_int16_t vol = VolID;
DSI *dsi;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"t122: test setfilparam open fork\n");

	memset(&filedir, 0, sizeof(filedir));
	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		goto test_exit;
	}
	fork = FPOpenFork(Conn, vol, type , 0 ,DIRDID_ROOT, name, OPENACC_WR |OPENACC_RD| OPENACC_DWR| OPENACC_DRD);
	if (!fork) {
		nottested();
		goto fin;
	}

	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name, bitmap,0)) {
		failed();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
		/* wrong attrib (open fork set ) */
 		ret = FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir);
		if (not_valid(ret, /* MAC */AFPERR_PARAM, 0)) {
			failed();
		}
		bitmap =  (1<<FILPBIT_FINFO)| (1<<FILPBIT_CDATE) | (1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE);
 		FAIL (FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir))

 		FAIL (htonl(AFPERR_BITMAP) != FPSetFileParams(Conn, vol, DIRDID_ROOT , name, 0xffff, &filedir))
	}
	fork1 = FPOpenFork(Conn, vol, type , 0 ,DIRDID_ROOT, name, OPENACC_RD);
	if (fork1) {
		FAIL (FPCloseFork(Conn,fork1))
		failed();
	}

	FPCloseFork(Conn,fork);
fin:
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
test_exit:
	exit_test("test122");
}

/* ------------------------- */
STATIC void test318()
{
char *name = "t318 PDinfo error";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<FILPBIT_PDINFO );
u_int16_t vol = VolID;
DSI *dsi;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t318: set UTF8 name(error)\n");

 	if (Conn->afp_version < 30) { 
		test_skipped(T_AFP3);
		goto test_exit;
 	}
 	
	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		goto test_exit;
	}
	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name, bitmap,0 )) {
		nottested();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
		FAIL (htonl(AFPERR_BITMAP) != FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir)) 
	}
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name)) 
test_exit:
	exit_test("test318");
}

/* ------------------------ */
static int afp_symlink(char *oldpath, char *newpath)
{
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap;
u_int16_t vol = VolID;
DSI *dsi;
int fork = 0;

	dsi = &Conn->dsi;

    if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , newpath)) {
	    return -1;
	}

	fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0 ,DIRDID_ROOT, newpath , OPENACC_WR | OPENACC_RD);
	if (!fork) {
	    return -1;
	}

	if (FPWrite(Conn, fork, 0, strlen(oldpath), oldpath, 0 )) {
	    return -1;
	}

    if (FPCloseFork(Conn,fork)) {
	    return -1;
    }
    fork = 0;

	bitmap = (1<< DIRPBIT_ATTR) |  (1<<FILPBIT_FINFO) |
	         (1<<DIRPBIT_CDATE) |  (1<<DIRPBIT_MDATE) |
		     (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID) | (1<<FILPBIT_FNUM );
    
	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , newpath, bitmap,0 )) {
	    return -1;
    }

    filedir.isdir = 0;
    afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
    memcpy(filedir.finder_info, "slnkrhap", 8);
	bitmap = (1<<FILPBIT_FINFO);
    
    if (FPSetFileParams(Conn, vol, DIRDID_ROOT , newpath, bitmap, &filedir))
        return -1;
    return 0;
}

/* ------------------------- */
STATIC void test426()
{
char *name = "t426 Symlink";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap;
u_int16_t vol = VolID;
DSI *dsi;
int fork = 0;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t426: Create a dangling symlink\n");
    
    if (afp_symlink("t426 dest", name)) {
		nottested();
		goto test_exit;
	}

	fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0 ,DIRDID_ROOT, name , OPENACC_WR | OPENACC_RD);
	if (!fork) {
		failed();
	}
	else {
	    FPCloseFork(Conn,fork);
    }

	fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0 ,DIRDID_ROOT, name , OPENACC_RD);
	if (!fork) {
	    /* Trying to open the linked file? */
		failed();
	}

test_exit:
    if (fork) {
	    FPCloseFork(Conn,fork);
    }
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
	exit_test("test426");
}

/* ------------------------- */
STATIC void test427()
{
char *name = "t427 Symlink";
char *dest = "t427 dest";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap;
u_int16_t vol = VolID;
DSI *dsi;
int fork = 0;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t427: Create a symlink\n");
    
    if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , dest)) {
		nottested();
		goto test_exit;
	}

    if (afp_symlink(dest, name)) {
		nottested();
		goto test_exit;
	}

	fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0 ,DIRDID_ROOT, name , OPENACC_RD);
	if (!fork) {
	    /* Trying to open the linked file? */
		failed();
	}

test_exit:
    if (fork) {
	    FPCloseFork(Conn,fork);
    }
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , dest))
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
	exit_test("test427");
}

/* ------------------------- */
STATIC void test428()
{
char *name = "t428 Symlink";
char *name2 = "t428 Symlink2";
char *dest = "t428 dest";
u_int16_t vol = VolID;
u_int16_t vol2 = 0xffff;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t428: Delete symlinks, two users\n");

	if (!Conn2) {
		test_skipped(T_CONN2);
		goto test_exit;
	}		

	vol2  = FPOpenVol(Conn2, Vol);
	if (vol2 == 0xffff) {
		nottested();
		goto test_exit;
	}
    
    if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , dest)) {
		nottested();
		goto test_error;
	}

    if (afp_symlink(dest, name)) {
		nottested();
		goto test_error;
	}
    if (afp_symlink(dest, name2)) {
		nottested();
		goto test_error;
	}
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
    FAIL (FPDelete(Conn2, vol2,  DIRDID_ROOT , name2))

test_error:
	if (vol2 != 0xffff)
	    FPCloseVol(Conn2,vol2);
    FPDelete(Conn, vol,  DIRDID_ROOT , dest);
    FPDelete(Conn, vol,  DIRDID_ROOT , name);
    FPDelete(Conn, vol,  DIRDID_ROOT , name2);

test_exit:
	exit_test("test428");
}

/* ------------------------- */
STATIC void test429()
{
char *name = "t429 Symlink";
char *dest = "t429 dest";
int  ofs =  sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<FILPBIT_FNUM );
u_int16_t vol = VolID;
DSI *dsi;
int fork = 0;
int id;

	dsi = &Conn->dsi;

	enter_test();
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t429: symlink File ID\n");
    
    if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , dest)) {
		nottested();
		goto test_exit;
	}

    if (afp_symlink(dest, name)) {
		nottested();
		goto test_exit;
	}

	id = get_fid(Conn, vol, DIRDID_ROOT , name);

	fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0 ,DIRDID_ROOT, name , OPENACC_RD);
	if (!fork) {
	    /* Trying to open the linked file? */
		failed();
		goto test_exit;
	}

	filedir.did = 0;
	if (FPGetForkParam(Conn, fork, bitmap)) {
		failed();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
		if (!filedir.did || filedir.did != id) {
		    fprintf(stderr,"\tFAILED cnids differ %x %x\n", filedir.did, id);
			failed_nomsg();
		}
	}

test_exit:
    if (fork) {
	    FPCloseFork(Conn,fork);
    }
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , dest))
    FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
	exit_test("test429");
}


/* ----------- */
void FPSetFileParms_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms page 262\n");
    test83();
    test96();
    test118();
    test122();
    test318();
    test426();
    test427();
    test428();
    test429();
}

