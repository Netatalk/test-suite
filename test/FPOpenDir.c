/* ----------------------------------------------
*/
#include "specs.h"

/* -------------------------- */
STATIC void test57()
{
int dir;
int pdir;
int rdir = 0;
char *name = "t57 dir no access";
char *name2 = "t57 ro dir";
char *name4 = "t57 dir";
char *name3 ="t57 file.txt";
u_int16_t vol = VolID;
int ret;
DSI *dsi;

	if (!Conn2) 
		return;
		
    fprintf(stderr,"===================\n");
	fprintf(stderr, "FPOpenDir:test57: OpenDir call\n");

	if (!(pdir = no_access_folder(vol, DIRDID_ROOT, name))) {
		return;
	}
	if (!(rdir = read_only_folder(vol, DIRDID_ROOT, name2) ) ) {
		goto fin;
	}
	if (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , name4))) {
		failed();
		goto fin;
	}
	dsi = &Conn->dsi;

	FAIL (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name3))
	dir = FPOpenDir(Conn,vol, DIRDID_ROOT , name);
	ret = dsi->header.dsi_code;
	if (not_valid(ret, /* MAC */0, AFPERR_ACCESS)) {
		failed();
	}
	dir = FPOpenDir(Conn,vol, DIRDID_ROOT , name2);
	if (!dir) {
		failed();
	}
	dir = FPOpenDir(Conn,vol, DIRDID_ROOT , name3);
    if (dir || ntohl(AFPERR_BADTYPE) != dsi->header.dsi_code) {
		failed();
	}

	dir = FPOpenDir(Conn,vol, DIRDID_ROOT_PARENT , "");
    if (dir || ntohl(AFPERR_PARAM) != dsi->header.dsi_code) {
		failed();
	}

	dir = FPOpenDir(Conn,vol, DIRDID_ROOT_PARENT , Vol);
    if (dir != DIRDID_ROOT) {
		failed();
	}

	dir = FPOpenDir(Conn,vol +1, DIRDID_ROOT_PARENT , "");
    if (dir || ntohl(AFPERR_PARAM) != dsi->header.dsi_code) {
		failed();
	}

	dir = FPOpenDir(Conn,vol, DIRDID_ROOT , name4);
    if (!dir) {
		failed();
		goto fin;
	}
	FAIL (FPCloseDir(Conn, vol, dir))
fin:
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name4))
	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name3)) 
	delete_folder(vol, DIRDID_ROOT, name);
	if (rdir) {
		delete_folder(vol, DIRDID_ROOT, name2);
	}
}

/* ----------- */
void FPOpenDir_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPOpenDir page 227\n");
	test57();
}

