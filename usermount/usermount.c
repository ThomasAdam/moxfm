#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <mntent.h>
#include <sys/mount.h>
#include <linux/fs.h>

#ifdef LOGFILE
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#endif

#define VERSION "1.0"
#define UFSTAB "/usr/local/lib/userfstab"
#define MTAB "/etc/mtab"

#ifndef MS_SYNCHRONOUS
#define MS_SYNCHRONOUS MS_SYNC
#endif

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
	 fprintf(flog, "Unsuccessfull trial to mount %s by %s: %s\n", device, pwent->pw_name, sys_errlist[err]);
     else
	 fprintf(flog, "%s mounted by %s\n", device, pwent->pw_name);
     fclose(flog);
 }
 else
     fprintf(stderr, "Unable to open log file %s for writing.\n", LOGFILE);

}
#endif

int main(int argc, char *argv[])
{
 struct mntent *entry;
 FILE *ufstab;
 FILE *mtab;
 unsigned long rwflag = MS_MGC_VAL;
 int found = 0, errfl = 0, readonly = 0;

 if (argc != 2)
 {
     fprintf(stderr, "usermount - Usage: usermount <device>\n");
     exit(-2);
 }
 if (!strcmp(argv[1], "-version"))
 {
     printf("usermount version %s\nUser filesystem table: %s\n", VERSION, UFSTAB);
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

 if (!(ufstab = setmntent(UFSTAB, "r")))
 {
     fprintf(stderr, "usermount: Could not open %s\n", UFSTAB);
     exit(-1);
 }
 while ((entry = getmntent(ufstab)))
 {
     if (!strcmp(argv[1], entry->mnt_fsname) || !strcmp(argv[1], entry->mnt_dir))
     {
	 found = 1;
	 break;
     }
 }
 if (!found)
 {
     fprintf(stderr, "usermount: no entry matching %s found in %s\n", argv[1], UFSTAB);
     endmntent(ufstab);
     exit(-3);
 }
 if (hasmntopt(entry, "ro"))
 {
     readonly = 1;
     rwflag |= MS_RDONLY;
 }
 if (hasmntopt(entry, "nosuid"))
     rwflag |= MS_NOSUID;
 if (hasmntopt(entry, "nodev"))
     rwflag |= MS_NODEV;
 if (hasmntopt(entry, "noexec"))
     rwflag |= MS_NOEXEC;
 if (hasmntopt(entry, "sync"))
     rwflag |= MS_SYNCHRONOUS;
 if (mount(entry->mnt_fsname, entry->mnt_dir, entry->mnt_type, rwflag, NULL))
 {
     if (errno == EROFS || errno == EACCES)
     {
	 readonly = 1;
	 rwflag |= MS_RDONLY;
	 if (mount(entry->mnt_fsname, entry->mnt_dir, entry->mnt_type, rwflag, NULL))
	     errfl = 1;
     }
     else errfl = 1;
     if (errfl)
     {
	 fprintf(stderr, "usermount: Could not mount %s on %s: %s\n", entry->mnt_fsname, entry->mnt_dir, sys_errlist[errno]);
#ifdef LOGFILE
	 logAction(entry->mnt_fsname, errno);
#endif
	 endmntent(ufstab);
	 exit(errno);
     }
 }
 if  (!(mtab = setmntent(MTAB, "r+")) || addmntent(mtab, entry))
 {
     fprintf(stderr, "usermount: Warning - error updating %s: %s\n", MTAB, sys_errlist[errno]);
     endmntent(mtab);
 }
#ifdef SET_OWNER
 if (!readonly && strcmp(entry->mnt_type, MNTTYPE_MSDOS) && strcmp(entry->mnt_type, MNTTYPE_HPFS))
     if (chown(entry->mnt_dir, getuid(), getgid()))
	 fprintf(stderr, "usermount: Warning - unable to set ownership of %s\n", entry->mnt_dir);
#endif

#ifdef LOGFILE
	 logAction(entry->mnt_fsname, 0);
#endif

 endmntent(ufstab);

 return 0;
}
