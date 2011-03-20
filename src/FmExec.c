/*---------------------------------------------------------------------------
  Module FmExec

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995

  Procedures for executing files
---------------------------------------------------------------------------*/


#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>

#include <X11/Intrinsic.h>

#include "Am.h"
#include "Fm.h"

ExecMapRec *exec_map = NULL;
int n_exec_maps = 0;

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

char **makeArgv(char *action, int n_args, String *arguments)
{
  char **argv;
  int i = 0, j;

  argv = (char **) XtMalloc( (n_args + (user.arg0flag ? 5 : 4)) * sizeof(char *));

  argv[i++] = XtNewString(user.shell);
  argv[i++] = XtNewString("-c");
  if (resources.start_from_xterm)
  {
      while (*action == ' ')  action++;
      if (resources.keep_xterm && !strncmp(action, "exec ", 5))
	  action += 5;
      argv[i++] = (String) XtMalloc((strlen(resources.xterm) + 2 * strlen(user.shell) + strlen(action) + 12) * sizeof(char));
      if (resources.keep_xterm)
	  sprintf(argv[2], "%s -e %s -c \"%s;%s\"", resources.xterm, user.shell, action, user.shell);
      else
	  sprintf(argv[2], "%s -e %s -c \"%s\"", resources.xterm, user.shell, action);
  }
  else
      argv[i++] = XtNewString(action);
  if (user.arg0flag)
    argv[i++] = XtNewString(user.shell);
  for (j=0; j<n_args; j++)
      argv[i++] = arguments[j];
  argv[i] = NULL;

  return argv;
}

/*---------------------------------------------------------------------------*/

void freeArgv(char **argv)
{
  int j;

  for (j=0; argv[j]; j++)  XTFREE(argv[j]);
  XTFREE(argv);
}

/*---------------------------------------------------------------------------*/

static void echoarg(char *arg)
{
  char *s;
  for (s = arg; *s; s++)
    if (isspace(*s)) {
      fprintf(stderr, " '%s'", arg);
      return;
    }
  fprintf(stderr, " %s", arg);
}

/*---------------------------------------------------------------------------*/

void executeApplication(char *path, char *directory, char **argv)
{
  int pid;

  zzz();
  XFlush(dpy);
  if (chdir(directory))
  {
      wakeUp();
      sysError(getAnyShell(), "Can't chdir:");
  }
  else if ((pid = fork()) == -1)
  {
      wakeUp();
      sysError(getAnyShell(), "Can't fork:");
  }
  else
  {
    if (!pid) {
      if (resources.echo_actions) {
	char **arg;
	fprintf(stderr, "[%s] %s", directory, path);
	for (arg = argv+1; *arg; arg++)  echoarg(*arg);
	fprintf(stderr, "\n");
      }
      /* Make sure that child processes don't lock up xfm with keyboard
	 input. This is certainly a kludge and if you know of any better
	 way to do this, please let me know. -ag */
      freopen("/dev/null", "r", stdin);
      execvp(path, argv);
      perror("Exec failed");
      exit(1);
    } else {
      sleep(1);
      chdir(user.home);
      wakeUp();
    }
  }    
}

/*---------------------------------------------------------------------------*/

void doEdit(char *directory, char *fname)
{
  char path[MAXPATHLEN];
  char **argv;

  if (resources.default_editor)
  {
      strcpy(path, resources.default_editor);
      strcat(path, " ");
      strcat(path, fname);

      argv = (char **) XtMalloc(4 * sizeof(char *));
      argv[0] = user.shell;
      argv[1] = "-c";
      argv[2] = path;
      argv[3] = NULL;

      executeApplication(user.shell, directory, argv);
  
      XTFREE(argv);
  }
  else  error(getAnyShell(), "No default editor defined", NULL);
}

/*---------------------------------------------------------------------------*/

void doView(char *directory, char *fname)
{
  char path[MAXPATHLEN];
  char **argv;

  if (resources.default_viewer) {

    strcpy(path, resources.default_viewer);
    strcat(path, " ");
    strcat(path, fname);

    argv = (char **) XtMalloc(4 * sizeof(char *));
    argv[0] = user.shell;
    argv[1] = "-c";
    argv[2] = path;
    argv[3] = NULL;

    executeApplication(user.shell, directory, argv);
  
    XTFREE(argv);
  }

  else
    error(getAnyShell(), "No default viewer defined", NULL);
}
