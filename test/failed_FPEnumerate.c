/* ----------------------------------------------
*/
#include "specs.h"

/* ------------------------- */
STATIC void test28()
{
u_int16_t vol = VolID;
char *name  = "t28 dir";
char *name1 = "t28 subdir";
char *name2 = "t28 file";
int  dir;
int  dir1;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPEnumerate:test28: test search by ID\n");

	/* we need to empty the server cashe */
	FPCloseVol(Conn, vol);
	
	vol = VolID = FPOpenVol(Conn, Vol);
	if (vol == 0xffff) {
		nottested();
		return;
	}

	dir   = FPCreateDir(Conn,vol, DIRDID_ROOT , name);
	if (!dir) {
		nottested();
		return;
	}

	dir1  = FPCreateDir(Conn,vol, dir , name1);
	if (!dir1) {
		nottested();
		goto fin;
	}

	FAIL (FPCreateFile(Conn, vol,  0, dir1 , name2))

	FAIL (FPCloseVol(Conn,vol))

	vol = VolID = FPOpenVol(Conn, Vol);
	if (vol == 0xffff) {
		nottested();
		return;
	}

	if (FPEnumerate(Conn, vol,  dir1 , "", 
	         (1<<FILPBIT_LNAME) | (1<<FILPBIT_FNUM ) | (1<<FILPBIT_ATTR) | (1<<FILPBIT_FINFO)|
	         (1<<FILPBIT_CDATE) | (1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE)
	         ,
		     (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_FINFO) |
	         (1<<DIRPBIT_CDATE) | (1<<DIRPBIT_BDATE) | (1<<DIRPBIT_MDATE) |
		    (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID) | (1<< DIRPBIT_DID)|(1<< DIRPBIT_ACCESS)
		)
	) {
#ifdef QUIRK	
		fprintf(stderr,"\tFAILED (IGNORED)\n");
#else
		failed();
#endif
		/* warm the cache */
		if (FPEnumerate(Conn, vol,  DIRDID_ROOT , "", 0,
		     (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_FINFO) |
	         (1<<DIRPBIT_CDATE) | (1<<DIRPBIT_BDATE) | (1<<DIRPBIT_MDATE) |
		    (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID) | (1<< DIRPBIT_DID)|(1<< DIRPBIT_ACCESS)
		)) {
			failed();
		}
		else if (FPEnumerate(Conn, vol,  dir, "", 0,
		     (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_FINFO) |
	         (1<<DIRPBIT_CDATE) | (1<<DIRPBIT_BDATE) | (1<<DIRPBIT_MDATE) |
		    (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID) | (1<< DIRPBIT_DID)|(1<< DIRPBIT_ACCESS)
		)) {
			failed();
		}
	}

	FAIL (dir1 && FPDelete(Conn, vol,  dir1 , name2))
fin:	
	FAIL (dir1 && FPDelete(Conn, vol,  dir1 , ""))
	FAIL (dir && FPDelete(Conn, vol,  dir, ""))
}



/* ----------- */
void FPEnumerate_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPEnumerate page 150\n");
    test28();
}

