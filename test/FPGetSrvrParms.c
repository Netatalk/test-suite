/* ----------------------------------------------
*/
#include "specs.h"

/* ----------------------- */
STATIC void test209(void)
{
int ret;
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPGetSrvrParms:test209: GetSrvrParms call\n");

	ret = FPGetSrvrParms(Conn);
	if (ret) {
		failed();
	}
	
}

/* ----------------------- */
STATIC void test316(void)
{
int ret;
DSI *dsi = &Conn->dsi;
int nbe,i;
int found = 0;
unsigned char len;
char *b;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPGetSrvrParms:test316: GetSrvrParms for a volume with option nostat set\n");

	FPCloseVol(Conn,VolID);

	ret = FPGetSrvrParms(Conn);
	if (ret) {
		failed();
	}
	nbe = *(dsi->data +4);
	b = dsi->data+5;
	for (i = 0; i < nbe; i++) {
	    b++; /* flags */
	    len = *b;
	    b++;
	    if (!strncmp(b, Vol, len))
	        found = 1;
	    b += len;
	}
	if (!found) {
		failed();
	}

	VolID = FPOpenVol(Conn, Vol);
	if (VolID == 0xffff) {
		nottested();
		return;
	}
}

/* ----------- */
void FPGetSrvrParms_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPGetSrvrParms page 203\n");
	test209();
	test316();
}

