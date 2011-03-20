/*---------------------------------------------------------------------------
  Module FmAwCb

  (c) S.Marlow 1990-92
  (c) A.Graef 1994
  (c) O.Mai 1995

  Callback routines for widgets in application windows
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <memory.h>

#include <X11/Intrinsic.h>
#include <Xm/RowColumn.h>

#include "Am.h"
#include "Fm.h"

/*-----------------------------------------------------------------------------
  STATIC DATA 
-----------------------------------------------------------------------------*/

TextFieldRec AppEditLines[] = {
    { "Label", NULL },
    { "Directory", NULL },
    { "File name", NULL },
    { "Icon", NULL },
    { "Push action", NULL },
    { "Drop action", NULL },
    { NULL }
};

TextFieldRec ObjEditLines[] = {
    { "Label", NULL },
    { "Icon", NULL },
    { NULL }
};

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/


void appOpenCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 Widget toggle;
 AppWindowRec *aw = (AppWindowRec *) client_data;

 XtVaGetValues(aw->form, XmNuserData, (XtPointer) &toggle, NULL);
 openApp(toggle, True);
}

/*---------------------------------------------------------------------------*/

void appEditCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 Widget button;
 AppWindowRec *aw = (AppWindowRec *) client_data;
 AppSpecRec *awi = (AppSpecRec *) XtMalloc(sizeof(AppSpecRec));

 XtVaGetValues(aw->form, XmNuserData, (XtPointer) &button, NULL);
 aw->blocked = True;
 awi->win.aw = aw;
 awi->item = findAppItem(aw, button);
 if (aw->apps[awi->item].objType == APPLICATION)
     appEdit(awi);
 else  usrObjEdit(awi);
}

/*---------------------------------------------------------------------------*/

void appEdit(AppSpecRec *awi)
{
 TextFieldRec editLines[XtNumber(AppEditLines)];
 AppRec *app;
 Widget shell;
 void (*callback)(XtPointer, int);

 if (awi->item == DTICON)
 {
     app = &awi->win.dticon->app;
     callback = changeDTIconProc;
     shell = awi->win.dticon->shell;
 }
 else
 {
     app = &awi->win.aw->apps[awi->item];
     callback = changeIconProc;
     shell = awi->win.aw->shell;
 }
 memcpy(editLines, AppEditLines, XtNumber(AppEditLines) * sizeof(TextFieldRec));
 editLines[0].value = &app->name;
 editLines[1].value = &app->directory;
 editLines[2].value = &app->fname;
 editLines[3].value = &app->icon;
 editLines[4].value = &app->push_action;
 editLines[5].value = &app->drop_action;
 textFieldDialog(shell, editLines, callback, (XtPointer) awi, app->icon_pm.bm);
}

/*---------------------------------------------------------------------------*/

void usrObjEdit(AppSpecRec *awi)
{
 TextFieldRec editLines[XtNumber(ObjEditLines)];
 AppRec *app;
 Widget shell;
 void (*callback)(XtPointer, int);

 if (awi->item == DTICON)
 {
     app = &awi->win.dticon->app;
     callback = changeDTIconProc;
     shell = awi->win.dticon->shell;
 }
 else
 {
     app = &awi->win.aw->apps[awi->item];
     callback = changeIconProc;
     shell = awi->win.aw->shell;
 }
 memcpy(editLines, ObjEditLines, XtNumber(ObjEditLines) * sizeof(TextFieldRec));
 editLines[0].value = &app->name;
 editLines[1].value = &app->icon;
 textFieldDialog(shell, editLines, callback, (XtPointer) awi, app->icon_pm.bm);
}

/*---------------------------------------------------------------------------*/

void appRemoveCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 Widget button;
 AppWindowRec *aw = (AppWindowRec *) client_data;

 XtVaGetValues(aw->form, XmNuserData, (XtPointer) &button, NULL);
 aw->blocked = True;
 removeAppDialog(aw, findAppItem(aw, button));
}

/*---------------------------------------------------------------------------*/

void appInstallCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppWindowRec *aw = (AppWindowRec *) client_data;
 TextFieldRec editLines[XtNumber(AppEditLines)];
 NewAppRec *nar = (NewAppRec *) XtMalloc(sizeof(NewAppRec));
 AppRec *app = (AppRec *) XtMalloc(sizeof(AppRec));

 aw->blocked = True;
 memcpy(editLines, AppEditLines, XtNumber(AppEditLines) * sizeof(TextFieldRec));
 nar->aw = aw;
 nar->app = app;
 app->loaded = False;
 app->remove_file = False;
 *(editLines[0].value = &app->name) = NULL;
 *(editLines[1].value = &app->directory) = NULL;
 *(editLines[2].value = &app->fname) = NULL;
 *(editLines[3].value = &app->icon) = NULL;
 *(editLines[4].value = &app->push_action) = NULL;
 *(editLines[5].value = &app->drop_action) = NULL;
 textFieldDialog(aw->shell, editLines, newAppProc, (XtPointer) nar, None);
}

/*---------------------------------------------------------------------------*/

void appboxNewCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 TextFieldRec editLines[] = {
     { "Application box name", NULL },
     { NULL }
 };
 NewAppBoxRec *apr = (NewAppBoxRec *) XtMalloc(sizeof(NewAppBoxRec));

 apr->aw = (AppWindowRec *)client_data;
 apr->aw->blocked = True;
 apr->icon = readIcon(apr->aw->shell, resources.applbox_icon);
 *(editLines[0].value = &apr->name) = NULL;
 textFieldDialog(apr->aw->shell, editLines, newAppBoxProc, (XtPointer) apr, apr->icon.bm);
}

/*---------------------------------------------------------------------------*/
void appEditCfgCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 doEdit(".", ((AppWindowRec *) client_data)->appfile);
}

/*---------------------------------------------------------------------------*/

void appSaveCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 writeApplicationData((AppWindowRec *) client_data);
}

/*---------------------------------------------------------------------------*/

void appLoadCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppWindowRec *aw = (AppWindowRec *) client_data;
 int j;

 for (j=0; j<aw->n_apps; j++) freeApplicationResources(&aw->apps[j]);
 XTFREE(aw->apps);
 readApplicationData(aw);
 updateApplicationDisplay(aw);
}

/*---------------------------------------------------------------------------*/

void appOpenFileWinCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 zzz();
 newFileWindow(user.home, NULL, NULL);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void raiseIconsCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 int i;

 for (i=0; i < n_dtIcons; i++)
 {
     XMapRaised(dpy, XtWindow(dtIcons[i]->shell));
 }
}

/*---------------------------------------------------------------------------*/

void saveIconsCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 writeDTIcons(True);
}

/*---------------------------------------------------------------------------*/

void appExitCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 exitDialog(((AppWindowRec *) client_data)->shell);
}
