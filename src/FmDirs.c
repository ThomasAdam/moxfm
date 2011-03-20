/*---------------------------------------------------------------------------
  Module FmDirs.c                                                           

  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

  functions for manipulating directory lists, and some other utilities
  related to the file system.

  modified 1-29-95 by rodgers@lvs-emh.lvs.loral.com (Kevin M. Rodgers)
  to add filtering of icon/text directory displays by a filename filter.

-----------------------------------------------------------------------------*/

#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef SVR4
#ifndef USE_STATVFS
#define USE_STATVFS
#endif
#endif	/* SVR4 */

#ifdef USE_STATVFS
#include <sys/types.h>	/* apparently needed on SunOS 4.x */
#include <sys/statvfs.h>
#else
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/mount.h>
#else
#ifdef OSF1
#include <sys/vfs_proto.h>
#else
#include <sys/vfs.h>
#endif	/* OSF1 */
#endif	/* __NetBSD__ */
#endif	/* USE_STATVFS */

#ifdef _AIX
#include <sys/statfs.h>
#endif

#include <X11/Intrinsic.h>
#include <Xm/MessageB.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/Xm.h>

#include "Fm.h"

/*-----------------------------------------------------------------------------
  STATIC DATA                                       
-----------------------------------------------------------------------------*/

static SortType sort_type;
static Boolean dirs_first;

/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static int comp(FileRec **fr1, FileRec **fr2)
{
  FileRec *fl1 = *fr1, *fl2 = *fr2;

  if (dirs_first) {
    if (S_ISDIR(fl1->stats.st_mode)) {
      if (!S_ISDIR(fl2->stats.st_mode))
	return -1;
    }
    else if (S_ISDIR(fl2->stats.st_mode))
      return 1;
  }
	
  switch (sort_type) {
  case SortByName:
    return strcmp(fl1->name, fl2->name);
  case SortBySize:
    return (int)(fl2->stats.st_size - fl1->stats.st_size);
  case SortByMTime:
    return (int)(fl2->stats.st_mtime - fl1->stats.st_mtime);
  }

  return 0;
}

/*---------------------------------------------------------------------------*/

static int fileStats(FileWindowRec *fw, FileRec *file)
{
    if (lstat(file->name, &file->stats))
	return -1;
    if (S_ISLNK(file->stats.st_mode))
    {
	file->sym_link = True;
	stat(file->name, &(file->stats));
    }
    else  file->sym_link = False;
    fw->n_bytes += file->stats.st_size;

    return 0;
}

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

/* Read in the directory for the file window specified. Note that since we have
   the stats available for the directory, we could simply check the modification
   time, and only read in the directory if necessay. This isn't worth it though-
   the time taken to stat all the files (which still needs to be done) far
   outweighs the time to read in the dir */

Boolean readDirectory(FileWindowRec *fw)
{
  FileList fl = NULL;
  DIR *dir;
  struct dirent *entry;
  int i, m, n_alloc = 0;

  if (stat(fw->directory, &fw->stats))
    goto error2;

  if (chdir(fw->directory))
    goto error2;

  if (!(dir = opendir(".")))
    goto error2;

  fw->n_bytes = 0;

  for (i = 0; (entry = readdir(dir)); i++)
  {
      if (i == n_alloc)
	  fl = (FileRec **) XTREALLOC(fl, (n_alloc = i+10) * sizeof(FileRec *));
      fl[i] = (FileRec *) XtMalloc(sizeof(FileRec));
      strcpy(fl[i]->name, entry->d_name);

      if (fileStats(fw, fl[i]))
	  goto error1;
  }

  if (closedir(dir))
      goto error1;

  fw->files = fl;
  fw->n_files = i;
  chdir(user.home);
  return True;

 error1:
  for (m = 0; m <= i; m++)  XTFREE(fl[m]);
  XTFREE(fl);

 error2:
  fw->n_files = 0;
  fw->files = NULL;
  chdir(user.home);
  return False;
} 

/*---------------------------------------------------------------------------*/

#ifdef MAGIC_HEADERS

void getMagicTypes(FileWindowRec *fw)
{
 FileRec *file;
 Boolean ierrfl, errfl = False, stopfl = False;
 int i;

 if (fw->magic_headers && !fw->slow_device && fw->readable)
 {
     if (chdir(fw->directory))
	 stopfl = True;
     else
     {
	 do
	 {
	     ierrfl = False;
	     if (errfl)
	     {
		 if (fw->readable)  freeFileList(fw->files, fw->n_files);
		 readFiles(fw);
		 if (chdir(fw->directory))
		 {
		     stopfl = True;
		     break;
		 }
	     }
	     fw->n_bytes = 0;
	     for (i=0; i < fw->n_files; i++)
	     {
		 file = fw->files[i];
		 /* Reread file stat in case
		  something has changed */
		 if (fileStats(fw, file))
		 {
		     if (errfl)  stopfl = True;
		     ierrfl = True;
		     break;
		 }
		 magic_get_type(file->name, file->magic_type);
	     }
	     errfl = ierrfl;
	 } while (errfl && !stopfl);
	 chdir(user.home);
     }
 }
 if (stopfl)
 {
     if (fw->readable)
     {
	 freeFileList(fw->files, fw->n_files);
	 fw->readable = False;
     }
     fw->files = NULL;
     fw->n_files = 0;
 }
}

#endif

/*-----------------------------------------------------------------------------
  Remove either files or directories from a FileList
-----------------------------------------------------------------------------*/

void filterDirectory(FileWindowRec *fw, FilterType type)
{
  FileList fl = NULL, oldfl = fw->files;
  int n = 0, m = 0;

#ifdef DEBUG_MALLOC
  fprintf(stderr,"entering filterDirectory: %lu\n",malloc_inuse(NULL));
#endif

  fw->n_bytes = 0;

  for (; m < fw->n_files; m++) {
    if (
	( !strcmp(oldfl[m]->name,".") && (type == Directories) ) ||
	( strcmp(oldfl[m]->name,".") && 
	 (
	  !strcmp(oldfl[m]->name,"..") ||
	  (
	   (fw->show_hidden || (oldfl[m]->name[0] != '.')) &&
	   (
	    (S_ISDIR(oldfl[m]->stats.st_mode) && (type != Files)) ||
	    (!S_ISDIR(oldfl[m]->stats.st_mode) && type != Directories)
	    )
	   )
	  )
	 )
	) {
      /* KMR now filter on dirFilter (I wouldn't dare mess with the above!) */
      /* AG modified to exclude folders from filtering */
      if ((type == Directories) || !fw->do_filter || 
	  S_ISDIR(oldfl[m]->stats.st_mode) ||
	  fnmultimatch(fw->dirFilter, oldfl[m]->name)) {
	fl = (FileList) XTREALLOC(fl, (n+1)*sizeof(FileRec *));
	fl[n] = oldfl[m];
	n++;
	fw->n_bytes += oldfl[m]->stats.st_size;
      }
      else
	XTFREE(oldfl[m]);
    }
    else
      XTFREE(oldfl[m]);
  }
  XTFREE(oldfl);
  
#ifdef DEBUG_MALLOC
  fprintf(stderr,"exiting filterDirectory: %lu\n",malloc_inuse(NULL));
#endif

  fw->n_files = n;
  fw->files = fl;
}  


/*-----------------------------------------------------------------------------
  Sort a directory according to the sort type and dfirst flag
-----------------------------------------------------------------------------*/

void sortDirectory(FileList fl, int n, SortType type, Boolean dfirst)
{
  sort_type = type;
  dirs_first = dfirst;
  qsort(fl, n, sizeof(FileRec *), (int (*)(const void *, const void *))comp);
}


/*-----------------------------------------------------------------------------
  Check if two directory paths point to the same directory, return 0 if true
-----------------------------------------------------------------------------*/

int dirComp(String dir1, String dir2)
{
 char path1[MAXPATHLEN], path2[MAXPATHLEN];

 if (!(resolvepath(dir1, path1)))  return -2;
 if (!(resolvepath(dir2, path2)))  return -2;
 return strcmp(path1, path2);
}

/*-----------------------------------------------------------------------------
  Check permission for an operation, equivalent to UNIX access()
-----------------------------------------------------------------------------*/

int permission(struct stat *stats, int perms)
{
  int mode = stats->st_mode;
  int result = 0;

  if (user.uid == 0 || user.uid == stats->st_uid) {
    if (mode & S_IRUSR)
      result |= P_READ;
    if (mode & S_IWUSR)
      result |= P_WRITE;
    if (mode & S_IXUSR)
      result |= P_EXECUTE;
  } 

  else if (user.uid == 0 || user.gid == stats->st_gid) {
    if (mode & S_IRGRP)
      result |= P_READ;
    if (mode & S_IWGRP)
      result |= P_WRITE;
    if (mode & S_IXGRP)
      result |= P_EXECUTE;
  } 

  else {
    if (mode & S_IROTH)
      result |= P_READ;
    if (mode & S_IWOTH)
      result |= P_WRITE;
    if (mode & S_IXOTH)
      result |= P_EXECUTE;
  } 

  return (result & perms) == perms;
}


/*---------------------------------------------------------------------------*/

void makePermissionsString(char *s, int perms, Boolean symlink)
{
  if (symlink)  s[0] = 'l';
  else if (S_ISDIR(perms))  s[0] = 'd';
  else s[0] = '-';
  s[1] = perms & S_IRUSR ? 'r' : '-'; 
  s[2] = perms & S_IWUSR ? 'w' : '-'; 
  s[3] = perms & S_IXUSR ? 'x' : '-'; 

  s[4] = perms & S_IRGRP ? 'r' : '-'; 
  s[5] = perms & S_IWGRP ? 'w' : '-'; 
  s[6] = perms & S_IXGRP ? 'x' : '-'; 

  s[7] = perms & S_IROTH ? 'r' : '-'; 
  s[8] = perms & S_IWOTH ? 'w' : '-'; 
  s[9] = perms & S_IXOTH ? 'x' : '-'; 
  
  s[10] = '\0';
}

/*---------------------------------------------------------------------------*/

void freeFileList(FileList files, int n_files)
{
 int i;

 if (files && n_files)
 {
     for (i = 0; i < n_files; i++)  XTFREE(files[i]);
     XTFREE(files);
 }
}  

/*---------------------------------------------------------------------------*/

void contractPath(String path)
{
 int i = 0, j, end = strlen(path)+1;

/* contract // */
 while (path[i])
 {
     if (path[i] == '/' && path[i+1] == '/')
	 memmove(&path[i], &path[i+1], (--end - i) * sizeof(char));
     else i++;
 }

/* interpret ./ and ../ */
 i = 0;
 while (path[i])
 {
     if (path[i] == '.' && (!i || path[i-1] == '/'))
     {
	 if (path[i+1] == '/')
	     memmove(&path[i], &path[i+2], ((end -= 2) - i) * sizeof(char));
	 else if (path[i+1] == 0)
	 {
	     path[i] = 0;
	     break;
	 }
	 else if (path[i+1] == '.' && (path[i+2] == '/' || path[i+2] == 0))
	 {
	     for (j=i-2; j>=0 && path[j] != '/'; j--) {};
	     if (j < 0)  i += 2;
	     else
	     {
		 if (!j && !path[i+2])
		 {
		     strcpy(path, "/");
		     break;
		 }
		 memmove(&path[j], &path[i+2], (end-i-2) *sizeof(char));
		 end -= i+2-j;
		 i = j;
	     }
	 }
	 else i++;
     }
     else i++;
 }
}

/*---------------------------------------------------------------------------*/

String absolutePath(String path)
{
 char dir[MAXPATHLEN];
 struct stat stats;
 Boolean dir_found = False;
 int i = -1;

 strcpy(dir, path);
 if (stat(dir, &stats) || !S_ISDIR(stats.st_mode))
 {
     for (i = strlen(dir)-1; i > 0; i--)
     {
	 if (dir[i] == '/')
	 {
	     dir_found = True;
	     dir[i] = 0;
	     if (!(stat(dir, &stats) && S_ISDIR(stats.st_mode)))
		 break;
	 }
     }
     if (!i)
     {
	 if (dir_found)
	     return NULL;
	 else
	     return XtNewString(path);
     }
 }
 if (!(chdir(dir)) && (getwd(dir)))
 {
     chdir(user.home);
     if (i > 0)
     {
	 strcat(dir, "/");
	 strcat(dir, &path[i+1]);
     }
 }
 else
 {
     chdir(user.home);
     return NULL;
 }
 return XtNewString(dir);
}

/*---------------------------------------------------------------------------*/

String relativePath(String path, String relative_to)
{
 char absPath[MAXPATHLEN], relTo[MAXPATHLEN];
 char *ret, *fname, *buffer = relTo;
 struct stat stats;
 size_t len = strlen(path);
 int i, n_up = 0;
 Boolean is_dir = False;

 strcpy(absPath, path);
 if (!(stat(path, &stats)) && S_ISDIR(stats.st_mode))
 {
     is_dir = True;
     fname = &path[len];
 }
 else
 {
     for (i=len-1; i>=0 && path[i] != '/'; i--) {};
     fname = &path[i+1];
     absPath[i+1] = 0;
 }
 if (!(strcmp(absPath, relative_to)))
     return XtNewString(fname);
 if (resolvepath(absPath, buffer))
     strcpy(absPath, buffer);
 else
     return XtNewString(path);
 if (absPath[strlen(absPath) - 1] != '/')
     strcat(absPath, "/");
 if (!(resolvepath(relative_to, relTo)))
     return XtNewString(path);
 if (relTo[strlen(relTo) - 1] != '/')
     strcat(relTo, "/");
 if (!(strcmp(absPath, relTo)))
 {
     if (fname[0])  return XtNewString(fname);
     else  return XtNewString(".");
 }
 len = strlen(relTo) - 1;
 while (strncmp(absPath, relTo, len + 1))
 {
     n_up++;
     for (i=len-1; i>=0 && relTo[i] != '/'; i--) {};
     len = i;
 }
 ret = (char *) XtMalloc((3 * n_up + strlen(&absPath[++len]) + strlen(fname) + 2) * sizeof(char));
 ret[0] = 0;
 for (i=0; i<n_up; i++)
     strcat(ret, "../");
 strcat(ret, &absPath[len]);
 strcat(ret, fname);
 if (is_dir && ret[len = strlen(ret) - 1] != '/')
 {
     ret[len + 1] = '/';
     ret[len + 2] = 0;
 }
 return ret;
}

/*---------------------------------------------------------------------------*/

#ifdef GB
#undef GB
#endif
#define GB 1000000L
#ifdef MB
#undef MB
#endif
#define MB 1000L

void fsInfo(FileWindowRec *fw)
{
 Widget dialog, form, line, labelw;
 String label;
 XmString str;
 char info[30];
 long blocks, bsize, ksize;
 long size, gbytes, mbytes, kbytes, rest;
 Dimension width;
 int i, res, per;
 Boolean cont;
 Arg args;
#ifdef USE_STATVFS
 struct statvfs stat;
#else
 struct statfs stat;
#endif

#ifdef USE_STATVFS
 res = statvfs(fw->directory, &stat);
#else
 res = statfs(fw->directory, &stat);
#endif

 if (res)
 {
     sysError(fw->shell, "Could not statfs directory:");
     return;
 }

#ifdef OSF1
 bsize = (long) stat.f_fsize / 1024L;
 if (!bsize)  ksize = 1024L / (long) stat.f_fsize;
#else
 bsize = (long) stat.f_bsize / 1024L;
 if (!bsize)  ksize = 1024L / (long) stat.f_bsize;
#endif

 XtSetArg(args, XmNforeground, resources.label_color);
 dialog = XmCreateInformationDialog(fw->shell, "Fsinfo", &args, 1);

 XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
 XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
 XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_MESSAGE_LABEL));

 form = XtVaCreateManagedWidget("form", xmFormWidgetClass, dialog, NULL);

 str = XmStringCreateLocalized("Free space available:");
 width = XmStringWidth(resources.bold_font, str) + 3;
 XmStringFree(str);

 for (i=0; i<5; i++)
 {
     cont = False;
     switch (i)
     {
	 case 0:
	     label = "File system size:";
	     blocks = stat.f_blocks; per = -1;
	     break;
	 case 1:
	     label = "Free space available:";
	     blocks = (geteuid())? stat.f_bavail : stat.f_bfree;
	     per = percent(blocks, stat.f_blocks);
	     break;
	 case 2:
	     label = "Used space:";
	     blocks = stat.f_blocks - stat.f_bfree;
	     per = percent(blocks, stat.f_blocks);
	     break;
	 case 3:
	     label = "Free inodes:";
	     if (stat.f_ffree == -1 || !stat.f_files)
		 cont = True;
	     else
	     {
		 size = stat.f_ffree;
		 per = percent(size, stat.f_files);
	     }
	     break;
	 case 4:
	     label = "Block size:";
	     size = stat.f_bsize;
	     per = -1;
     }

     if (cont)  continue;

     if (i < 3)
     {
	 if (bsize)
	     kbytes = blocks * bsize;
	 else
	     kbytes = blocks / ksize;
     }
     if (i == 3)
	 sprintf(info, "%ld", size);
     else if (i == 4)
	 sprintf(info, "%ld Bytes", size);
     else
     {
	 gbytes = kbytes / GB;
	 mbytes = (rest = kbytes - gbytes * GB) / MB;
	 kbytes = rest - mbytes * MB;
	 if (gbytes)
	     sprintf(info, "%ld,%03ld,%03ld", gbytes, mbytes, kbytes);
	 else if (mbytes)
	     sprintf(info, "%ld,%03ld", mbytes, kbytes);
	 else
	     sprintf(info, "%ld", kbytes);
	 sprintf(&info[strlen(info)], " %sBytes", (i==4)? "" : "K");
     }
     if (per >= 0)
	 sprintf(&info[strlen(info)], " (%d%%)", per);

     if (i)
	 line = XtVaCreateManagedWidget("line", xmFormWidgetClass, form, XmNforeground, resources.label_color, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, line, XmNrightAttachment, XmATTACH_FORM, XmNtopOffset, 5, NULL);
     else
	 line = XtVaCreateManagedWidget("line", xmFormWidgetClass, form, XmNforeground, resources.label_color, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);

     labelw = XtVaCreateManagedWidget(label, xmLabelGadgetClass, line, XmNfontList, resources.bold_font, XmNwidth, width, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_END, NULL);
     XtVaCreateManagedWidget(info, xmLabelGadgetClass, line, XmNfontList, resources.label_font, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, labelw, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
 }

 XtManageChild(dialog);
 XtPopup(XtParent(dialog), XtGrabNone);
}
