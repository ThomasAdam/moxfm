/*---------------------------------------------------------------------------
  Module FmOps

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995

  Various file manipulation operations and other useful routines
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <X11/Intrinsic.h>

#include "Fm.h"

#define NBLOCKS 2

CopyDataRec cpy = { 0, 0, -1, -1, -1, NULL, 0 };

static Boolean abort_copy, move;
static ino_t *inodes;
static int n_inodes;

static void copyfile(char *oldpath, char *newpath);
static void copydir(struct stat *oldstats, char *oldpath, char *newpath);
static void copy(char *oldpath, char *newpath);

/*-------------------------------------------------------------------------*/

/* split a string into substrings delimited by a given character */

char *split(char *s, char c)
{
  static char *t;
  if (!s)
    s = t;
  t = s;
  if (t)
    while ((t = strchr(t, c)) && t>s && t[-1]=='\\') t++;
  if (t)
    *t++ = '\0';
  return s;
}

/*-------------------------------------------------------------------------*/

/* expand escapes in a string */

char *expand(char *s, char *t, char *c)
{
  char *s0 = s;
  for (; *t; t++) {
    if (strchr(c, *t))
      *s++ = '\\';
    *s++ = *t;
  }
  *s = '\0';
  return s0;
}

/*-------------------------------------------------------------------------*/

/* remove escapes from a string */

char *strparse(char *s, char *t, char *c)
{
  char *s0 = s;
  for (; *t; t++)
    if (*t != '\\')
      *s++ = *t;
    else if (*++t) {
      if (!(strchr(c, *t)))
	*s++ = '\\';
      *s++ = *t;
    } else
      break;
  *s = '\0';
  return s0;
}
      
/*-------------------------------------------------------------------------*/

/* expand filename */

char *fnexpand(char *fn)
{
  char s[MAXPATHLEN];
  int l;

  if (!fn || !user.home)
    return NULL;
  else if (fn[0] != '~' || (fn[1] != '\0' && fn[1] != '/'))
    return fn;

  l = strlen(user.home);

  if (l+strlen(fn)-1 >= MAXPATHLEN)
    return NULL;

  strcpy(s, user.home);
  strcpy(s+l, fn+1);
  return(strcpy(fn, s));
}

/*---------------------------------------------------------------------------*/

/* The following variant of fnmatch matches the . and .. dirs only if
   specified explicitly. */

#define fnmatchnodot(pattern,fn) (strcmp(fn,".")&&strcmp(fn,"..")? \
  fnmatch(pattern,fn):!strcmp(pattern,fn))

Boolean fnmultimatch(String pattern, String word)
{
 String buffer = XtNewString(pattern);
 String expword = (String) XtMalloc(2 * sizeof(char) * strlen(word));
 char *start, *s = buffer;
 Boolean res = False;

 expand(expword, word, "\\ ");
 while ((start = split(s, ' ')))
 {
     s = NULL;
     if (fnmatchnodot(start, expword))
     {
	 res = True;
	 break;
     }
 }
 XTFREE(buffer);
 XTFREE(expword);
 return res;
}

/* match a pattern with a filename, returning nonzero if the match was
   correct */

/* Currently only *, ? and [...] (character classes) are recognized, no curly
   braces. An escape mechanism for metacharacters is also missing. This could
   be implemented more efficiently, but the present simple backtracking
   routine does reasonably well for the usual kinds of patterns. -ag */

int fnmatch(String pattern, String fn)
{
  char *start;
  
  for (;; fn++, pattern++) {
    
    switch (*pattern) {
      
    case '?':
      if (!*fn)
	return 0;
      break;
      
    case '*':
      pattern++;
      do
	if (fnmatch(pattern,fn))
	  return 1;
      while (*fn++);
      return 0;
      
    case '[':
      start = pattern+1;
      do {
      next:
	pattern++;
	if (*pattern == ']')
	  return 0;
	else if (pattern[0] == '-' && pattern > start && pattern[1] != ']')
	  if (pattern[-1] <= *fn && *fn <= pattern[1])
	    break;
	  else {
	    start = (++pattern)+1;
	    goto next;
	  }
      } while (*fn != *pattern);
      while (*pattern != ']')
	if (!*pattern++)
	  return 0;
      break;
      
    default:
      if (*fn != *pattern)
	return 0;
    }
    
    if (!*fn)
      return 1;
  };
}

/*-------------------------------------------------------------------------*/

/* check whether one path is the prefix of another */

int prefix(char *s, char *t)
{
  int l = strlen(s);

  return !strncmp(s, t, l) && (s[l-1] == '/' || t[l] == '\0' || t[l] == '/');
}

/*-------------------------------------------------------------------------*/

/* check whether a file exists */

int exists(char *path)
{
  struct stat stats;

  return (!(lstat(path, &stats)));
}

/*-------------------------------------------------------------------------*/

/* find file on given search path */

char *searchPath(char *s1, char *p, char *s2)
{
  char           *s, *t;

  if (*s2 == '/' || *s2 == '~' || !p)
  {
      fnexpand(strcpy(s1, s2));
      return s1;
  }
  for (s = p; *s; s = t) {
    int l;
    if (!(t = strchr(s, ':')))
      t = strchr(s, 0);
    if (s == t) goto next;
    if (s[0] == '.')
      if ((t = s+1))
	s = t;
      else if (s[1] == '/')
	s += 2;
    l = t-s;
    strncpy(s1, s, l);
    if (l > 0 && s1[l - 1] != '/')
      s1[l] = '/', l++;
    strcpy(s1+l, s2);
    fnexpand(s1);
    if (exists(s1))
      return s1;
  next:
    if (*t) t++;
  }
  return strcpy(s1, s2);
}

/* The following operations return zero on success and -1 on error, with errno
   set appropriately */

/*-------------------------------------------------------------------------*/

/* create a new file */

int create(char *path, mode_t mode)
{
 int file = open(path, O_WRONLY|O_CREAT|O_EXCL, mode);

 if (file == -1 || close(file))
     return -1;

 return 0;
}

/*-------------------------------------------------------------------------*/

/* make a link to a file */

int makeLink(String from, String to)
{
 char todir[MAXPATHLEN], *relPath;
 int i, res;

 strcpy(todir, to);
 for (i = strlen(to)-1; i >= 0 && to[i] != '/'; i--) {};
 todir[i + 1] = 0;
 relPath = relativePath(from, todir);
 res = symlink(relPath, to);
 XTFREE(relPath);
 return res;
}

/*-------------------------------------------------------------------------*/

int movefile(String from, String to)
{
 int res;

 if ((res = rename(from, to)))
 {
     if (errno == EXDEV)
     {
	 rcopy(from, to, True);
	 res = 0;
     }
 }
 return res;
}

/*-------------------------------------------------------------------------*/

/* recursive copy operation */

void rcopy(char *oldpath, char *newpath, Boolean move_flag)
{
 abort_copy = False;
 inodes = NULL;
 n_inodes = 0;
 move = move_flag;
 copy(oldpath, newpath);
}

/*-------------------------------------------------------------------------*/

static void copyfile(char *oldpath, char *newpath)
{
 String err = "Could not mount device for copying from/to";

 if (mountDev(findDev(oldpath), True))
 {
     error(getAnyShell(), err, oldpath);
     return;
 }
 if (mountDev(findDev(newpath), True))
 {
     error(getAnyShell(), err, newpath);
     return;
 }
 if (cpy.first)
 {
     if (cpy.last >= cpy.first)
	 memmove(cpy.copies, &cpy.copies[cpy.first], (cpy.last-cpy.first+1) * sizeof(CopyRec));
     cpy.last -= cpy.first;
     cpy.first = 0;
 }
 if (++(cpy.last) == cpy.n_alc)
     cpy.copies = (CopyRec *) XTREALLOC(cpy.copies, (cpy.n_alc += 100) * sizeof(CopyRec));
 cpy.copies[cpy.last].from = XtNewString(oldpath);
 cpy.copies[cpy.last].to = XtNewString(newpath);
 cpy.copies[cpy.last].move = move;
 cpy.copies[cpy.last].dirToBeRemoved = NULL;
 if (cpy.last == cpy.first)
 {
     cpy.wpID = XtAppAddWorkProc(app_context, (XtWorkProc) copyWorkProc, NULL);
     if (resources.copy_info)  createCopyDialog();
 }
}

/*-------------------------------------------------------------------------*/

static void copydir(struct stat *oldstats, char *oldpath, char *newpath)
{
  DIR *dir = NULL;
  struct dirent *entry;
  int i, n_bg_cp, ol = strlen(oldpath), nl = strlen(newpath);
  struct stat newstats;
  int devto, devfrom, errfl = 0;

  for (i = n_inodes-1; i >= 0; i--)
    if (inodes[i] == oldstats->st_ino) {
      errno = EINVAL;
      errfl = 1;
    }
  if (mountDev(devto = findDev(newpath), False) || mountDev(devfrom = findDev(oldpath), False))
  {
      error(getAnyShell(), "Cannot mount device for copying", oldpath);
      umountDev(devto, False);
      umountDev(devfrom, False);
      return;
  }

  if (!errfl && ((mkdir(newpath, user.umask & 0777) < 0 && errno != EEXIST) || lstat(newpath, &newstats) || !(dir = opendir(oldpath))))
    errfl = 1;

  if (!errfl)
  {
      inodes = (ino_t *) XTREALLOC(inodes, (n_inodes+1)*sizeof(ino_t));
      inodes[n_inodes++] = newstats.st_ino;

      n_bg_cp = cpy.last - cpy.first;

      for (i = 0; (entry = readdir(dir)); i++)
      {
	  if (entry->d_name[0] != '.' || (entry->d_name[1] != '\0' && (entry->d_name[1] != '.' || entry->d_name[2] != '\0')))
	  {
	      int ol1 = ol, nl1 = nl, l = strlen(entry->d_name);
	      char *oldpath1 = (char *)alloca(ol1+l+2), *newpath1 = (char *)alloca(nl1+l+2);

	      strcpy(oldpath1, oldpath);
	      strcpy(newpath1, newpath);
	      if (oldpath1[ol1-1] != '/')
		  oldpath1[ol1++] = '/';
	      if (newpath1[nl1-1] != '/')
		  newpath1[nl1++] = '/';
	      strcpy(oldpath1+ol1, entry->d_name);
	      strcpy(newpath1+nl1, entry->d_name);
	      copy(oldpath1, newpath1);
	      if (abort_copy)  break;
	  }
      }
      if (move && !abort_copy)
      {
	  /* Check if there are regular files which are being copied
	   in the background:*/
	  if (cpy.last - cpy.first > n_bg_cp)
	  {
	      CopyRec *cr = &cpy.copies[cpy.last];

	      if (cr->dirToBeRemoved)  XTFREE(cr->dirToBeRemoved);
	      cr->dirToBeRemoved = XtNewString(oldpath);
	  }
	  /* Otherwise the directory should be empty now: */
	  else if (rmdir(oldpath) && opError(getAnyShell(), "Error removing source directory", oldpath) != YES)
	      abort_copy = True;
      }
  }
  if (dir && closedir(dir))  errfl = 1;
  umountDev(devto, False);
  umountDev(devfrom, False);
  if (errfl)
  {
      if (opError(getAnyShell(), "Error copying directory", oldpath) != YES)
	  abort_copy = True;
  }
}

/*-------------------------------------------------------------------------*/

static void copy(char *oldpath, char *newpath)
{
  struct stat stats;
  int errfl = 0;

  if (lstat(oldpath, &stats))  errfl = 1;

  /* Directory: copy recursively */
  else if (S_ISDIR(stats.st_mode))
      copydir(&stats, oldpath, newpath);

  /* Regular file: copy block by block */
  else if (S_ISREG(stats.st_mode))
      copyfile(oldpath, newpath);

  else
  {
      /* Remove existing target files: Won't work for directories,
       so moxfm doesn't overwrite existing directories
       when copying recursively */
      if (exists(newpath))
	  unlink(newpath);

      /* Fifo: make a new one */
      if (S_ISFIFO(stats.st_mode))
	  errfl = mkfifo(newpath, user.umask & 0666);

      /* Device: make a new one */
      else if (S_ISBLK(stats.st_mode) || S_ISCHR(stats.st_mode) || S_ISSOCK(stats.st_mode))
	  errfl = mknod(newpath, user.umask & 0666, stats.st_rdev);

      /* Symbolic link: make a new one */
      else if (S_ISLNK(stats.st_mode))
      {
	  char lnk[MAXPATHLEN+1];
	  int l = readlink(oldpath, lnk, MAXPATHLEN);

	  if (l<0)  errfl = 1;
	  else
	  {
	      lnk[l] = '\0';
	      errfl = symlink(lnk, newpath);
	  }
      }

      /* This shouldn't happen */
      else
      {
	  errfl = 1;
	  error(getAnyShell(), "Unrecognized file type:", oldpath);
      }

      if (move && !errfl)
      {
	  if (unlink(oldpath) && opError(getAnyShell(), "Error removing source file", oldpath) != YES)
	      abort_copy = True;
      }

  }
  if (errfl)
  {
      if (opError(getAnyShell(), "Error copying file", oldpath) != YES)
	  abort_copy = True;
  }
}

/*-------------------------------------------------------------------------*/

Boolean copyWorkProc(XtPointer dummy)
{
 CopyRec *file;
 struct stat stats;
 static char buffer[BUFSIZ];
 size_t n;
 int i, errfl = 0;
 Boolean ret = False;

 if (cpy.src == -1 || cpy.dest == -1)
 {
     if (cpy.first && copyInfo.dialog)  updateCopyDialog();
     file = &cpy.copies[cpy.first];
     if ((cpy.src = open(file->from, O_RDONLY)) == -1 || stat(file->from, &stats))  errfl = 1;
     else
     {
	 fcntl(cpy.src, F_SETFD, 1);
	 if (exists(file->to))  unlink(file->to);
	 if ((cpy.dest = creat(file->to, stats.st_mode)) == -1)  errfl = 1;
	 else  fcntl(cpy.dest, F_SETFD, 1);
     }
 }
 if (!errfl)
 {
     for (i=0; i<NBLOCKS; i++)
     {
	 if (!(n = read(cpy.src, buffer, BUFSIZ)))  break;
	 if (n == -1 || write(cpy.dest, buffer, n) != n)
	 {
	     errfl = 1;
	     break;
	 }
     }
 }
 if (errfl || !n)
 {
     file = &cpy.copies[cpy.first];
     if (cpy.src != -1)
	 if (close(cpy.src))  errfl = 1;
     if (cpy.dest != -1)
	 if (close(cpy.dest))  errfl = 1;
     cpy.src = cpy.dest = -1;
     if (file->move && !errfl)
     {
	 if (unlink(file->from))
	     error(getAnyShell(), "Cannot remove file", file->from);
	 else
	 {
	     markForUpdate(file->from, CHECK_FILES);
	     if (file->dirToBeRemoved)
	     {
		 rmDirs(file->dirToBeRemoved);
		 markForUpdate(file->dirToBeRemoved, CHECK_FILES);
		 XTFREE(file->dirToBeRemoved);
	     }
	 }
     }
     if (++(cpy.first) > cpy.last)
     {
	 ret = True;
	 intUpdate(CHECK_FILES);
     }
     if (errfl)
     {
	 ret = True;
	 copyError(file->from, file->to);
	 unlink(file->to);
     }
     XTFREE(file->from);
     XTFREE(file->to);
 }
 if (ret)
 {
     if (!errfl)
	 for (i=0; i < mntable.n_dev; i++)
	     umountDev(i, True);
     if (copyInfo.dialog)
     {
	 XtDestroyWidget(copyInfo.dialog);
	 copyInfo.dialog = NULL;
     }
 }
 return ret;
}

/*-------------------------------------------------------------------------*/

void copyErrorProc(XtPointer ptr, int conf)
{
 CopyRec *file;
 int i;

 if (conf == YES && cpy.first <= cpy.last)
 {
     cpy.wpID = XtAppAddWorkProc(app_context, (XtWorkProc) copyWorkProc, NULL);
     if (resources.copy_info)  createCopyDialog();
 }
 else
 {
     for (i = cpy.first; i <= cpy.last; i++)
     {
	 XTFREE((file = &cpy.copies[i])->from);
	 XTFREE(file->to);
	 if (file->dirToBeRemoved)  XTFREE(file->dirToBeRemoved);
     }
     cpy.first = 0;
     cpy.last = -1;
     for (i=0; i < mntable.n_dev; i++)
	 umountDev(i, True);
 }
}
