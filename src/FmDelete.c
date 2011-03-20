/*---------------------------------------------------------------------------
  Module FmDelete

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995

  Functions for implementing the delete operation
---------------------------------------------------------------------------*/

#include <errno.h>
#include <X11/Intrinsic.h>

#include "Fm.h"

/*---------------------------------------------------------------------------*/

static Boolean abort_delete;

static void rdel(char *path, Boolean rm_only_dirs);
static void rmError(char *path, Boolean abort_prompt);

/*---------------------------------------------------------------------------*/

static void rmError(char *path, Boolean abort_prompt)
{
 char *err = "Error deleting";

 if (abort_prompt)
 {
     if (opError(getAnyShell(), err, path) != YES)
	 abort_delete = True;
 }
 else
 {
     char msg[MAXPATHLEN+16];

     sprintf(msg, "%s %s", err, path);
     sysError(getAnyShell(), msg);
 }
}

/*---------------------------------------------------------------------------*/

void fileDeleteCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SelFileNamesRec *fnames;

 fnames = getSelFiles(fw);
 if (!fnames)  return;
 deleteFilesDialog(fnames);
}

/*---------------------------------------------------------------------------*/

void deleteFilesProc(XtPointer fsel, int conf)
{
 SelFileNamesRec *fnames = (SelFileNamesRec *) fsel;
 struct stat stats;
 String name;
 int i;

 if (conf != YES)
 {
     freeSelFiles(fnames);
     return;
 }
 if (chdir(fnames->directory))
 {
     sysError(fnames->shell, "System error:");
     freeSelFiles(fnames);
     return;
 }
 if (!fnames->first)
     abort_delete = False;
 for (i = fnames->first; i < fnames->n_sel && !abort_delete; i++)
 {
     if (!(lstat(name = fnames->names[i], &stats)))
     {
	 if (!S_ISLNK(stats.st_mode) && S_ISDIR(stats.st_mode))
	 {
	     if (!(strcmp(name, ".")) || !(strcmp(name, "..")))
	     {
		 error(fnames->shell, "Cannot delete . or ..", NULL);
		 continue;
	     }
	     fnames->first = i;
	     chdir(user.home);
	     deleteDirDialog(fnames, name);
	     return;
	 }
	 else
	 {
	     if (unlink(name))
	     {
		 if (opError(fnames->shell, "Error deleting", fnames->names[i]) != YES)
		     break;
	     }
	     else fnames->update = True;
	 }
     }
     else if (opError(fnames->shell, "Error deleting", fnames->names[i]) != YES)
	 break;
 }
 chdir(user.home);
 if (fnames->update)
 {
     markForUpdate(fnames->directory, CHECK_DIR);
     intUpdate(CHECK_DIR);
 }
 freeSelFiles(fnames);
}

/*---------------------------------------------------------------------------*/

void deleteDirProc(XtPointer fsel, int conf)
{
 SelFileNamesRec *fnames = (SelFileNamesRec *) fsel;

 switch (conf)
 {
     case CANCEL:	fnames->first = fnames->n_sel; break;
     case YES:
	 if (chdir(fnames->directory))
	     sysError(fnames->shell, "System error:");
	 else
	 {
	     rdel(fnames->names[fnames->first], False);
	     chdir(user.home);
	     fnames->update = True;
	 }
     case NO:		fnames->first++;
 }
 deleteFilesProc(fsel, YES);
}

/*-------------------------------------------------------------------------*/

void rdelete(char *path)
{
 abort_delete = False;
 rdel(path, False);
}

/*-------------------------------------------------------------------------*/

void rmDirs(char *path)
{
 abort_delete = False;
 rdel(path, True);
}

/*-------------------------------------------------------------------------*/

/* recursive delete */

void rdel(char *path, Boolean rm_only_dirs)
{
 DIR *dir;
 struct dirent *entry;
 struct stat stats;
 int i, pl = strlen(path), res;
  
 if (abort_delete || lstat(path, &stats) || !(dir = opendir(path)))
     return;

 if (!(permission(&stats, P_WRITE)))
 {
     closedir(dir);
     errno = EPERM;
     rmError(path, !rm_only_dirs);
     return;
 }

 for (i=0; (entry = readdir(dir)); i++)
 {
      if (entry->d_name[0] != '.' || (entry->d_name[1] != '\0' && (entry->d_name[1] != '.' || entry->d_name[2] != '\0')))
      {
	  int pl1 = pl, l = strlen(entry->d_name);
	  char *path1 = (char *)alloca(pl1+l+2);

	  strcpy(path1, path);
	  if (path1[pl1-1] != '/')  path1[pl1++] = '/';
	  strcpy(path1+pl1, entry->d_name);
	  if (!(res = lstat(path1, &stats)))
	  {
	      if (!S_ISLNK(stats.st_mode) && S_ISDIR(stats.st_mode))
		  rdel(path1, rm_only_dirs);
	      else if (!rm_only_dirs) res = unlink(path1);
	  }
	  if (res)
	  {
	      rmError(path1, !rm_only_dirs);
	      if (abort_delete)  break;
	  }
      }
 }
 if (closedir(dir) || rmdir(path))
     rmError(path, !rm_only_dirs);
}
