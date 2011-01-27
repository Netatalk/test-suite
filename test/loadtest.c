#include <sys/time.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "afpclient.h"
#include "test.h"
#include "specs.h"

int Verbose = 0;
int Quirk = 0;

static u_int16_t vol;
static DSI *dsi;
CONN *Conn;

int ExitCode = 0;

/* =============================== */

DSI *Dsi;

char Data[300000] = "";
/* ------------------------------- */
char    *Server = NULL;
int     Proto = 0;
int     Port = 548;
char    *Password = "";
char    *Vol = "";
char    *User;
char    *Path;
int     Version = 21;
int     Mac = 0;
int     Iterations = 1;
int     Iterations_save;

extern  int     Throttle;

struct timeval tv_start;
struct timeval tv_end;
struct timeval tv_dif;

#define READ_WRITE_SIZE 100

#define TEST_OPENSTATREAD    0
#define TEST_WRITE100MB      1
#define TEST_READ100MB       2
#define TEST_LOCKUNLOCK      3
#define TEST_CREATE2000FILES 4
#define TEST_ENUM2000FILES   5
#define TEST_DIRTREE         6
#define LASTTEST TEST_DIRTREE
#define NUMTESTS (LASTTEST+1)

char *resultstrings[] = {
    "Opening, stating and reading 512 bytes from 1000 files ",
    "Writing 100 MB to one file                             ",
    "Reading 100 MB from one file                           ",
    "Locking/Unlocking 10000 times each                     ",
    "Creating dir with 2000 files                           ",
    "Enumerate dir with 2000 files                          ",
    "Create directory tree with 10^3 dirs                   "
};

char teststorun[NUMTESTS];
unsigned long (*results)[][NUMTESTS];

static void starttimer(void)
{
    gettimeofday(&tv_start, NULL);
}

static void stoptimer(void)
{
    gettimeofday(&tv_end, NULL);
}

static unsigned long timediff(void)
{
    if (tv_end.tv_usec < tv_start.tv_usec) {
        tv_end.tv_usec += 1000000;
        tv_end.tv_sec -= 1;
    }
    tv_dif.tv_sec = tv_end.tv_sec - tv_start.tv_sec;
    tv_dif.tv_usec = tv_end.tv_usec - tv_start.tv_usec;

    return (tv_dif.tv_sec * 1000) + (tv_dif.tv_usec / 1000);
}

static void addresult(int test, int iteration)
{
    unsigned long t;
    unsigned long long avg;

    t = timediff();
    printf("Run %u => %s%6lu ms", iteration, resultstrings[test], t);
    if ((test == TEST_WRITE100MB) || (test == TEST_READ100MB)) {
        avg = (READ_WRITE_SIZE * 1000) / t;
        printf(" (avg. %llu MB/s)", avg);
    }
    printf("\n");
    (*results)[iteration][test] = t;
}

static void displayresults(void)
{
    int i, test, maxindex, minindex, divsub = 0;
    unsigned long long sum, max = 0, min = 18446744073709551615ULL;

    /* Eleminate runaways */
    if (Iterations_save > 5) {
        divsub = 2;
        for (test=0; test != NUMTESTS; test++) {
            if (! teststorun[test])
                continue;
            for (i=0, sum=0; i < Iterations_save; i++) {
                if ((*results)[i][test] < min) {
                    min = (*results)[i][test];
                    minindex = i;
                }
                if ((*results)[i][test] > max) {
                    max = (*results)[i][test];
                    maxindex = i;
                }
            }
            (*results)[minindex][test] = 0;
            (*results)[maxindex][test] = 0;
        }
    }

    printf("\nNetatalk Lantest Results (averages)\n");
    printf("===================================\n\n");

    unsigned long long avg, thrput;

    for (test=0; test != NUMTESTS; test++) {
        if (! teststorun[test])
            continue;
        for (i=0, sum=0; i < Iterations_save; i++)
            sum += (*results)[i][test];
        avg = sum / (Iterations_save - divsub);
        printf("%s%6llu ms", resultstrings[test], avg);
        if ((test == TEST_WRITE100MB) || (test == TEST_READ100MB)) {
            thrput = (READ_WRITE_SIZE * 1000) / avg;
            printf(" (avg. %llu MB/s)", thrput);
        }

        printf("\n");
    }

}

/* ------------------------- */
void failed(void)
{
    fprintf(stderr,"\tFAILED\n");
    if (!ExitCode)
        ExitCode = 1;
}

/* ------------------------- */
void fatal_failed(void)
{
    fprintf(stderr,"\tFAILED\n");
    exit(1);
}
/* ------------------------- */
void nottested(void)
{
    fprintf(stderr,"\tNOT TESTED\n");
    if (!ExitCode)
        ExitCode = 2;
}

/* --------------------------------- */
int is_there(CONN *conn, int did, char *name)
{
    return FPGetFileDirParams(conn, vol,  did, name,
                              (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID)
                              ,
                              (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID)
        );
}

/* ------------------------- */
void test143()
{
    int id;
    char *ndir = NULL;
    int dir;
    int i,maxi = 0;
    int j, k;
    int fork;
    int test = 0;
    char *data = NULL;
    int nowrite;
    static char temp[MAXPATHLEN];

    /* Configure the tests */
    int smallfiles = 1000;                     /* 1000 files */
    int numread = (100*1024*1024) / (64*1024); /* 100 MB in blocks of 64k */
    int locking = 10000 / 40;                  /* 10000 times */
    int create_enum_files = 2000;              /* 2000 files */
#define DIRNUM 10                              /* 10^3 nested dirs. This is a define because we
                                                  currently create static arrays based on it*/

    id = getpid();
    if (!ndir) {
        sprintf(temp,"LanTest-%d", id);
        ndir = strdup(temp);
    }
    if (!data)
        data = calloc(1, 65536);
    
    if (FPGetFileDirParams(Conn, vol, DIRDID_ROOT, "", 0, (1<< DIRPBIT_DID)))
        fatal_failed();

    if (is_there(Conn, DIRDID_ROOT, ndir) == AFP_OK)
        fatal_failed();

    if (!(dir = FPCreateDir(Conn,vol, DIRDID_ROOT , ndir)))
        fatal_failed();
    if (FPGetFileDirParams(Conn, vol, dir, "", 0 , (1<< DIRPBIT_DID)))
        fatal_failed();

    if (is_there(Conn, DIRDID_ROOT, ndir) != AFP_OK)
        fatal_failed();

    /* --------------- */
    /* Test (1)        */
    if (teststorun[TEST_OPENSTATREAD]) {
        for (i=0; i <= smallfiles; i++) {
            sprintf(temp, "File.small%d", i);
            if (ntohl(AFPERR_NOOBJ) != is_there(Conn, dir, temp)) {
                fatal_failed();
            }
            else {
                if (FPGetFileDirParams(Conn, vol,  dir, "", 0, (1<< DIRPBIT_DID) )) {
                    fatal_failed();
                }
                if (FPCreateFile(Conn, vol,  0, dir , temp)){
                    fatal_failed();
                }
                if (is_there(Conn, dir, temp)) {
                    fatal_failed();
                }
                if (FPGetFileDirParams(Conn, vol,  dir, temp,
                                       (1<<FILPBIT_FNUM )|(1<<FILPBIT_PDID)|(1<<FILPBIT_FINFO)|
                                       (1<<FILPBIT_CDATE)|(1<<FILPBIT_MDATE)|(1<<FILPBIT_DFLEN)|(1<<FILPBIT_RFLEN)
                                       , 0)) {
                    fatal_failed();
                }
                if (FPGetFileDirParams(Conn, vol,  dir, temp,
                                       (1<<FILPBIT_FNUM )|(1<<FILPBIT_PDID)|(1<<FILPBIT_FINFO)|
                                       (1<< DIRPBIT_ATTR)|(1<<DIRPBIT_BDATE)|
                                       (1<<FILPBIT_CDATE)|(1<<FILPBIT_MDATE)|(1<<FILPBIT_DFLEN)|(1<<FILPBIT_RFLEN)
                                       , 0)) {
                    fatal_failed();
                }
                fork = FPOpenFork(Conn, vol, OPENFORK_DATA ,
                                  (1<<FILPBIT_PDID)|(1<< DIRPBIT_LNAME)|(1<<FILPBIT_FNUM)|(1<<FILPBIT_DFLEN)
                                  , dir, temp, OPENACC_WR |OPENACC_RD| OPENACC_DWR| OPENACC_DRD);
                if (!fork) {
                    fatal_failed();
                }
                else {
                    if (FPGetForkParam(Conn, fork, (1<<FILPBIT_PDID)|(1<< DIRPBIT_LNAME)|(1<<FILPBIT_DFLEN))
                        ) {
                        fatal_failed();
                    }
                    if (FPWrite(Conn, fork, 0, 20480, data, 0 )) {
                        fatal_failed();
                    }
                    if (FPCloseFork(Conn,fork)) {fatal_failed();}
                }
            }
            maxi = i;
        }

        if (FPEnumerate(Conn, vol,  dir , "",
                        (1<<FILPBIT_LNAME) | (1<<FILPBIT_FNUM ) | (1<<FILPBIT_ATTR) | (1<<FILPBIT_FINFO)|
                        (1<<FILPBIT_CDATE) | (1<<FILPBIT_BDATE) | (1<<FILPBIT_MDATE) |
                        (1<<FILPBIT_DFLEN) | (1<<FILPBIT_RFLEN)
                        ,
                        (1<< DIRPBIT_ATTR) |  (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_FINFO) |
                        (1<<DIRPBIT_CDATE) | (1<<DIRPBIT_BDATE) | (1<<DIRPBIT_MDATE) |
                        (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID) | (1<< DIRPBIT_DID)|(1<< DIRPBIT_ACCESS)
                )) {
            fatal_failed();
        }

        starttimer();
        for (i=1; i <= maxi; i++) {
            sprintf(temp, "File.small%d", i);
            if (is_there(Conn, dir, temp)) {
                fatal_failed();
            }
            if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x72d,0)) {
                fatal_failed();
            }
            if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x73f, 0x133f )) {
                fatal_failed();
            }
            fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0x342, dir, temp, OPENACC_RD);
            if (!fork) {
                fatal_failed();
            }
            else {
                if (FPGetForkParam(Conn, fork, (1<<FILPBIT_DFLEN))) {fatal_failed();}
                if (FPRead(Conn, fork, 0, 512, data)) {fatal_failed();}
                if (FPCloseFork(Conn,fork)) {fatal_failed();}
            }
            fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0x342 , dir, temp,OPENACC_RD| OPENACC_DWR);
            if (!fork) {
                fatal_failed();
            }
            else {
                if (FPGetForkParam(Conn, fork, 0x242)) {fatal_failed();}
                if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x72d,0)) {
                    fatal_failed();
                }
                if (FPCloseFork(Conn,fork)) {fatal_failed();}
            }
        }
        stoptimer();
        addresult(TEST_OPENSTATREAD, Iterations);

        /* ---------------- */
        for (i=0; i <= maxi; i++) {
            sprintf(temp, "File.small%d", i);
            if (is_there(Conn, dir, temp)) {
                fatal_failed();
            }
            if (FPGetFileDirParams(Conn, vol,  dir, temp, 0, (1<< FILPBIT_FNUM) )) {
                fatal_failed();
            }

            if (FPDelete(Conn, vol,  dir, temp)) {fatal_failed();}
        }

        if (FPGetVolParam(Conn, vol, (1 << VOLPBIT_MDATE )|(1 << VOLPBIT_XBFREE))) {
            fatal_failed();
        }
    }

    /* -------- */
    /* Test (2) */
    if (teststorun[TEST_WRITE100MB]) {
        strcpy(temp, "File.big");
        if (FPCreateFile(Conn, vol,  0, dir , temp)){
            failed();
            goto fin1;
        }
        if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x72d,0))
            fatal_failed();
        if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x73f, 0x133f ))
            fatal_failed();

        fork = FPOpenFork(Conn, vol, OPENFORK_DATA ,
                          (1<<FILPBIT_PDID)|(1<<FILPBIT_FNUM)|(1<<FILPBIT_DFLEN)
                          ,dir, temp, OPENACC_WR |OPENACC_RD| OPENACC_DWR| OPENACC_DRD);
        if (!fork)
            fatal_failed();
        if (FPGetForkParam(Conn, fork, (1<<FILPBIT_PDID)))
            fatal_failed();

        starttimer();
        for (i=0; i <= numread ; i++) {
            if (FPWrite(Conn, fork, i*65536, 65536, data, 0 )) {
                fatal_failed();
            }
        }
        if (FPCloseFork(Conn,fork))
            fatal_failed();
        stoptimer();
        addresult(TEST_WRITE100MB, Iterations);
    }

    /* -------- */
    /* Test (3) */
    if (teststorun[TEST_READ100MB]) {
        if (is_there(Conn, dir, temp) != AFP_OK)
            fatal_failed();
        if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x72d, 0))
            fatal_failed();

        fork = FPOpenFork(Conn, vol, OPENFORK_DATA, 0x342, dir, temp,
                          OPENACC_RD|OPENACC_DWR);
        if (!fork)
            fatal_failed();

        if (FPGetForkParam(Conn, fork, 0x242))
            fatal_failed();
        if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x72d,0))
            fatal_failed();

        starttimer();
        for (i=0; i <= numread ; i++) {
            if (FPRead(Conn, fork, i*65536, 65536, data)) {
                fatal_failed();
            }
        }
        if (FPCloseFork(Conn,fork))
            fatal_failed();
        stoptimer();
        addresult(TEST_READ100MB, Iterations);
    }

    if (teststorun[TEST_WRITE100MB] || teststorun[TEST_READ100MB])
        FPDelete(Conn, vol,  dir, "File.big");

    /* -------- */
    /* Test (4) */
    if (teststorun[TEST_LOCKUNLOCK]) {
        strcpy(temp, "File.lock");
        if (ntohl(AFPERR_NOOBJ) != is_there(Conn, dir, temp))
            fatal_failed();

        if (FPGetFileDirParams(Conn, vol,  dir, "", 0, (1<< DIRPBIT_DID) ))
            fatal_failed();
        if (FPCreateFile(Conn, vol,  0, dir , temp))
            fatal_failed();
        if (is_there(Conn, dir, temp))
            fatal_failed();

        if (FPGetFileDirParams(Conn, vol,  dir, temp,
                               (1<<FILPBIT_FNUM )|(1<<FILPBIT_PDID)|(1<<FILPBIT_FINFO)|
                               (1<<FILPBIT_CDATE)|(1<<FILPBIT_DFLEN)|(1<<FILPBIT_RFLEN)
                               , 0))
            fatal_failed();

        fork = FPOpenFork(Conn, vol, OPENFORK_DATA ,
                          (1<<FILPBIT_PDID)|(1<<FILPBIT_FNUM)|(1<<FILPBIT_DFLEN)
                          , dir, temp, OPENACC_WR|OPENACC_RD|OPENACC_DWR|OPENACC_DRD);
        if (!fork)
            fatal_failed();

        if (FPGetForkParam(Conn, fork, (1<<FILPBIT_PDID)|(1<<FILPBIT_DFLEN)))
            fatal_failed();

        if (FPGetFileDirParams(Conn, vol,  dir, temp,
                               (1<< DIRPBIT_ATTR)|(1<<FILPBIT_CDATE)|(1<<FILPBIT_MDATE)|
                               (1<<FILPBIT_FNUM)|
                               (1<<FILPBIT_FINFO)|(1<<FILPBIT_DFLEN)|(1<<FILPBIT_RFLEN)
                               , 0))
            fatal_failed();

        if (FPWrite(Conn, fork, 0, 40000, data, 0 ))
            fatal_failed();

        if (FPCloseFork(Conn,fork))
            fatal_failed();

        if (is_there(Conn, dir, temp))
            fatal_failed();
        if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x73f, 0x133f))
            fatal_failed();

        fork = FPOpenFork(Conn, vol, OPENFORK_DATA , 0x342 , dir, temp,OPENACC_RD);
        if (!fork)
            fatal_failed();

        if (FPGetForkParam(Conn, fork, (1<<FILPBIT_DFLEN))) {fatal_failed();}
        if (FPRead(Conn, fork, 0, 512, data)) {fatal_failed();}
        if (FPCloseFork(Conn,fork)) {fatal_failed();}

        fork = FPOpenFork(Conn, vol, OPENFORK_DATA, 0x342, dir, temp,
                          OPENACC_RD|OPENACC_WR);

        if (!fork)
            fatal_failed();

        if (FPGetForkParam(Conn, fork, 0x242))
            fatal_failed();
        if (FPGetFileDirParams(Conn, vol,  dir, temp, 0x72d, 0))
            fatal_failed();

        starttimer();
        for (j = 0; j < locking; j++) {
            for (i = 0;i <= 390; i += 10) {
                if (FPByteLock(Conn, fork, 0, 0 , i , 10)) {fatal_failed();}
            }
            for (i = 390;i >= 0; i -= 10) {
                if (FPByteLock(Conn, fork, 0, 1 , i , 10)) {fatal_failed();}
            }
        }
        stoptimer();
        addresult(TEST_LOCKUNLOCK, Iterations);

        if (is_there(Conn, dir, temp)) {fatal_failed();}
        if (FPCloseFork(Conn,fork)) {fatal_failed();}
        if (FPDelete(Conn, vol,  dir, "File.lock")) {fatal_failed();}
    }

    /* -------- */
    /* Test (5) */
    if (teststorun[TEST_CREATE2000FILES]) {
        starttimer();
        for (i=1; i <= create_enum_files; i++) {
            sprintf(temp, "File.0k%d", i);
            if (FPCreateFile(Conn, vol,  0, dir , temp)){
                fatal_failed();
                break;
            }
            if (FPGetFileDirParams(Conn, vol,  dir, temp,
                                   (1<<FILPBIT_FNUM )|(1<<FILPBIT_PDID)|(1<<FILPBIT_FINFO)|
                                   (1<<FILPBIT_CDATE)|(1<<FILPBIT_DFLEN)|(1<<FILPBIT_RFLEN)
                                   , 0) != AFP_OK)
                fatal_failed();
        }
        maxi = i;

        stoptimer();
        addresult(TEST_CREATE2000FILES, Iterations);
    }

    /* -------- */
    /* Test (6) */
    if (teststorun[TEST_ENUM2000FILES]) {
        starttimer();
        for (i=1; i <= create_enum_files; i +=32) {
            if (FPEnumerateFull(Conn, vol,  i, 32, DSI_DATASIZ, dir , "",
                                (1<<FILPBIT_LNAME) | (1<<FILPBIT_FNUM ) | (1<<FILPBIT_ATTR) |
                                (1<<FILPBIT_FINFO) | (1<<FILPBIT_CDATE) | (1<<FILPBIT_BDATE)|
                                (1<<FILPBIT_MDATE) | (1<<FILPBIT_DFLEN) | (1<<FILPBIT_RFLEN)
                                ,
                                (1<< DIRPBIT_ATTR) |  (1<<DIRPBIT_ATTR) | (1<<DIRPBIT_FINFO)|
                                (1<<DIRPBIT_CDATE) | (1<<DIRPBIT_BDATE) | (1<<DIRPBIT_MDATE)|
                                (1<< DIRPBIT_LNAME) | (1<< DIRPBIT_PDID) | (1<< DIRPBIT_DID)|
                                (1<< DIRPBIT_ACCESS)))
                fatal_failed();
        }
        stoptimer();
        addresult(TEST_ENUM2000FILES, Iterations);
    }

    /* Delete files from Test (5/6) */
    if (teststorun[TEST_CREATE2000FILES]) {
        for (i=1; i < maxi; i++) {
            sprintf(temp, "File.0k%d", i);
            if (FPDelete(Conn, vol,  dir, temp))
                fatal_failed();
        }
    }

    /* -------- */
    /* Test (7) */
    if (teststorun[TEST_DIRTREE]) {
        uint32_t idirs[DIRNUM];
        uint32_t jdirs[DIRNUM][DIRNUM];
        uint32_t kdirs[DIRNUM][DIRNUM][DIRNUM];

        starttimer();
        for (i=0; i < DIRNUM; i++) {
            sprintf(temp, "dir%02u", i+1);
            FAILEXIT(!(idirs[i] = FPCreateDir(Conn,vol, dir, temp)), fin1);

            for (j=0; j < DIRNUM; j++) {
                sprintf(temp, "dir%02u", j+1);
                FAILEXIT(!(jdirs[i][j] = FPCreateDir(Conn,vol, idirs[i], temp)), fin1);

                for (k=0; k < DIRNUM; k++) {
                    sprintf(temp, "dir%02u", k+1);
                    FAILEXIT(!(kdirs[i][j][k] = FPCreateDir(Conn,vol, jdirs[i][j], temp)), fin1);
                }
            }
        }
        stoptimer();
        addresult(TEST_DIRTREE, Iterations);

        for (i=0; i < DIRNUM; i++) {
            for (j=0; j < DIRNUM; j++) {
                for (k=0; k < DIRNUM; k++) {
                    FAILEXIT(FPDelete(Conn,vol, kdirs[i][j][k], "") != 0, fin1);
                }
                FAILEXIT(FPDelete(Conn,vol, jdirs[i][j], "") != 0, fin1);
            }
            FAILEXIT(FPDelete(Conn,vol, idirs[i], "") != 0, fin1);
        }
    }

fin1:
    FPDelete(Conn, vol,  dir, "");
fin:
    return;
}

/* ------------------ */
static void run_one()
{
    dsi = &Conn->dsi;
    vol  = FPOpenVol(Conn, Vol);
    if (vol == 0xffff) {
        nottested();
        return;
    }
    while (Iterations--) {
        test143();
    }
}

/* =============================== */
void usage( char * av0 )
{
    int i=0;
    fprintf( stderr, "usage:\t%s -h host [-m|v|V] [-3|4|5] [-p port] [-s vol] [-u user] [-w password] [-n iterations] [-t tests to run]\n", av0 );
    fprintf( stderr,"\t-m\tserver is a Mac (ignore this too!)\n");
    fprintf( stderr,"\t-h\tserver host name (default localhost)\n");
    fprintf( stderr,"\t-p\tserver port (default 548)\n");
    fprintf( stderr,"\t-s\tvolume to mount (default home)\n");
    fprintf( stderr,"\t-u\tuser name (default uid)\n");
    fprintf( stderr,"\t-w\tpassword (default none)\n");
    fprintf( stderr,"\t-3\tAFP 3.0 version\n");
    fprintf( stderr,"\t-4\tAFP 3.1 version (default)\n");
    fprintf( stderr,"\t-5\tAFP 3.2 version\n");
    fprintf( stderr,"\t-n\thow often to run (default: 1)\n");
    fprintf( stderr,"\t-v\tverbose\n");
    fprintf( stderr,"\t-V\tvery verbose\n");
    fprintf( stderr,"\t-t\ttests to run, eg 134 for tests 1, 3 and 4\n");
    fprintf( stderr,"\tAvailable tests:\n");
    for (i = 0; i < NUMTESTS; i++)
        fprintf( stderr,"\t(%u) %s\n", i+1, resultstrings[i]);
    exit (1);
}

/* ------------------------------- */
int main(int ac, char **av)
{
    int cc, i, t;
    int Debug = 0;
    char *tests = NULL;
    static char *vers = "AFP3.1";
    static char *uam = "Cleartxt Passwrd";

    while (( cc = getopt( ac, av, "t:mvV345h:n:p:s:u:w:c:" )) != EOF ) {
        switch ( cc ) {
        case 't':
            tests = strdup(optarg);
            break;
        case '3':
            vers = "AFPX03";
            Version = 30;
            break;
        case '4':
            vers = "AFP3.1";
            Version = 31;
            break;
        case '5':
            vers = "AFP3.2";
            Version = 32;
            break;
        case 'm':
            Mac = 1;
            break;
        case 'n':
            Iterations = atoi(optarg);
            break;
        case 'h':
            Server = strdup(optarg);
            break;
        case 's':
            Vol = strdup(optarg);
            break;
        case 'u':
            User = strdup(optarg);
            break;
        case 'w':
            Password = strdup(optarg);
            break;
        case 'p' :
            Port = atoi( optarg );
            if (Port <= 0) {
                fprintf(stderr, "Bad port.\n");
                exit(1);
            }
            break;
        case 'v':
            Debug = 1;
            break;
        case 'V':
            Verbose = 1;
            break;
        default :
            usage( av[ 0 ] );
        }
    }

    if (! Server)
        usage( av[ 0 ] );

    if (! Debug) {
        Verbose = 0;
        freopen("/dev/null", "w", stderr);
    }

    Iterations_save = Iterations;
    results = calloc(Iterations * NUMTESTS, sizeof(unsigned long));

    if (!tests) {
        memset(teststorun, 1, NUMTESTS);
    } else {
        i = 0;
        for (; tests[i]; i++) {
            t = tests[i] - '1';
            if ((t >= 0) && (t <= NUMTESTS))
                teststorun[t] = 1;
        }
        if (teststorun[TEST_READ100MB])
            teststorun[TEST_WRITE100MB] = 1;
        if (teststorun[TEST_ENUM2000FILES])
            teststorun[TEST_CREATE2000FILES] = 1;
    }

    /************************************
     *                                  *
     * Connection user 1                *
     *                                  *
     ************************************/

    if ((Conn = (CONN *)calloc(1, sizeof(CONN))) == NULL) {
        return 1;
    }
    Conn->type = Proto;
    if (!Proto) {
        int sock;
        Dsi = &Conn->dsi;
        dsi = Dsi;
        sock = OpenClientSocket(Server, Port);
        if ( sock < 0) {
            return 2;
        }
        Dsi->protocol = DSI_TCPIP;
        Dsi->socket = sock;
    }

    /* login */
    ExitCode = ntohs(FPopenLogin(Conn, vers, uam, User, Password));

    if (ExitCode == AFP_OK) {
        Conn->afp_version = Version;
        run_one();
        FPLogOut(Conn);
    }

    if (ExitCode) {
        if (! Debug)
            printf("Error, ExitCode: %u. Run with -v to see what went wrong.\n", ExitCode);
    } else
        displayresults();

    return ExitCode;
}
