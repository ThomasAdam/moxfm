/*-----------------------------------------------------------------------------
  Module FmMonitor.c                                                             

  (c) Albert Graef 1996
                                                                           
  functions & data for monitoring changing application default files
 (modified by O. Mai)
                                                  
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "Am.h"
#include "Fm.h"

/* Symbolic constants for events and action types. */

#define MON_EVENT_ANY        0
#define MON_EVENT_DELETE     1
#define MON_EVENT_CREATE     2
#define MON_EVENT_EMPTY      3
#define MON_EVENT_NONEMPTY   4
#define MON_EVENT_EQ         5
#define MON_EVENT_NE         6
#define MON_EVENT_LT         7
#define MON_EVENT_LE         8
#define MON_EVENT_GT         9
#define MON_EVENT_GE         10

#define MON_TYPE_ANY         0
#define MON_TYPE_STARTUP     1
#define MON_TYPE_UPDATE      2

typedef struct _MonActRec {
    char *fname, *action;
    int event, type;
    Boolean readable;
    struct stat stats;
} MonActRec, *MonActList;

int n_monacts = 0;
static MonActList monact;
static Boolean mon_init = True;

/* This one reads in the moxfmmon file which specifies the events which
   should be monitored during automatic updates. Each line contains the
   following:
       filename:event:type:action
   where filename specifies the name of the file or directory to be
   monitored; event specifies the event upon which to react (DELETE, CREATE,
   EMPTY, NONEMPTY, EQ, NE, LT, LE, GT, GE; an empty entry means "any
   change"); type can either be STARTUP, UPDATE or empty (in which case the
   entry is considered both at startup time and during later updates); and
   action may be EDIT, VIEW, OPEN, LOAD or any shell command. */

static int parseMonAct(FILE *fp, char **fname, char **event, char **type,
		       char **action)
{
  static char s[MAXCFGSTRINGLEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXCFGSTRINGLEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*fname = split(s, ':')))
    return -1;
  if (!(*event = split(NULL, ':')))
    return -1;
  if (!(*type = split(NULL, ':')))
    return -1;
  if (!(*action = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

void initMonitor(void)
{
    FILE *fp;
    char *fname, *event, *type, *action;
    int i, p;

    mon_init = True;
    /* this file is optional, so don't bother if we can't open it. */
    if (!(fp = fopen(resources.mon_file, "r")))
	return;
    for (i=0; (p = parseMonAct(fp, &fname, &event, &type, &action)) > 0;
	      i++) {
	char s[MAXPATHLEN], msg[180];
	monact = (MonActList) XTREALLOC(monact, (i+1) * sizeof(MonActRec));
	/* Make sure we have an absolute pathname: */
	if (fname[0] != '/' && (fname[0] != '~' || fname[1] != '/'))
	{
	    if (strcmp(fname, "MAILPATH"))
		strcat(strcpy(s, "~/"), fname);
	    else
	    {
		char *mailp = getenv("MAILPATH");

		if (mailp && mailp[0])
		    strcpy(s, mailp);
		else
		    sprintf(s, "%s/%s", MAILDIR, user.name);
	    }
	}
	else
	    strcpy(s, fname);
	monact[i].fname = XtNewString(fnexpand(s));
	monact[i].event = MON_EVENT_ANY;
	if (!*event)
	    ;
	else if (!strcmp(event, "DELETE"))
	    monact[i].event = MON_EVENT_DELETE;
	else if (!strcmp(event, "CREATE"))
	    monact[i].event = MON_EVENT_CREATE;
	else if (!strcmp(event, "EMPTY"))
	    monact[i].event = MON_EVENT_EMPTY;
	else if (!strcmp(event, "NONEMPTY"))
	    monact[i].event = MON_EVENT_NONEMPTY;
	else if (!strcmp(event, "EQ"))
	    monact[i].event = MON_EVENT_EQ;
	else if (!strcmp(event, "NE"))
	    monact[i].event = MON_EVENT_NE;
	else if (!strcmp(event, "LT"))
	    monact[i].event = MON_EVENT_LT;
	else if (!strcmp(event, "LE"))
	    monact[i].event = MON_EVENT_LE;
	else if (!strcmp(event, "GT"))
	    monact[i].event = MON_EVENT_GT;
	else if (!strcmp(event, "GE"))
	    monact[i].event = MON_EVENT_GE;
	else {
	    sprintf(msg, "Unrecognized event in monitor entry #%d", i+1);
	    error(getAnyShell(), msg, NULL);
	}
	monact[i].type = MON_TYPE_ANY;
	if (!*type)
	    ;
	else if (!strcmp(type, "STARTUP"))
	    monact[i].type = MON_TYPE_STARTUP;
	else if (!strcmp(type, "UPDATE"))
	    monact[i].type = MON_TYPE_UPDATE;
	else {
	    sprintf(msg, "Unrecognized action type in monitor entry #%d", i+1);
	    error(getAnyShell(), msg, NULL);
	}
	monact[i].action = XtNewString(action);
    }

    if (p == -1)
	error(getAnyShell(), "Error in monitor file", NULL);

    n_monacts = i;

    if (fclose(fp))
	sysError(getAnyShell(), "Error reading monitor file:");

}

/*---------------------------------------------------------------------------*/

void freeMonacts(void)
{
    int i;

    for (i=0; i<n_monacts; i++) {
	XTFREE(monact[i].fname);
	XTFREE(monact[i].action);
    }
    XTFREE(monact);
    monact = NULL;
    n_monacts = 0;
}

/*---------------------------------------------------------------------------*/

/* Reload the monitor setup. */

void reinitMonitor(void)
{
    if (n_monacts)  freeMonacts();
    initMonitor();
}

/* This one scans the monitor action lists, and performs the appropriate
   actions for files and directories whose status has changed since the
   last invokation. It also invokes EMPTY, NONEMPTY, and "any event" entries
   during initialization. */

static void ExecMonAction(char *fname, char *action)
{
     if (!(strcmp(action, "EDIT"))) doEdit(user.home, fname);
     else if (!(strcmp(action, "VIEW"))) doView(user.home, fname);
     else if (!(strcmp(action, "OPEN"))) {
	 zzz();
	 newFileWindow(fname, NULL, NULL);
	 wakeUp();
     } else if (!(strcmp(action, "LOAD"))) {
	 zzz();
	 newApplicationWindow(fname, NULL);
	 wakeUp();
     } else if (fname[0]) {
	 int pid;
	 chdir(user.home);
	 if ((pid = fork()) == -1)
	     sysError(topShell, "Can't fork:");
	 else if (!pid) {
	     if (resources.echo_actions)
		 fprintf(stderr, "%s\n", action);
	     freopen("/dev/null", "r", stdin);
	     if (user.arg0flag)
		 execlp(user.shell, user.shell, "-c", action, user.shell,
			NULL);
	     else
		 execlp(user.shell, user.shell, "-c", action, NULL);
	     perror("Exec failed");
	     exit(1);
	 } else
	     waitpid(pid, NULL, 0);
     }
}

/*---------------------------------------------------------------------------*/

static Boolean dirIsEmpty(char *dirname)
{
    DIR *dir;
    struct dirent *entry;
    if (!(dir = opendir(dirname)))
	return True;
    while ((entry = readdir(dir)) &&
	   (!strcmp(entry->d_name, ".") ||
	    !strcmp(entry->d_name, "..")))
	;
    closedir(dir);
    return (!entry);
}

/*---------------------------------------------------------------------------*/

void doMonitor(void)
{
    int i;
    struct stat cur;
    Boolean readable, isDir;

    if (chdir(user.home)) return;

    for (i=0; i<n_monacts; i++) {
	readable = !stat(monact[i].fname, &cur);
	isDir = readable?S_ISDIR(cur.st_mode):False;
	if (mon_init) {
	    if (monact[i].type == MON_TYPE_ANY ||
		monact[i].type == MON_TYPE_STARTUP) {
		switch (monact[i].event) {
		case MON_EVENT_ANY:
		    ExecMonAction(monact[i].fname, monact[i].action);
		    break;
		case MON_EVENT_EMPTY:
		    if (!readable)
			break;
		    /* determine whether file has zero size, or whether
		       directory has zero proper entries */
		    if (isDir) {
			if (!dirIsEmpty(monact[i].fname))
			    break;
		    } else if (cur.st_size > 0)
			break;
		    ExecMonAction(monact[i].fname, monact[i].action);
		    break;
		case MON_EVENT_NONEMPTY:
		    if (!readable)
			break;
		    if (isDir) {
			if (dirIsEmpty(monact[i].fname))
			    break;
		    } else if (cur.st_size == 0)
			break;
		    ExecMonAction(monact[i].fname, monact[i].action);
		    break;
		}
	    }
	} else if ((monact[i].type == MON_TYPE_ANY ||
		    monact[i].type == MON_TYPE_UPDATE) &&
		   (readable != monact[i].readable || (
		    readable && monact[i].readable &&
		    (cur.st_ctime > monact[i].stats.st_ctime ||
		     cur.st_mtime > monact[i].stats.st_mtime)))) {
	    switch (monact[i].event) {
	    case MON_EVENT_ANY:
		ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_DELETE:
		if (!readable && monact[i].readable)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_CREATE:
		if (readable && !monact[i].readable)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_EMPTY:
		if (!readable)
		    break;
		if (isDir) {
		    if (!dirIsEmpty(monact[i].fname))
			break;
		} else if (cur.st_size > 0)
		    break;
		ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_NONEMPTY:
		if (!readable)
		    break;
		if (isDir) {
		    if (dirIsEmpty(monact[i].fname))
			break;
		} else if (cur.st_size == 0)
		    break;
		ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_EQ:
		if (readable && monact[i].readable &&
		    cur.st_size == monact[i].stats.st_size)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_NE:
		if (readable && monact[i].readable &&
		    cur.st_size != monact[i].stats.st_size)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_LT:
		if (readable && monact[i].readable &&
		    cur.st_size < monact[i].stats.st_size)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_LE:
		if (readable && monact[i].readable &&
		    cur.st_size <= monact[i].stats.st_size)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_GT:
		if (readable && monact[i].readable &&
		    cur.st_size > monact[i].stats.st_size)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    case MON_EVENT_GE:
		if (readable && monact[i].readable &&
		    cur.st_size >= monact[i].stats.st_size)
		    ExecMonAction(monact[i].fname, monact[i].action);
		break;
	    }
	}
	monact[i].readable = readable;
	memcpy(&monact[i].stats, &cur, sizeof(struct stat));
    }
    mon_init = False;
}

/*---------------------------------------------------------------------------*/

void appTimeoutCb(XtPointer data, XtIntervalId *id)
{
 doMonitor();
 appUpdate();
 XtAppAddTimeOut(app_context, resources.update_interval, appTimeoutCb, NULL);
}
