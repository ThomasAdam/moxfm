/*-----------------------------------------------------------------------------
  Module FmUtils.c

  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

  General utility functions for saving, questions, etc.,
  and functions for desensetising.
-----------------------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <Xm/Xm.h>

#include "Am.h"
#include "Fm.h"

#define kDefaultValueMarker "--" /* Marker to denote default value */

void fillIn(Widget w)
{
  XtVaSetValues(w, XmNsensitive, (XtArgVal) True, NULL);
}

/*---------------------------------------------------------------------------*/

void grayOut(Widget w)
{
  XtVaSetValues(w, XmNsensitive, (XtArgVal) False, NULL);
}

/*---------------------------------------------------------------------------*/

int writeGeometry(Widget shell, Boolean iconic, FILE *fout)
{
 GeomRec geom;

 XtVaGetValues(shell, XmNx, &geom.x, XmNy, &geom.y, XmNwidth, &geom.width, XmNheight, &geom.height, NULL);
 if (1 != fwrite(&geom.x, sizeof(Dimension), 1, fout))
     return 1;
 if (1 != fwrite(&geom.y, sizeof(Dimension), 1, fout))
     return 1;
 if (1 != fwrite(&geom.width, sizeof(Dimension), 1, fout))
     return 1;
 if (1 != fwrite(&geom.height, sizeof(Dimension), 1, fout))
     return 1;
 if (1 != fwrite(&iconic, sizeof(Boolean), 1, fout))
     return 1;
 return 0;
}

/*---------------------------------------------------------------------------*/

int readGeometry(GeomRec *geom, FILE *fin)
{
 if (1 != fread(&geom->x, sizeof(Dimension), 1, fin))
     return 1;
 if (1 != fread(&geom->y, sizeof(Dimension), 1, fin))
     return 1;
 if (1 != fread(&geom->width, sizeof(Dimension), 1, fin))
     return 1;
 if (1 != fread(&geom->height, sizeof(Dimension), 1, fin))
     return 1;
 if (1 != fread(&geom->iconic, sizeof(Boolean), 1, fin))
     return 1;
 return 0;
}

/*---------------------------------------------------------------------------*/

int saveWindows(void)
{
 FILE *fout = fopen(resources.startup_file, "w");
 AppWindowRec *aw;
 FileWindowRec *fw;
 int n_awins = 0, n_fwins = 0, ret = 0;

 if (!fout)  ret = -1;
 else
 {
     for (aw = app_windows; (aw); aw = aw->next)
	 n_awins++;
     if (1 == fwrite(&n_awins, sizeof(int), 1, fout))
     {
	 for (aw = app_windows; (aw); aw = aw->next)
	 {
	     if (saveApplicationWindow(aw, fout))
	     {
		 ret = -2;
		 break;
	     }
	 }
     }
     else  ret = -2;
 }

 for (fw = file_windows; (fw); fw = fw->next)
     n_fwins++;
 if (!ret && 1 == fwrite(&n_fwins, sizeof(int), 1, fout))
 {
      for (fw = file_windows; (fw); fw = fw->next)
      {
	  if (saveFileWindow(fw, fout))
	  {
	      ret = -2;
	      break;
	  }
      }
 }
 if (mntable.n_dev)  saveMountWindow(fout);
 writeDTIcons(True);
 if (fout)  fclose(fout);

 return ret;
}

/*---------------------------------------------------------------------------*/

void saveStartupCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 if (saveWindows())
     error(getAnyShell(), "Error writing startup file", resources.startup_file);
}

/*---------------------------------------------------------------------------*/

void zzz(void)
{
  FileWindowRec *fw;
  AppWindowRec *aw;
  int i;

  for (fw = file_windows; fw; fw = fw->next)
    XDefineCursor(dpy, XtWindow(fw->shell), cur);

  for (aw = app_windows; aw; aw=aw->next)
      XDefineCursor(dpy, XtWindow(aw->shell), cur);

  for (i=0; i<n_dtIcons; i++)
      XDefineCursor(dpy, XtWindow(dtIcons[i]->app.form), cur);

  XFlush(dpy);
}

/*---------------------------------------------------------------------------*/

void wakeUp(void)
{
  FileWindowRec *fw;
  AppWindowRec *aw;
  int i;

  for (fw = file_windows; fw; fw = fw->next)
    XUndefineCursor(dpy, XtWindow(fw->shell));

  for (aw = app_windows; aw; aw=aw->next)
      XUndefineCursor(dpy, XtWindow(aw->shell));

  for (i=0; i<n_dtIcons; i++)
      XUndefineCursor(dpy, XtWindow(dtIcons[i]->app.form));
}

/*---------------------------------------------------------------------------*/

#define MAXVARSTRINGLEN MAXPATHLEN

typedef struct
{
    String action;
    String *args;
    int n_vars;
    String *acts;
    int nacts;
    String *static_act;
    String directory;
    String act1;
    int n_args;
    String *arguments;
} ArgActionRec;

/*---------------------------------------------------------------------------*/

void performAction(Widget shell, Pixmap icon_bm, String action, String directory, int n_args, String *arguments)
/* *arguments must be allocated on heap. They will be free'ed */
{
  static char *act = NULL;
  char *act1 = XtMalloc(strlen(action)+1), *s, *t;
  char *str = (char *)alloca(strlen(action)+1);
  char **acts = NULL, **vars = NULL;
  char **argv;
  char *def_val;
  char **vals = NULL;
  int n_acts = 0, n_vars = 0, i;
  size_t size;

  if (act) XTFREE(act);
  act = NULL;
  strcpy(act1, action);

  for (s = split(act1, '%'); s; s = split(NULL, '%'))
  {
      acts = (char **)XTREALLOC(acts, (n_acts+1)*sizeof(char *));
      acts[n_acts++] = XtNewString(strparse(str, s, "\\%"));
      if ((t = split(NULL, '%')))
      {
	  vars = (char **)XTREALLOC(vars, (n_vars+1) * sizeof(char*));
	  vars[n_vars] = XtNewString(strparse(str, t, "\\%"));
	  /* Check string for default value character */
	  vals = (char **)XTREALLOC(vals, (n_vars+1)*sizeof(char *));
	  if ((def_val = strstr(vars[n_vars], kDefaultValueMarker)) == NULL)
	      vals[n_vars++] = NULL;
	  else
	  {
	      def_val[0] = '\0'; /* Separate label and default value */
	      vals[n_vars++] = XtNewString(&def_val[strlen(kDefaultValueMarker)]);
	  }
      }
      else  break;
  }

  if (n_vars)
  {
      TextFieldRec *editLines;
      ArgActionRec *command = (ArgActionRec *) XtMalloc(sizeof(ArgActionRec));

      editLines = (TextFieldRec *) XtMalloc((n_vars+1) * sizeof(TextFieldRec));
      command->args = vals;
      command->n_vars = n_vars;
      command->action = action;
      command->acts = acts;
      command->nacts = n_acts;
      command->static_act = &act;
      command->n_args = n_args;
      if (n_args)
      {
	  size = n_args * sizeof(String);
	  command->arguments = (String *) XtMalloc(size);
	  memcpy(command->arguments, arguments, size);
      }
      command->directory = XtNewString(directory);
      command->act1 = act1;
      for (i=0; i<n_vars; i++)
      {
	  editLines[i].label = vars[i];
	  editLines[i].value = &command->args[i];
      }
      editLines[n_vars].label = NULL;
      textFieldDialog(shell, editLines, execArgProc, (XtPointer) command, icon_bm);
      for (i=0; i<n_vars; i++)  XTFREE(vars[i]);
      XTFREE(vars);
      XTFREE(editLines);
  }
  else
  {
      for (i=0; i < n_acts; i++)  XTFREE(acts[i]);
      if (n_acts) XTFREE(acts);
      XTFREE(act1);
      argv = makeArgv(action, n_args, arguments);
      executeApplication(user.shell, directory, argv);
      freeArgv(argv);
  }
}

/*---------------------------------------------------------------------------*/

void execArgProc(XtPointer pcom, int conf)
{
 ArgActionRec *command = (ArgActionRec *) pcom;
 int i, l, l1, l2;
 String *act = command->static_act;
 String *argv;

 if (conf == YES)
 {
     for (l = i = 0; i < command->nacts; i++)
     {
	l1 = strlen(command->acts[i]);
	l2 = (i < command->n_vars)? strlen(command->args[i]) :0;
	*act = (String) XTREALLOC(*act, l+l1+l2+1);
	strcpy((*act)+l, command->acts[i]);
	if (l2) strcpy((*act)+l+l1, command->args[i]);
	l += l1+l2;
      }
     argv = makeArgv(*act, command->n_args, command->arguments);
     executeApplication(user.shell, command->directory, argv);
     freeArgv(argv);
 }
 XTFREE(command->args);
 for (i=0; i < command->nacts; i++)  XTFREE(command->acts[i]);
 if (command->nacts)  XTFREE(command->acts);
 XTFREE(command->directory);
 if (command->n_args)  XTFREE(command->arguments);
 XTFREE(command->act1);
 XTFREE(command);
}

/*---------------------------------------------------------------------------*/

Boolean lastWindow(void)
{
 int n=0;
 Boolean retval;
 if (app_windows)
 {
     n++;
     if (app_windows->next) n++;
 }
 if (file_windows)
 {
     n++;
     if (file_windows->next) n++;
 }
 if (n == 1)  retval = True;
 else retval = False;
 return retval;
}

/*---------------------------------------------------------------------------*/

Widget getAnyShell(void)
{
 FileWindowRec *fw = file_windows;
 AppWindowRec *aw = app_windows;

 while (fw)
 {
     if (XtIsRealized(fw->shell))
	 return fw->shell;
     fw = fw->next;
 }
 while (aw)
 {
     if (XtIsRealized(aw->shell))
	 return aw->shell;
     aw = aw->next;
 }
 return topShell;
}

/*---------------------------------------------------------------------------*/

int percent(long part, long whole)
{
 unsigned long denom;
 int res;

 if (!whole)  return 100;
 if (part < 0 && whole > 0)  return 0;
 denom = (unsigned long) part * (unsigned long) 100 + (unsigned long) (whole / 2);
 res = (int) (denom / (unsigned long) whole);
 return res;
}

/*---------------------------------------------------------------------------*/
#ifndef HAVE_REALPATH

char *resolvepath(const char *path, char *resolved_path)
{
 char *dir;

 if (chdir(path))
     return NULL;
 dir = getcwd(resolved_path, MAXPATHLEN);
 chdir(user.home);
 return dir;
}

#endif	/* HAVE_REALPATH */

#ifdef NO_MEMMOVE

/*******************************************************************/
/*   Copy the characters at 's2' to 's1'.  Make sure the copy      */
/*   is done correctly even when the two objects overlap.          */
/*******************************************************************/
/*    Contributed by Nick Thompson: uknt@brwlserv.cnet.att.com */

#include <stddef.h>
#include <string.h>

void *memmove(void *s1, const void *s2, size_t n)
{
 register char *ptr1 = (char *) s1;
 register char *ptr2 = (char *) s2;
 register size_t length = n;

 /* If the objects overlap be carefull about copy direction */
 if ((ptr2 < ptr1)  &&  ((ptr2 + length) > ptr1))
 {
     ptr1 = ptr1 + length;	/* Copy by hand, starting at the back */
     ptr2 = ptr2 + length;
     for (++length; --length;)
	 *(--ptr1) = *(--ptr2);
 }
 else  memcpy (s1, s2, n);	/* memcpy copies from front */
 return(s1);
}

#endif	/* NO_MEMMOVE */
