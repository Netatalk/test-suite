/* ----------------------------------------------
*/
#include "specs.h"
#include "adoublehelper.h"

/* ------------------------- */
STATIC void test89()
{
int  dir;
char *file = "t89 test error setfilparam";
char *name = "t89 error setfilparams dir";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<FILPBIT_FINFO)| (1<<FILPBIT_CDATE) | 
					(1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE);
u_int16_t vol = VolID;
DSI *dsi = &Conn->dsi;
unsigned ret;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:test89: test set file setfilparam\n");

	if (!Mac && !Path) {
		nottested();
		return;
	}
 	if (!(dir = folder_with_ro_adouble(vol, DIRDID_ROOT, name, file))) {
		nottested();
		return;
 	}

	if (FPGetFileDirParams(Conn, vol,  dir , file, bitmap,0)) {
		failed();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
		ret = FPSetFileParams(Conn, vol, dir , file, bitmap, &filedir);
		if (not_valid(ret, 0, AFPERR_ACCESS)) {
			failed();
		}
		if (!Mac && ret != htonl(AFPERR_ACCESS)) {
			failed();
		}
	}
	bitmap = (1<<FILPBIT_MDATE);
	if (FPGetFileDirParams(Conn, vol,  dir, file, bitmap,0)) {
		failed();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
 		FAIL (FPSetFileParams(Conn, vol, dir , file, bitmap, &filedir))
	}
	delete_ro_adouble(vol, dir, file);
}

/* ------------------------- */
STATIC void test120()
{
char *name = "t120 test file setfilparam";
int  ofs =  3 * sizeof( u_int16_t );
struct afp_filedir_parms filedir;
u_int16_t bitmap = (1<<FILPBIT_ATTR) | (1<<FILPBIT_FINFO)| (1<<FILPBIT_CDATE) | 
					(1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE);
u_int16_t vol = VolID;
DSI *dsi = &Conn->dsi;


    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms:t120: test set file setfilparam (create .AppleDouble)\n");

	if (!Mac && !Path) {
		nottested();
		return;
	}

	if (FPCreateFile(Conn, vol,  0, DIRDID_ROOT , name)) {
		nottested();
		return;
	}

	if (FPGetFileDirParams(Conn, vol,  DIRDID_ROOT , name, bitmap,0)) {
		failed();
	}
	else {
		filedir.isdir = 0;
		afp_filedir_unpack(&filedir, dsi->data +ofs, bitmap, 0);
		if (!Mac) {
			delete_unix_rf(Path,"", name);
		}
 		FAIL (FPSetFileParams(Conn, vol, DIRDID_ROOT , name, bitmap, &filedir))
	}

	FAIL (FPDelete(Conn, vol,  DIRDID_ROOT , name))
}

/* ----------- */
void FPSetFileParms_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPSetFileParms page 262\n");
    test89();
    test120();
}

