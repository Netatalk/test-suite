/* ----------------------------------------------
*/
#include "specs.h"
extern char *Vol2;
extern int Manuel;

/* ----------------------- */
STATIC void test320(void)
{
int ret;
DSI *dsi = &Conn->dsi;
int nbe,i;
int found = 0;
unsigned char len;
u_int16_t vol2;
char *b;

    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPGetSrvrParms:test320: GetSrvrParms after volume config file has been modified\n");

	if (!Mac && !Path) {
		test_skipped(T_MAC_PATH);
		return;
	}
	if (!Manuel || *Vol2 == 0) {
		test_skipped(T_VOL2);
		return;
	}

	vol2 = FPOpenVol(Conn, Vol2);
	if (vol2 == 0xffff) {
		nottested();
		return;
	}
	printf("Modify AppleVolumes.default and press enter\n");
	getchar();
	

	FAIL (FPCloseVol(Conn,vol2))

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

	vol2 = FPOpenVol(Conn, Vol2);
	if (vol2 == 0xffff) {
		nottested();
		return;
	}
	FAIL (FPCloseVol(Conn,vol2))
}

/* ----------- */
void FPGetSrvrParms_test()
{
    fprintf(stderr,"===================\n");
    fprintf(stderr,"FPGetSrvrParms page 203\n");
	test320();
}

