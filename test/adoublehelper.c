/* ----------------------------------------------
*/
#include "specs.h"
#include "adoublehelper.h"

static char temp[MAXPATHLEN];   

/* -------------------- 
   delete a resource fork
*/
int delete_unix_rf(char *path, char *name, char *file)
{
	if (!*file) {
		sprintf(temp, "%s/%s/.AppleDouble/.Parent", path, name);
	}
	else {
		sprintf(temp, "%s/%s/.AppleDouble/%s", path, name, file);
	}
	fprintf(stderr,"unlink(%s)\n", temp);
	if (unlink(temp) <0) {
		fprintf(stderr,"\tFAILED unlink(%s) %s\n", temp, strerror(errno));
		failed_nomsg();
		return -1;
	}
	return 0;
}

/* ----------------------------- */
int delete_unix_adouble(char *path, char *name)
{
	fprintf(stderr,"rmdir(%s/.AppleDouble) \n", name);
	sprintf(temp, "%s/%s/.AppleDouble/.Parent", path, name);
	if (unlink(temp) <0) {
		fprintf(stderr,"\tFAILED unlink(%s) %s\n", temp, strerror(errno));
		failed_nomsg();
		return -1;
	}

	sprintf(temp, "%s/%s/.AppleDouble", path, name);
	if (rmdir(temp) <0) {
		fprintf(stderr,"\tFAILED rmdir(%s) %s\n", temp, strerror(errno));
		failed_nomsg();
		return -1;
	}
	return 0;
}

/* --------------------
*/
int chmod_unix_adouble(char *path,char *name, int mode)
{
	sprintf(temp, "%s/%s/.AppleDouble", path, name);
	fprintf(stderr, "chmod (%s, %o)\n", temp, mode);
	if (chmod(temp, mode)) {
		fprintf(stderr,"\tFAILED %s\n", strerror(errno));
		failed_nomsg();
		return -1;
	}
	return 0;
}

/* -------------------- 
	delete an empty directory
*/
int delete_unix_dir(char *path, char *name)
{
	fprintf(stderr,"rmdir(%s)\n", name);
	if (delete_unix_adouble(path, name)) {
		return -1;
	}
	sprintf(temp, "%s/%s", path, name);
	if (rmdir(temp) <0) {
		fprintf(stderr,"\tFAILED rmdir %s %s\n", temp, strerror(errno));
		failed_nomsg();
		return -1;
	}
	return 0;
}

/* ---------------------- 
 * create a folder with r-xr-xr-x .AppleDouble
*/
int folder_with_ro_adouble(u_int16_t vol, int did, char *name, char *file)
{
int ret = 0;
int dir = 0;
u_int16_t bitmap =  (1 << DIRPBIT_ACCESS);

	fprintf(stderr,"\t>>>>>>>> Create folder with ro adouble <<<<<<<<<< \n");

	if (!(dir = FPCreateDir(Conn,vol, did , name))) {
		nottested();
		goto fin;
	}

	if (FPGetFileDirParams(Conn, vol,  dir , "", 0,bitmap )) {
		nottested();
		goto fin;
	}

	if (FPCreateFile(Conn, vol,  0, dir , file)) {
		nottested();
		goto fin;
	}
	if (!Mac) {
		if (delete_unix_rf(Path, name, "")) {
			nottested();
		}
		else if (delete_unix_rf(Path, name, file)) {
			nottested();
		}
		else if (chmod_unix_adouble(Path,name,0555)) {
			nottested();
		}
		ret = dir;
	}
	else {
		ret = dir;
	}
fin:
	if (!ret && dir) {
		FPDelete(Conn, vol,  dir, file);
		if (FPDelete(Conn, vol,  did, name)) {
			nottested();
		}
	}
	fprintf(stderr,"\t>>>>>>>> done <<<<<<<<<< \n");
	return ret;
}

/* -------------------------------- */
int delete_ro_adouble(u_int16_t vol, int did, char *file)
{

	fprintf(stderr,"\t>>>>>>>> delete folder with ro adouble <<<<<<<<<< \n");
	FAIL (FPDelete(Conn, vol, did, file))
	FAIL (FPDelete(Conn, vol, did, ""))
	return 0;	
}

