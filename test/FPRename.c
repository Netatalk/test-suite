/* ----------------------------------------------
*/
#include "specs.h"

/* ------------------------- */
STATIC void test69()
{
int  dir;
char *name = "t69 rename file!name";
char *name2 = "t69 new name";
u_int16_t vol = VolID;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPRename:test69: rename a folder with Unix name != Mac name\n");

	if (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , name))) {
		nottested();
		return;
	}
	FAIL (FPRename(Conn, vol, DIRDID_ROOT, name, name2)) 

	FAIL (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , name))) 

	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name2))

	FAIL (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , name2)))

	FAIL (htonl(AFPERR_EXIST) != FPRename(Conn, vol, DIRDID_ROOT, name, name2)) 

	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name2))
	FAIL (FPRename(Conn, vol, DIRDID_ROOT, name, name2))
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name2))

    /* other sens */
	FAIL (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , name2))) 
	FAIL (FPRename(Conn, vol, DIRDID_ROOT, name2, name)) 
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name)) 
}

/* ------------------------- */
STATIC void test72()
{
int dir = 0;
int dir1 = 0;
u_int16_t bitmap = 0;
char *name  = "t72 check rename input";
char *ndel = "t72 no delete dir";
char *name1 = "t72 new folder";
char *name2 = "t72 dir";
int  ret;
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t vol = VolID;
DSI *dsi;

	dsi = &Conn->dsi;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPRename:test72: check input parameter\n");

	if (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , name2))) {
		nottested();
		return;
	}

	if (!(dir1 = FPCreateDir(Conn,vol, DIRDID_ROOT , ndel))) {
		nottested();
		goto fin;
	}

	FAIL (ntohl(AFPERR_NORENAME) != FPRename(Conn, vol, DIRDID_ROOT, "", "volume")) 

	bitmap = (1<<DIRPBIT_ATTR);
	if (FPGetFileDirParams(Conn, vol,  dir1 , "", 0,bitmap )) {
		failed();
	}
	else {
		filedir.isdir = 1;
		afp_filedir_unpack(&filedir, dsi->data +ofs, 0, bitmap);
		filedir.attr = ATTRBIT_NORENAME | ATTRBIT_SETCLR ;
 		FAIL (FPSetDirParms(Conn, vol, dir1 , "", bitmap, &filedir)) 
		ret = FPRename(Conn, vol, DIRDID_ROOT, ndel, "volume");
		if (ntohl(AFPERR_OLOCK) != ret) {
			failed();
			if (!ret) {
				FPRename(Conn, vol, DIRDID_ROOT, "volume", ndel);
			}
		}
		filedir.attr = ATTRBIT_NODELETE;
 		FAIL (FPSetDirParms(Conn, vol, dir1 , "", bitmap, &filedir)) 
	}
	
	ret = FPRename(Conn, vol, DIRDID_ROOT, name2, name2);
	if (not_valid(ret, /* MAC */0, AFPERR_EXIST)) {
		failed();
	}

	FAIL (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name))

	FAIL (FPRename(Conn, vol, DIRDID_ROOT, name, name)) 

	FAIL (FPRename(Conn, vol, DIRDID_ROOT, name, name1)) 
	FAIL (ntohl(AFPERR_NOOBJ) != FPDelete(Conn, vol,  DIRDID_ROOT, name)) 
fin:
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, ndel)) 
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, name1)) 
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, name2))
}

/* -------------------------- */
static int create_double_deleted_folder(u_int16_t vol, char *name)
{
u_int16_t vol2;
int tdir;
int tdir1 = 0;
DSI *dsi2;
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
DSI *dsi = &Conn->dsi; 
u_int16_t bitmap;
int tp,tp1;

	tdir  = FPCreateDir(Conn,vol, DIRDID_ROOT, name);
	if (!tdir) {
		nottested();
		return 0;
	}

	tdir1  = FPCreateDir(Conn,vol,tdir, name);
	if (!tdir1) {
		nottested();
		goto fin;
	}

	if (FPGetFileDirParams(Conn, vol,  tdir1 , "", 0, (1 << DIRPBIT_OFFCNT))) {
		nottested();
		goto fin;
	}

	bitmap = (1 << DIRPBIT_ACCESS);
	if (FPGetFileDirParams(Conn, vol,  tdir , "", 0, bitmap)) {
		nottested();
		goto fin;
	}
	filedir.isdir = 1;
	afp_filedir_unpack(&filedir, dsi->data +ofs, 0, bitmap);
    filedir.access[0] = 0; 
    filedir.access[1] = 7; 
    filedir.access[2] = 7; 
    filedir.access[3] = 7; 
 	if ( FPSetDirParms(Conn, vol, tdir , "", bitmap, &filedir)) {
		nottested();
		goto fin;
 	}

	dsi2 = &Conn2->dsi;
	vol2  = FPOpenVol(Conn2, Vol);
	if (vol2 == 0xffff) {
		nottested();
		goto fin;
	}
	bitmap = (1 << DIRPBIT_ACCESS)| (1<< DIRPBIT_PDID) | (1<< DIRPBIT_DID);
	if (FPGetFileDirParams(Conn2, vol2,  DIRDID_ROOT , name, 0, bitmap)) {
		nottested();
		goto fin;
	}
	tp = get_did(Conn2, vol2, DIRDID_ROOT, name);
	if (!tp) {
		nottested();
		goto fin;
	}
	if (tp != tdir) {
		fprintf(stderr,"Warning DID connection1 0x%x ==> connection2 0x%x\n", tdir, tp);
	}
	tp1 = get_did(Conn2, vol2, tp, name);
	if (!tp1) {
		nottested();
		goto fin;
	}
	if (tp1 != tdir1) {
		fprintf(stderr,"Warning DID connection1 0x%x ==> connection2 0x%x\n", tdir1, tp1);
	}
	if (FPDelete(Conn2, vol2,  tp1 , "")) { 
		nottested();
		FPDelete(Conn, vol, tdir1 , "");
		tdir1 = 0;
	}
	if (FPDelete(Conn2, vol2,  tp , "")) { 
		nottested();
		FPDelete(Conn, vol, tdir , "");
		tdir1 = 0;
	}	
	FPCloseVol(Conn2,vol2);
	return tdir1;
fin:
	FAIL (tdir && FPDelete(Conn, vol, tdir , ""))
	FAIL (tdir1 && FPDelete(Conn, vol, tdir1 , ""))
	return 0;
}

/* ----------- */
STATIC void test183()
{
char *tname = "test183";
char *name1 = "test183.new";
int tdir;
u_int16_t vol = VolID;

	if (!Conn2) 
		return;
	
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPRename:test183: did error two users in  folder did=<deleted> name=test183\n");

	/* ---- directory.c ---- */
	if (!(tdir = create_double_deleted_folder(vol, tname))) {
		return;
	}

	FAIL (ntohl(AFPERR_NOOBJ) != FPDelete(Conn, vol, tdir , ""))

 	/* ---- filedir.c ------------ */
	if (!(tdir = create_double_deleted_folder(vol, tname))) {
		return;
	}
	FAIL (ntohl(AFPERR_NOOBJ) != FPRename(Conn, vol, tdir, "", name1))
}

/* ------------------------- */
STATIC void test184()
{
char *name  = "t184.txt";
char *name1 = "t184new.txt";
u_int16_t vol = VolID;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPRename:test184: rename\n");

	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		return;
	}
	
	FAIL (FPRename(Conn, vol, DIRDID_ROOT, name, name1))
	FAIL (FPDelete(Conn, vol, DIRDID_ROOT , name1)) 
	FAIL (!FPDelete(Conn, vol, DIRDID_ROOT , name)) 
	FPFlush(Conn, vol);
}

/* ------------------------- */
STATIC void test191()
{
char *name = "t191 dir";
char *name1= "t191 subdir";
char *dest = "t191 newname";
u_int16_t vol = VolID;
int  dir = 0,dir1 = 0,dir2 = 0;
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPRename:test191: rename folders\n");

	dir  = FPCreateDir(Conn,vol, DIRDID_ROOT , name);
	if (!dir) {
		nottested();
		return;
	}
	dir1  = FPCreateDir(Conn,vol, DIRDID_ROOT , name1);
	if (!dir1) {
		nottested();
		goto fin;
	}

	dir2  = FPCreateDir(Conn,vol, dir1 , name1);
	if (!dir2) {
		nottested();
		goto fin;
	}

	if (FPRename(Conn, vol, DIRDID_ROOT, name, dest)) {
		failed();
	}

fin:
	FAIL (dir2 && FPDelete(Conn, vol,  dir2, "")) 
	FAIL (dir1 && FPDelete(Conn, vol,  dir1, "")) 
	FAIL (dir && FPDelete(Conn, vol,  dir, "")) 
}

/* ----------- */
void FPRename_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPRename page 250\n");
	test69();
	test72();
	test183();
	test184();
	test191();
}

