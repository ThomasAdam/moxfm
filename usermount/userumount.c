#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <mntent.h>
#include <sys/mount.h>
#include <sys/stat.h>

#ifdef LOGFILE
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#endif

#define VERSION "1.0"
#define UFSTAB "/usr/local/lib/userfstab"
#define MTAB "/etc/mtab"
#define TMPTAB "/etc/newmtab"

#ifdef LOGFILE
void logAction(char *device, int err);

void logAction(char *device, int err)
{
 struct passwd *pwent;
 FILE *flog = fopen(LOGFILE, "a");
 time_t now;

 if (flog)
 {
     pwent = getpwuid(getuid());
     now = time(NULL);
     fprintf(flog, "%s", asctime(localtime(&now)));
     if (err)
	 fprintf(flog, "Unsuccessfull trial to unmount %s by %s: %s\n", device, pwent->pw_name, sys_errlist[err]);
     else
	 fprintf(flog, "%s unmounted by %s\n", device, pwent->pw_name);
     fclose(flog);
 }
 else
     fprintf(stderr, "Unable to open log file %s for writing.\n", LOGFILE);

}
#endif

int main(int argc, char *argv[])
{
 struct mntent *entry;
 struct stat stats;
 uid_t uid = getuid();
 FILE *ufstab;
 FILE *tmptab;
 FILE *mtab;
 char *warning = "Warning: Cannot update";
 int found = 0;

 if (!(ufstab = setmntent(UFSTAB, "r")))
 {
     fprintf(stderr, "userumount: Could not open %s\n", UFSTAB);
     exit(-1);
 }
 if (argc != 2)
 {
     fprintf(stderr, "userumount - Usage: usermount <device>\n");
     exit(-2);
 }
 if (!strcmp(argv[1], "-version"))
 {
     printf("userumount version %s\nUser filesystem table: %s\n", VERSION, UFSTAB);
#ifdef SET_OWNER
     printf("Caution: The user is made the owner of read-write mounted filesystems.\n");
#else
     printf("The ownerships of mounted filesystems other than DOS FAT and HPFS are not changed.\n");
#endif
#ifdef LOGFILE
     printf("(Un)mount actions are logged in %s.\n", LOGFILE);
#else
     printf("(Un)mount actions are not logged.\n");
#endif
     exit(0);
 }

 while ((entry = getmntent(ufstab)))
 {
     if (!strcmp(argv[1], entry->mnt_fsname) || !strcmp(argv[1], entry->mnt_dir))
     {
	 found = 1;
	 break;
     }
 }
#ifdef SET_OWNER	
 if (found && uid && !hasmntopt(entry, "ro"))
#else
 if (found && uid && (!strcmp(entry->mnt_type, MNTTYPE_MSDOS)  || !strcmp(entry->mnt_type, MNTTYPE_HPFS)))
#endif
 {
     if (lstat(entry->mnt_dir, &stats))
     {
	 fprintf(stderr, "userumount: Could not lstat %s: %s\n", entry->mnt_dir, sys_errlist[errno]);
	 endmntent(ufstab);
	 exit(-4);
     }
     if (stats.st_uid != uid)
     {
	 fprintf(stderr, "userumount: %s is not mounted or it is used by another user\n", argv[1]);
	 endmntent(ufstab);
	 exit(-5);
     }
 }
 endmntent(ufstab);
 if (!found)
 {
     fprintf(stderr, "userumount: no entry matching %s found in %s\n", argv[1], UFSTAB);
     exit(-3);
 }
 if (umount(argv[1]))
 {
     fprintf(stderr, "userumount: Could not unmount %s: %s\n", argv[1], sys_errlist[errno]);
#ifdef LOGFILE
     logAction(argv[1], errno);
#endif
     exit(errno);
 }
 else
 {
#ifdef LOGFILE
     logAction(argv[1], 0);
#endif
     if (!(tmptab = setmntent(TMPTAB, "w")))
     {
	 fprintf(stderr, "userumount: Error writing %s: %s\n -> %s %s\n", TMPTAB, sys_errlist[errno], warning, MTAB);
	 exit(1);
     }
     if (!(mtab = setmntent(MTAB, "r")))
     {
	 fprintf(stderr, "userumount: Warning - error reading %s: %s\n", MTAB, sys_errlist[errno]);
	 endmntent(tmptab);
	 unlink(TMPTAB);
	 exit(1);
     }
     while ((entry = getmntent(mtab)))
     {
	 if (strcmp(entry->mnt_fsname, argv[1]) && strcmp(entry->mnt_dir, argv[1]))
	 {
	     if (addmntent(tmptab, entry))
	     {
		 fprintf(stderr, "userumount: Error writing %s: %s\n -> %s %s\n", TMPTAB, sys_errlist[errno], warning, MTAB);
		 endmntent(tmptab);
		 endmntent(mtab);
		 unlink(TMPTAB);
		 exit(1);
	     }
	 }
     }
     endmntent(mtab);
     endmntent(tmptab);
     if (rename(TMPTAB, MTAB))
     {
	 fprintf(stderr, "userumount: %s %s : %s\n", warning, MTAB, sys_errlist[errno]);
	 unlink(TMPTAB);
     }
 }
 return 0;
}
