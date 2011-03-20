/*---------------------------------------------------------------------------
  Module FmAwActions
  
  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

  Action procedures for widgets application windows
---------------------------------------------------------------------------*/

#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include "Am.h"
#include "Fm.h"

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

int findAppItem(AppWindowRec *aw, Widget button)
{
 int i;

 for (i=0; i<aw->n_apps; i++)
 {
     if (aw->apps[i].toggle == button)  return i;
 }
 return -1;
}

/*---------------------------------------------------------------------------*/

int findAppItemByName(AppWindowRec *aw, String name)
{
 int i;

 for (i=0; i<aw->n_apps; i++)
 {
     if (aw->apps[i].name == name)  return i;
 }
 return -1;
}

/*---------------------------------------------------------------------------*/

AppWindowRec *findAppWidget(Widget w, int *i)
{
 AppWindowRec *aw;
 int j;

 for (aw = app_windows; (aw); aw = aw->next)
 {
     for (j=0; j<aw->n_apps; j++)
     {
	 if (aw->apps[j].toggle == w)
	 {
	     *i = j;
	     return aw;
	 }
     }
 }
 return NULL;
}

/*---------------------------------------------------------------------------*/

AppWindowRec *findAppWidgetByForm(Widget form)
{
 AppWindowRec *aw;

 for (aw = app_windows; (aw); aw = aw->next)
 {
     if (aw->form == form)  return aw;
 }
 return NULL;
}

/*---------------------------------------------------------------------------*/

void openApp(Widget w, Boolean newAppWin)
{
 AppWindowRec *aw;
 AppRec *app;
 DTIconRec *dticon;
 Widget shell;
 int i, l;
 String argv[1], push_action;
 int n_args;
 Boolean blocked;
 char directory[MAXPATHLEN];

 if ((aw = findAppWidget(w, &i)))
 {
     app = &aw->apps[i];
     shell = aw->shell;
     blocked = aw->blocked;
 }
 else if ((dticon = findIcon(w)))
 {
     app = &dticon->app;
     shell = XtParent(app->form);
     blocked = False;
 }
 else  return;
 strcpy(directory, app->directory);
 if (*directory)  fnexpand(directory);
 else  strcpy(directory, user.home);

 if (app->objType == APPLICATION)
     push_action = app->push_action;
 else
     push_action = getPushAction(app->objType);

 if (app->push_action[0])
 {
     if (!(strcmp(push_action, "EDIT"))) doEdit(directory, app->fname);
     else if (!(strcmp(push_action, "VIEW"))) doView(directory, app->fname);
     else if (!(strcmp(push_action, "OPEN")))
     {
        int l = strlen(directory);
	if (directory[l-1] != '/')  directory[l++] = '/';
	strcpy(directory+l, app->fname);
	zzz();
	newFileWindow(directory, NULL, NULL);
	wakeUp();
     }
     else if (!(strcmp(push_action, "LOAD")))
     {
	 String newpath;

	 if (blocked)  return;
	 zzz();
	 l = strlen(directory);
	 if (app->fname[0] == '/' || app->fname[0] == '~')  strcpy(directory, app->fname);
	 else
	 {
	     if (directory[l-1] != '/')  directory[l++] = '/';
	     strcpy(directory+l, app->fname);
	 }
	 fnexpand(directory);
	 newpath = XtNewString(directory);
	 if (!newAppWin)
	 {
	     change_app_path_struct *data = (change_app_path_struct*) XtMalloc(sizeof (change_app_path_struct));

	     data->newpath = newpath;
	     data->aw = aw;
	     if (!resources.auto_save && aw->modified) saveDialog(shell, (XtPointer) data);
	     else if (aw->modified)  changeAppProc(data, YES);
	     else changeAppProc(data, NO);
	 }
	 else
	 {
	     newApplicationWindow(newpath, NULL);
	     XTFREE(newpath);
	     wakeUp();
	 }
     }
     else
     {
	 if (app->fname[0])
	 {
	     n_args = 1;
	     argv[0] = XtNewString(app->fname);
	 }
	 else
	     n_args = 0;
	 performAction(shell, app->icon_pm.bm, push_action, directory, n_args, argv);
     }
 }
 else error(shell, "No push action defined for", app->name);
}

/*---------------------------------------------------------------------------*/

void runApp(Widget w, XtPointer client_data, XtPointer call_data)
{
 openApp(w, False);
}


