/* -------------------------------------
*/
#include "afpclient.h"
#include "test.h"

extern u_int16_t VolID;
extern DSI *dsi, *dsi2; 
extern int Mac;
extern int ExitCode;

#include <signal.h>  

#define FAIL(a) if (a) { failed();}
#define STATIC 

extern void illegal_fork(DSI * dsi, char cmd, char *name);
extern int no_access_folder(u_int16_t vol, int did, char *name);
extern int read_only_folder(u_int16_t vol, int did, char *name);
extern int delete_folder(u_int16_t vol, int did, char *name);

extern int get_did(CONN *conn, u_int16_t vol, int dir, char *name);
extern int get_fid(CONN *conn, u_int16_t vol, int dir, char *name);
extern u_int32_t get_forklen(DSI *dsi, int type);

extern void write_fork(CONN *conn, u_int16_t vol,int dir, char *name, char *data);
extern void read_fork(CONN *conn, u_int16_t vol,int dir, char *name,int len);

extern int read_only_folder_with_file(u_int16_t vol, int did, char *name, char *file);
extern int delete_folder_with_file(u_int16_t vol, int did, char *name, char *file);
extern int get_vol_attrib(u_int16_t vol) ;

extern void failed_nomsg(void);
extern void failed(void);
extern void nottested(void);
extern int not_valid(int ret, int mac_error, int afpd_error);

/* ---------------------------------
*/
