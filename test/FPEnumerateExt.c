/* ----------------------------------------------
*/
#include "specs.h"

/* ------------------------- */
STATIC void test23()
{
char *name = "t23 dir";
char *name1 = "t23 subdir";
char *name2 = "t23 file";
int  dir,dir1;
u_int16_t vol = VolID;


    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPEnumerateExt:test23: AFP 3.0 FPEnumerate ext\n");

	dir  = FPCreateDir(Conn,vol, DIRDID_ROOT , name);
	if (!dir) {
		nottested();
		return;
	}

	if (Conn->afp_version < 30) {
		if (ntohl(AFPERR_NOOP) != FPEnumerate_ext(Conn, vol,  DIRDID_ROOT , "", 
		                    (1 << FILPBIT_PDINFO )|(1 << FILPBIT_EXTDFLEN) | (1 << FILPBIT_EXTRFLEN)
		                    |(1 << FILPBIT_DFLEN) |(1 << FILPBIT_RFLEN), 0xffff)) {
			failed();
		}
		FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, name))
		return;
	}	
	FAIL (FPGetSessionToken(Conn, 1))
	FAIL (FPGetSrvrInfo(Conn))
	dir1 = FPCreateDir(Conn,vol, dir , name1);
	if (dir1) {
		FAIL (FPGetFileDirParams(Conn, vol,  dir , name1, 0, (1 << DIRPBIT_PDINFO ) | (1 << DIRPBIT_OFFCNT)))
		FAIL (FPCreateFile(Conn, vol,  0, dir1 , name2))
		FAIL (FPGetFileDirParams(Conn, vol,  dir , name1, 0, (1 << DIRPBIT_PDINFO ) | (1 << DIRPBIT_OFFCNT)))
		if (FPEnumerate_ext(Conn, vol,  dir1 , "", 
			                (1 << FILPBIT_PDINFO )| (1 << FILPBIT_EXTDFLEN) | (1 << FILPBIT_EXTRFLEN)
		        	            |(1 << FILPBIT_DFLEN) |(1 << FILPBIT_RFLEN),
	                        	(1 << DIRPBIT_PDINFO ) | (1 << DIRPBIT_OFFCNT)
		                     )) {
			failed();
		}
		if (FPEnumerate_ext(Conn, vol,  dir , "", 
		                (1 << FILPBIT_PDINFO )| (1 << FILPBIT_EXTDFLEN) | (1 << FILPBIT_EXTRFLEN)
		                    |(1 << FILPBIT_DFLEN) |(1 << FILPBIT_RFLEN) | (1 << FILPBIT_LNAME),
	                        (1 << DIRPBIT_PDINFO ) | (1 << DIRPBIT_OFFCNT) | (1 << DIRPBIT_LNAME)
	                     )) {
			failed();
		}	                   
		FAIL (FPDelete(Conn, vol,  dir1, name2))
	}
	else {
		failed();
	}
	if (dir) {
		FAIL (FPDelete(Conn, vol,  dir, name1))
		FAIL (FPDelete(Conn, vol,  DIRDID_ROOT, name))
	}
}

/* ----------- */
void FPEnumerateExt_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPEnumerateExt page 155\n");
    test23();
}

