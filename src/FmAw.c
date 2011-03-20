/*-----------------------------------------------------------------------------
  FmAw.c
  
  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

  Functions & data for creating & maintaining  application windows
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <time.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <Xm/ScrolledW.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/LabelG.h>
#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>

#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
#include <X11/Xmu/Editres.h>
#endif
#endif

#include "Am.h"

/*-----------------------------------------------------------------------------
  PUBLIC DATA 
-----------------------------------------------------------------------------*/

AppWindowList app_windows = NULL;

XPropRec winInfo = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*-----------------------------------------------------------------------------
  STATIC DATA 
-----------------------------------------------------------------------------*/

static MenuItemRec button_popup_menu[] = {
    { "Open", &xmPushButtonGadgetClass, 0, NULL, NULL, appOpenCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Edit...", &xmPushButtonGadgetClass, 0, NULL, NULL, appEditCb, NULL, NULL, NULL },
    { "Delete", &xmPushButtonGadgetClass, 0, NULL, NULL, appRemoveCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Put on desktop", &xmPushButtonGadgetClass, 0, NULL, NULL, appOnDeskCb, NULL, NULL, NULL },
    { NULL, NULL }
 };

static MenuItemRec appbox_popup_menu[] = {
    { "Install application...", &xmPushButtonGadgetClass, 0, NULL, NULL, appInstallCb, NULL, NULL, NULL },
    { "New application group...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>N", "Ctrl+N", appboxNewCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Save setup", &xmPushButtonGadgetClass, 0, "Ctrl<Key>S", "Ctrl+S", appSaveCb, NULL, NULL, NULL },
    { "Reload setup", &xmPushButtonGadgetClass, 0, NULL, NULL, appLoadCb, NULL, NULL, NULL },
    { "Edit setup file", &xmPushButtonGadgetClass, 0, NULL, NULL, appEditCfgCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Windows", &xmCascadeButtonGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Close window", &xmPushButtonGadgetClass, 0, "Ctrl<Key>W", "Ctrl+W", closeApplicationCB, NULL, NULL, NULL },
    { "Exit", &xmPushButtonGadgetClass, 0, "Ctrl<Key>Q", "Ctrl+Q", appExitCb, NULL, NULL },
    { NULL, NULL }
};

static MenuItemRec window_cscd_menu[] = {
    { "Main appl window", &xmPushButtonGadgetClass, 0, NULL, NULL, newAppWinCb, NULL, NULL, NULL },
    { "Open file window", &xmPushButtonGadgetClass, 0, "Ctrl<Key>F", "Ctrl+F", appOpenFileWinCb, NULL, NULL, NULL },
    { "Mount table window", &xmPushButtonGadgetClass, 0, NULL, NULL, mountTableCb, NULL, NULL, NULL },
    { "Raise desktop icons", &xmPushButtonGadgetClass, 0, "Ctrl<Key>I", "Ctrl+I", raiseIconsCb, NULL, NULL, NULL },
    { "Save windows", &xmPushButtonGadgetClass, 0, NULL, NULL, saveStartupCb, NULL, NULL, NULL },
    { "Save desktop icons", &xmPushButtonGadgetClass, 0, NULL, NULL, saveIconsCb, NULL, NULL, NULL },
    { NULL, NULL }
};

static char app_file_header[] = "#XFM\n\
# xfm applications file\n\
# written by moxfm  %s\
#\n\
##########################################\n\
\n";

/*-----------------------------------------------------------------------------
  Widget Argument lists
-----------------------------------------------------------------------------*/

static Arg icon_toggle_args[] = {
    { XmNlabelType, XmPIXMAP },
    { XmNtopAttachment, XmATTACH_FORM },
    { XmNleftAttachment, XmATTACH_FORM },
    { XmNrightAttachment, XmATTACH_FORM },
    { XmNlabelPixmap, 0 },
    { XmNheight, 0 },
    { XmNbackground, 0 },
    { XmNtranslations, 0 },
    { XmNhighlightThickness, 1 },
    { XmNhighlightColor, 0 },
    { XmNforeground, 0 }
};

static Arg icon_label_args[] = {
    { XmNleftAttachment, XmATTACH_FORM },
    { XmNrightAttachment, XmATTACH_FORM },
    { XmNbottomAttachment, XmATTACH_FORM },
    { XmNtopAttachment, XmATTACH_WIDGET },
    { XmNtopWidget, 0 },
    { XmNlabelString, 0 },
    { XmNfontList, 0 }
};

static Arg drop_reg_args[] = {
    { XmNimportTargets, 0 },
    { XmNnumImportTargets, 0 },
    { XmNdropSiteOperations, XmDROP_COPY | XmDROP_MOVE | XmDROP_LINK },
    { XmNdropSiteType, XmDROP_SITE_COMPOSITE },
    { XmNdropProc, (XtArgVal) handleAppDrop }
};

static Arg push_reg_args[] = {
    { XmNimportTargets, 0 },
    { XmNnumImportTargets, 2 },
    { XmNdropSiteOperations, XmDROP_COPY | XmDROP_MOVE | XmDROP_LINK },
    { XmNanimationStyle, XmDRAG_UNDER_SHADOW_IN },
    { XmNdropProc, (XtArgVal) handlePushDrop }
};

/*-----------------------------------------------------------------------------
  Translation tables
-----------------------------------------------------------------------------*/

static char app_translations[] = "\
#override    <Btn2Down> : startAppDrag()\n";

static char form_translations[] = "\
#override    <Btn3Down>	: buttonPopup()\n";

/*-----------------------------------------------------------------------------
  Action tables
-----------------------------------------------------------------------------*/

static XtActionsRec app_actions[] = {
    { "startAppDrag",	startAppDrag },
    { "buttonPopup",	buttonPopup },
};

/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static int longestName(AppWindowRec *aw)
{
  int i, l, longest = 0;
  XmString name;

  for (i=0; i<aw->n_apps; i++)
  {
      name = XmStringCreateLocalized(aw->apps[i].name);
      if ((l = XmStringWidth((XmFontList) resources.icon_font, name)) > longest)
	  longest = l;
      XmStringFree(name);
  }
  return longest;
}

/*-------------------------------------------------------------------------*/

int parseApp(FILE *fp, char **name, char **directory, char **fname, char **icon, char **push_action, char **drop_action)
{
  static char s[MAXAPPSTRINGLEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXAPPSTRINGLEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*name = split(s, ':')))
    return -1;
  if (!(*directory = split(NULL, ':')))
    return -1;
  if (!(*fname = split(NULL, ':')))
    return -1;
  if (!(*icon = split(NULL, ':')))
    return -1;
  if (!(*push_action = split(NULL, ':')))
    return -1;
  if (!(*drop_action = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

/* determine the default icon of an application */

IconRec defaultIcon(char *name, char *directory, char *fname)
{
  if (*fname)
  {
    struct stat stats;
    Boolean sym_link;
    int l;
    char path[MAXPATHLEN];
    
    strcpy(path, *directory?directory:user.home);
    fnexpand(path);
    l = strlen(path);

    if (l) {
      if (path[l-1] != '/')
	path[l++] = '/';
      strcpy(path+l, fname);
    } else
      strcpy(path, fname);

    if (lstat(path, &stats))
      return icons[FILE_BM];
    else if (S_ISLNK(stats.st_mode)) {
      sym_link = True;
      stat(path, &stats);
    } else
      sym_link = False;

    if (S_ISLNK(stats.st_mode))
      return icons[BLACKHOLE_BM];
    else if (S_ISDIR(stats.st_mode))
      if (sym_link)
	return icons[DIRLNK_BM];
      else if (!strcmp(name, ".."))
	return icons[UPDIR_BM];
      else
	return icons[DIR_BM];
    else if (stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
      if (sym_link)
	return icons[EXECLNK_BM];
      else
	return icons[EXEC_BM];
    else if (sym_link)
      return icons[SYMLNK_BM];
    else
      return icons[FILE_BM];
  }
 return icons[EXEC_BM];
}

/*---------------------------------------------------------------------------*/

void setTitle(AppWindowRec *aw)
{
 String title;
 int i;

 for (i = strlen(aw->appfile) - 1; (i >= 0 && aw->appfile[i] != '/'); i--);
 title = (String) XtMalloc((strlen(&aw->appfile[i+1]) + 16) * sizeof(char));
 strcpy(title, "Applications - ");
 strcat(title, &aw->appfile[i+1]);
 XtVaSetValues(aw->shell, XmNtitle, title, XmNiconName, title, NULL);
 XTFREE(title);
}

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

AppWindowRec *createApplicationWindow(String newpath, FILE *fin, GeomRec *geom)
{
 AppWindowRec *aw;
 MenuItemRec appboxPopupMenu[XtNumber(appbox_popup_menu)];
 MenuItemRec buttonPopupMenu[XtNumber(button_popup_menu)];
 MenuItemRec windowCscdMenu[XtNumber(window_cscd_menu)];
 Atom *targets = (Atom *) XtMalloc((usrObjects.nr + 1) * sizeof(Atom));
 Widget cont;
 static Boolean firstcall = True;
 int i;

 aw = (AppWindowRec *) XtMalloc(sizeof(AppWindowRec));
 aw->next = app_windows;
 app_windows = aw;


 if (fin)
 {
     if (readApplicationWindow(aw, geom, fin))
     {
	 app_windows = aw->next;
	 XTFREE(aw);
	 sysError(getAnyShell(), "Error reading startup file:");
	 return NULL;
     }
     aw->shell = XtVaAppCreateShell(NULL, "Moxfm", topLevelShellWidgetClass, dpy, XmNwidth, geom->width, XmNheight, geom->height, XmNiconic, geom->iconic, NULL);
 }
 else
 {
     if (newpath)
     {
	 strcpy(aw->appfile, newpath);
	 fnexpand(aw->appfile);
     }
     else
	 strcpy(aw->appfile, resources.app_mainfile);
     if (!firstcall)
     {
	 Dimension xPos, yPos;

	 xPos = winInfo.appXPos;
	 yPos = winInfo.appYPos;
	 if (xPos < 100)  xPos += winInfo.rootWidth - winInfo.appWidth;
	 if (yPos < 80)  yPos += winInfo.rootHeight - winInfo.appHeight;
	 xPos -= 100;
	 yPos -= 80;
	 aw->shell = XtVaAppCreateShell(NULL, "Moxfm", topLevelShellWidgetClass, dpy, XmNx, xPos, XmNy, yPos, XmNgeometry, resources.app_geometry, NULL);
     }
     else
     {
	 firstcall = False;
	 aw->shell = XtVaAppCreateShell(NULL, "Moxfm", topLevelShellWidgetClass, dpy, XmNgeometry, resources.init_app_geometry, NULL);
     }
 }

 setTitle(aw);
 XtVaSetValues(aw->shell, XmNiconPixmap, icons[APPBOX_BM].bm, XmNiconMask, icons[APPBOX_BM].mask, XmNdeleteResponse, XmDO_NOTHING, XmNallowShellResize, False, NULL);

 /* Add new actions */
 XtAppAddActions(app_context, app_actions, XtNumber(app_actions));

 icon_label_args[6].value = (XtArgVal) resources.icon_font;
 aw->iconBoxCreated = False;
 aw->iconic = True;

 /* This widget seems to be necessary in order to prevent segfaults in connection with
  popup menus under Linux w/ MetroLink Motif 1.2.4 */
 cont = XtVaCreateManagedWidget("container", xmFormWidgetClass, aw->shell, XmNbackground, winInfo.background, NULL);

 /* create the form */
 aw->form = XtVaCreateManagedWidget("form", xmScrolledWindowWidgetClass, cont,  XmNscrollingPolicy, XmAUTOMATIC, XmNbackground, winInfo.background, XmNspacing, 0, XmNtranslations, XtParseTranslationTable (form_translations), XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);

 targets[0] = dragAtoms.appl;
 for (i=0; i < usrObjects.nr; i++)
     targets[i+1] = usrObjects.obj[i].atom;
 drop_reg_args[0].value = (XtArgVal) targets;
 drop_reg_args[1].value = usrObjects.nr + 1;
 XmDropSiteRegister(aw->form, drop_reg_args, XtNumber(drop_reg_args));
 XTFREE(targets);

 memcpy(appboxPopupMenu, appbox_popup_menu, XtNumber(appbox_popup_menu) * sizeof(MenuItemRec));
 for (i=0; i<XtNumber(appbox_popup_menu); i++)
     appboxPopupMenu[i].callback_data = aw;
 appboxPopupMenu[7].subitems = windowCscdMenu;

 memcpy(buttonPopupMenu, button_popup_menu, XtNumber(button_popup_menu) * sizeof(MenuItemRec));
 for (i=0; i<XtNumber(button_popup_menu); i++)
     buttonPopupMenu[i].callback_data = aw;

 memcpy(windowCscdMenu, window_cscd_menu, XtNumber(window_cscd_menu) * sizeof(MenuItemRec));
 for (i=0; i<XtNumber(window_cscd_menu); i++)
     windowCscdMenu[i].callback_data = aw;

 aw->buttonPopup = BuildMenu(aw->form, XmMENU_POPUP, "Button Actions", 0, False, buttonPopupMenu);

 aw->appboxPopup = BuildMenu(aw->form, XmMENU_POPUP, "Application Box Actions", 0, False, appboxPopupMenu);

 if (!mntable.n_dev)
     XtVaSetValues(windowCscdMenu[2].object, XmNsensitive, False, NULL);

 return aw;
}

/*---------------------------------------------------------------------------*/

int saveApplicationWindow(AppWindowRec *aw, FILE *fout)
{
 size_t size;

 size = strlen(aw->appfile);
 if (1 != fwrite(&size, sizeof(size_t), 1, fout))
     return 1;
 if (size != fwrite(aw->appfile, sizeof(char), size, fout))
     return 1;
 return writeGeometry(aw->shell, aw->iconic, fout);
}

/*---------------------------------------------------------------------------*/

int readApplicationWindow(AppWindowRec *aw, GeomRec *geom, FILE *fin)
{
 size_t size;

 if (1 != fread(&size, sizeof(size_t), 1, fin))
     return 1;
 if (size != fread(aw->appfile, sizeof(char), size, fin))
     return 1;
 aw->appfile[size] = 0;
 return readGeometry(geom, fin);
}

/*---------------------------------------------------------------------------*/

void createApplicationDisplay(AppWindowRec *aw)
{
 int i;
 Dimension buttonwidth;
 XmString label;
 XtTranslations trans;
 AppRec *app;
 Atom targets[2];

 targets[0] = dragAtoms.file;
 targets[1] = dragAtoms.filelist;
 push_reg_args[0].value = (XtArgVal) targets;

 aw->icon_box = XtVaCreateWidget("icon box", xmRowColumnWidgetClass, aw->form, XmNpacking, XmPACK_COLUMN, XmNorientation, XmHORIZONTAL, XmNbackground, winInfo.background, XmNforeground, resources.label_color, NULL);
 aw->iconBoxCreated = True;

 if (aw->n_apps == 0)
    XtVaCreateManagedWidget("No configured applications", xmLabelGadgetClass, aw->icon_box, XmNfontList, resources.label_font, NULL);
 else
 {
     icon_toggle_args[5].value = (XtArgVal) resources.app_icon_height;
     icon_toggle_args[6].value = winInfo.background;
     icon_toggle_args[10].value = resources.label_color;
     icon_toggle_args[9].value = resources.select_color;
     buttonwidth = setApplGeom(aw);
     trans = XtParseTranslationTable(app_translations);
     icon_toggle_args[7].value = (XtArgVal) trans;
     for (i=0; i < aw->n_apps; i++)
     {
	 app = &aw->apps[i];
	 app->form = XtVaCreateManagedWidget("button form", xmFormWidgetClass, aw->icon_box, XmNbackground, winInfo.background, XmNforeground, resources.label_color, XmNwidth, buttonwidth, NULL);

	 icon_toggle_args[4].value = app->icon_pm.bm;
	 app->toggle = XtCreateManagedWidget("icon", xmPushButtonWidgetClass, app->form, icon_toggle_args, XtNumber(icon_toggle_args) );

	 XtAddCallback(app->toggle, XmNactivateCallback, (XtCallbackProc) runApp, NULL);

	 if (app->drop_action[0])
	     XmDropSiteRegister(app->toggle, push_reg_args, XtNumber(push_reg_args));

	 icon_label_args[4].value = (XtArgVal) app->toggle;
	 label = XmStringCreateLocalized(app->name);
	 icon_label_args[5].value = (XtArgVal) label;
	 app->label = XtCreateManagedWidget("label", xmLabelGadgetClass, app->form, icon_label_args, XtNumber(icon_label_args) );
	 XmStringFree(label);
     }
 }
 XtManageChild(aw->icon_box);
}

/*---------------------------------------------------------------------------*/

Dimension setApplGeom(AppWindowRec *aw)
{
 Dimension width, itemwidth, newwidth, scrollbarwidth;
 Widget vsb;
 int ncols, nrows;

 XtVaGetValues(aw->form, XmNwidth, &width, XmNverticalScrollBar, &vsb, NULL);
 XtVaGetValues(vsb, XmNwidth, &scrollbarwidth, NULL);

 width -= scrollbarwidth + 17;
 itemwidth = longestName(aw);
 if (itemwidth < resources.app_icon_width)
     itemwidth = resources.app_icon_width;
 ncols = width / itemwidth;
 if (!ncols)  ncols = 1;
 nrows = (aw->n_apps-1)/ncols + 1;
 XtVaSetValues(aw->icon_box, XmNnumColumns, nrows, NULL);
 ncols = (aw->n_apps-1)/nrows + 1;
 newwidth =  width / ncols - 2;
 if (newwidth > itemwidth)  itemwidth = newwidth;
 return itemwidth;
}

/*---------------------------------------------------------------------------*/

void resizeAppl(Widget form, XtPointer client_data, XEvent *event)
{
 AppWindowRec *aw = (AppWindowRec *) client_data;

 aw->update = True;
 updateApplicationDisplay(aw);
}

/*---------------------------------------------------------------------------*/

void appIconifyHandler(Widget shell, XtPointer client_data, XEvent *event)
{
 AppWindowRec *aw = (AppWindowRec *) client_data;

 if (event->type == MapNotify)
     aw->iconic = False;
 else if (event->type == UnmapNotify)
     aw->iconic = True;
}

/*---------------------------------------------------------------------------*/

AppWindowRec *newApplicationWindow(String newpath, FILE *fin)
{
 AppWindowRec *aw;
 GeomRec geom;

 if (!(aw = createApplicationWindow(newpath, fin, &geom)))
     return NULL;
#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
 XtAddEventHandler(aw->shell, (EventMask)0, True, _XEditResCheckMessages,
		   NULL);
#endif
#endif
 readApplicationData(aw);
 XtAddEventHandler(aw->shell, StructureNotifyMask, True, (XtEventHandler) appIconifyHandler, aw);
 XtAddEventHandler(aw->form, StructureNotifyMask, True, (XtEventHandler) resizeAppl, aw);
 XmAddWMProtocolCallback(aw->shell, wm_delete_window, closeApplicationCB, aw);
 XtRealizeWidget(aw->shell);
 if (fin)
     XtVaSetValues(aw->shell, XmNx, geom.x, XmNy, geom.y, XmNiconic, geom.iconic, NULL);
 else
     XtVaGetValues(aw->shell, XmNx, &winInfo.appXPos, XmNy, &winInfo.appYPos, NULL);
 return aw;
}

/*---------------------------------------------------------------------------*/

void removeProc(XtPointer awp, int conf)
{
 AppSpecRec *appl = (AppSpecRec *) awp;
 AppWindowRec *aw = appl->win.aw;
 int i = appl->item;
 size_t size;

 aw->blocked = False;
 if (conf == YES)
 {
     if (aw->apps[i].objType != APPLICATION)
     {
	 if (!chdir(aw->apps[i].directory))
	 {
	     unlink(aw->apps[i].fname);
	     chdir(user.home);
	     aw->apps[i].remove_file = False;
	 }
     }
     freeApplicationResources(&aw->apps[i]);
     if ((size = (aw->n_apps -i-1) * sizeof(AppRec)))
	 memmove(&aw->apps[i], &aw->apps[i+1], size);
     aw->n_apps--;
     aw->apps = (AppRec *) XTREALLOC(aw->apps, aw->n_apps * sizeof(AppRec));
     aw->modified = True;
     aw->update = True;

     /* Replaced the asynchronous invokations of updateApplicationDisplay()
	through XtAppAddWorkProc() by direct calls to make sure that
	updateApplicationDisplay() is called before
	writeApplicationData(). The goal is to prevent the application window
	to be in an unstable state (pixmap already destroyed etc.) before
	writeApplication() is called which may pop up a dialog window
	obscuring the application window. (I don't think there is a great
	performance penalty on this anyway.) -AG */

     /*
     XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, aw);
     */
     updateApplicationDisplay(aw);
     if (resources.auto_save)
	 writeApplicationData(aw);
 }
 XTFREE(awp);
}

/*---------------------------------------------------------------------------*/

void closeProc(XtPointer awp, int save)
{
 AppWindowRec *awi, *aw = (AppWindowRec *) awp;
 int j;

 if (save == CANCEL)  return;
 if (save == YES)  writeApplicationData(aw);
 XtDestroyWidget(aw->shell);
 for (j=0; j<aw->n_apps; j++) freeApplicationResources(&aw->apps[j]);
 XTFREE(aw->apps);
 if (aw == app_windows)  app_windows = aw->next;
 else
 {
     for (awi=app_windows; (awi); awi=awi->next)
     {
	 if (awi->next == aw)
	 {
	     awi->next = aw->next;
	     break;
	 }
     }
 }
 XTFREE(aw);
}

void exitProc(XtPointer xp, int answer)
{
 AppWindowRec *aw;

 if (answer == CANCEL)  return;
 /* Check if any button has been modified: */
 if (resources.auto_save)  quit(NULL, YES);
 else
 {
     for (aw = app_windows; aw; aw = aw->next)
     {
	 if (aw->modified)
	 {
	     lastSaveDialog(aw->shell);
	     break;
	 }
     }
     if (!aw)  quit(NULL, NO);
 }
}

void changeIconProc(XtPointer awi, int conf)
{
 AppWindowRec *aw = ((AppSpecRec *)awi)->win.aw;
 int i = ((AppSpecRec *)awi)->item;

 XTFREE(awi);
 aw->blocked = False;
 if (conf == YES)
 {
     if (aw->apps[i].loaded)  freeIcon(aw->apps[i].icon_pm);
     aw->modified = True;
     aw->apps[i].objType = getObjectType(aw->apps[i].push_action);
     readApplicationIcon(aw->shell, &aw->apps[i], ERR_POPUP);
     aw->update = True;
     /*
     XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, aw);
     */
     updateApplicationDisplay(aw);
     if (resources.auto_save)
	 writeApplicationData(aw);
 }
}

/*---------------------------------------------------------------------------*/

void newAppProc(XtPointer nar, int conf)
{
 NewAppRec *napr = (NewAppRec *) nar;
 AppList *applist;

 napr->aw->blocked = False;
 if (conf == YES)
 {
     applist = (AppList *) XtMalloc(sizeof(AppList));
     *applist = napr->app;
     applist[0]->objType = getObjectType(applist[0]->push_action);
     insertNewApp(napr->aw, napr->aw->n_apps, applist, 1);
 }
 else
 {
     freeApplicationResources(napr->app);
     XTFREE(napr->app);
 }
 XTFREE(napr);
}

void newAppBoxProc(XtPointer apr, int conf)
{
 NewAppBoxRec *napr = (NewAppBoxRec *) apr;
 AppWindowRec *aw = napr->aw;
 String name  = napr->name;
 AppRec *app;
 time_t t;
 FILE *fp;
 String filename, fname;
 int i, len;
 Boolean ok = False;

 aw->blocked = False;
 freeIcon(napr->icon);
 XTFREE(napr);
 if (conf == YES)
 {
     if ((len = strlen(name)))
     {
	 fname = (char *) XtMalloc((len + 1) * sizeof(char));
	 strcpy(fname, name);
	 for (i=0; i<len; i++)
	 {
	     if (fname[i] == ' ' || fname[i] == '/' || fname[i] == '\\' || fname[i] == '*' || fname[i] == '?')  fname[i] = '_';
	 }
	 filename = (char *) XtMalloc( (strlen(resources.appcfg_path) + len + 5) * sizeof(char));
	 strcpy(filename, resources.appcfg_path);
	 strcat(filename, fname);
	 if ((fp = fopen(filename, "r")))  ok = True;
	 else
	 {
	     if (!(fp = fopen(filename, "w")))
	     {
		 ok = False;
		 error(aw->shell, "Could not open application configuration file for either read or write:", filename);
	     }
	     else
	     {
		 time(&t);
		 fprintf(fp, app_file_header, ctime(&t));
		 fprintf(fp, "[..]::%s:%s:LOAD:\n", aw->appfile,
			 resources.applbox_icon);
		 ok = True;
	     }
	 }
	 if (ok)
	 {
	     fclose(fp);
	     aw->apps = (AppList) XTREALLOC(aw->apps, (++aw->n_apps) * sizeof(AppRec));
	     app = &aw->apps[aw->n_apps-1];
	     app->name = name;
	     app->directory = XtNewString(resources.appcfg_path);
	     app->fname = fname;
	     app->icon = XtNewString(resources.applbox_icon);
	     app->push_action = XtNewString("LOAD");
	     app->drop_action = XtNewString("");
	     app->objType = APPLICATION;
	     app->remove_file = False;
	     aw->modified = True;
	     aw->update = True;
	     readApplicationIcon(aw->shell, app, ERR_POPUP);
	     /*
	     XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, aw);
	     */
	     updateApplicationDisplay(aw);
	     if (resources.auto_save)
		 writeApplicationData(aw);
	 }
	 else
	 {
	     XTFREE(fname); XTFREE(name);
	 }
	 XTFREE(filename);
     }
     else XTFREE(name);
 }
}

void changeAppProc(XtPointer chs, int save)
{
 AppWindowRec *aw = ((change_app_path_struct *) chs)->aw;
 String path = ((change_app_path_struct *) chs)->newpath;
 int j;

 if (save == YES)  writeApplicationData(aw);
 if (save != CANCEL)
 {
     for (j=0; j<aw->n_apps; j++) freeApplicationResources(&aw->apps[j]);
     XTFREE(aw->apps);
     strcpy(aw->appfile, path);
     readApplicationData(aw);
     /*
     XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, aw);
     */
     updateApplicationDisplay(aw);
     setTitle(aw);
 }
 XTFREE(path);
 XTFREE(chs);
 wakeUp();
}

Boolean updateApplicationDisplay(AppWindowRec *aw)
{
  if (!aw->update)  return True;
  if (aw->iconBoxCreated)  XtDestroyWidget(aw->icon_box);
  createApplicationDisplay(aw);
  aw->update = False;
  return True;
}

/*---------------------------------------------------------------------------*/

void appUpdate(void)
{
    AppWindowRec *aw;

    updateIconDisplay();
    for (aw = app_windows; aw; aw = aw->next) {
	struct stat cur;
	Boolean reread = False;
	if (aw->drag_source || aw->blocked)
	    continue;
	if (aw->readable)
	    reread = stat(aw->appfile, &cur) || cur.st_ctime >
		aw->stats.st_ctime || cur.st_mtime > aw->stats.st_mtime;
	else
	    reread = !stat(aw->appfile, &cur);
	if (reread) {
	    int j;
	    zzz();
	    for (j=0; j<aw->n_apps; j++)
		freeApplicationResources(&aw->apps[j]);
	    XTFREE(aw->apps);
	    readApplicationData(aw);
	    aw->update = True;
	    updateApplicationDisplay(aw);
	    wakeUp();
	}
    }
}

/*---------------------------------------------------------------------------*/

void readApplicationIcon(Widget shell, AppRec *app, int errDispType)
{
 app->loaded = False;
 if (!app->icon[0])
     app->icon_pm = defaultIcon(app->name, app->directory, app->fname);
 else if ((app->icon_pm = readIcon(shell, app->icon)).bm == None)
 {
     if (errDispType == ERR_POPUP)
	 error(shell, "Could not read icon for", app->name);
     else if (!resources.suppress_warnings)
	 fprintf(stderr, "%s: can't read icon for application %s\n", progname, app->name);
     app->icon_pm = defaultIcon(app->name, app->directory, app->fname);
 }
 else  app->loaded = True;
}

/*---------------------------------------------------------------------------*/

void readApplicationData(AppWindowRec *aw)
{
  FILE *fp;
  Widget shell;
  char *name, *directory, *fname, *icon, *push_action, *drop_action;
  char s[MAXAPPSTRINGLEN];
  int i, p;
  
  aw->n_apps = 0;
  aw->apps = NULL;
  aw->modified = False;
  aw->blocked = False;
  aw->drag_source = False;
  aw->update = True;

  if (XtIsRealized(aw->shell))
      shell = aw->shell;
  else
      shell = getAnyShell();
  
  if (stat(aw->appfile, &aw->stats) || !(fp = fopen(aw->appfile, "r")))
  {
      aw->readable = False;
      error(shell, "Cannot read application file", aw->appfile);
      return;
  }
  aw->readable = True;

  for (i=0; (p = parseApp(fp, &name, &directory, &fname, &icon, &push_action, &drop_action)) > 0; i++) {
    aw->apps = (AppList) XTREALLOC(aw->apps, (i+1)*sizeof(AppRec) );
    aw->apps[i].name = XtNewString(strparse(s, name, "\\:"));
    aw->apps[i].directory = XtNewString(strparse(s, directory, "\\:"));
    aw->apps[i].fname = XtNewString(strparse(s, fname, "\\:"));
    aw->apps[i].icon = XtNewString(strparse(s, icon, "\\:"));
    aw->apps[i].push_action = XtNewString(strparse(s, push_action, "\\:"));
    aw->apps[i].drop_action = XtNewString(strparse(s, drop_action, "\\:"));
    aw->apps[i].objType = getObjectType(aw->apps[i].push_action);
    aw->apps[i].remove_file = False;
    readApplicationIcon(shell, &aw->apps[i], STDERR);
  }

  if (p == -1)
      error(shell, "Error in applications file", NULL);

  aw->n_apps = i;
  
  if (fclose(fp))
      sysError(shell, "Error reading applications file:");
}

/*---------------------------------------------------------------------------*/

void overwriteApplicationData(AppWindowRec *aw)
{
 AppWindowRec *awi;
 FILE *fp;
 int i;
 time_t t;
 char name[2*MAXAPPSTRINGLEN], directory[2*MAXAPPSTRINGLEN], fname[2*MAXAPPSTRINGLEN], icon[2*MAXAPPSTRINGLEN], push_action[2*MAXAPPSTRINGLEN], drop_action[2*MAXAPPSTRINGLEN];
  
 if (!(fp = fopen(aw->appfile, "w")))
 {
     sysError(aw->shell, "Error writing applications file:");
     return;
 }

 time(&t);
 fprintf(fp, app_file_header, ctime(&t));

 for (i=0; i < aw->n_apps; i++)
 {
     expand(name, aw->apps[i].name, "\\:");
     expand(directory, aw->apps[i].directory, "\\:");
     expand(fname, aw->apps[i].fname, "\\:");
     expand(icon, aw->apps[i].icon, "\\:");
     expand(push_action, aw->apps[i].push_action, "\\:");
     expand(drop_action, aw->apps[i].drop_action, "\\:");
     fprintf(fp, "%s:%s:%s:%s:%s:%s\n", name, directory, fname, icon, push_action, drop_action);
     aw->apps[i].remove_file = False;
 }
  
 if (fclose(fp))
 {
     sysError(aw->shell, "Error writing applications file:");
     return;
 }

 aw->modified = False;
 aw->readable = !stat(aw->appfile, &aw->stats);

 for (awi=app_windows; (awi); awi=awi->next)
 {
     if (!(awi->blocked) && (awi != aw) && !(strcmp(awi->appfile, aw->appfile)))
     {
	 for (i=0; i<awi->n_apps; i++)
	     freeApplicationResources(&awi->apps[i]);
	 XTFREE(awi->apps);
	 readApplicationData(awi);
         /*
	 XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, awi);
	 */
	 updateApplicationDisplay(awi);
     }
 }
}

/*-----------------------------------------------------------------------------*/

void writeApplicationDataProc(XtPointer data, int conf)
{
    AppWindowRec *aw = (AppWindowRec*) data;
    aw->blocked = False;
    if (conf == YES)
	overwriteApplicationData(aw);
}

/*---------------------------------------------------------------------------*/

void writeApplicationData(AppWindowRec *aw)
{
    struct stat cur;
    if (!stat(aw->appfile, &cur) &&
	(!aw->readable || cur.st_ctime > aw->stats.st_ctime ||
	 cur.st_mtime > aw->stats.st_mtime)) {
	aw->blocked = True;
	overwriteAppsDialog(aw);
    } else
	overwriteApplicationData(aw);
}

/*---------------------------------------------------------------------------*/

void freeApplicationResources(AppRec *app)
{
  if (app->loaded)  freeIcon(app->icon_pm);
  if (app->remove_file && app->objType != APPLICATION && app->fname)
  {
      if (app->directory)
	  chdir(app->directory);
      else
	  chdir(user.home);
      unlink(app->fname);
      chdir(user.home);
  }
  XTFREE(app->name);
  XTFREE(app->directory);
  XTFREE(app->fname);
  XTFREE(app->icon);
  XTFREE(app->push_action);
  XTFREE(app->drop_action);
}

/*---------------------------------------------------------------------------*/
void insertNewApp(AppWindowRec *aw, int i, AppList *applist, int n_apps)
{
 char dir[MAXPATHLEN];
 AppRec *app;
 size_t size;
 int j;

 size = (aw->n_apps += n_apps) * sizeof(AppRec);
 aw->apps = (AppList) XTREALLOC(aw->apps, size);
 if (i >= aw->n_apps - n_apps)
     i = aw->n_apps - n_apps;
 else
 {
     size = (aw->n_apps - n_apps - i) * sizeof(AppRec);
     memmove(&aw->apps[i+n_apps], &aw->apps[i], size);
 }
 for (j=0; j < n_apps; j++)
 {
     app = applist[j];
     strcpy(dir, app->directory);
     fnexpand(dir);
     XTFREE(app->directory);
     app->directory = XtNewString(dir);
     memcpy(&(aw->apps[i+j]), app, sizeof(AppRec));
     readApplicationIcon(aw->shell, &aw->apps[i+j], ERR_POPUP);
     XTFREE(app);
 }
 aw->modified = True;
 aw->update = True;
 /*
 XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, aw);
 */
 updateApplicationDisplay(aw);
 if (resources.auto_save)
     writeApplicationData(aw);
 XTFREE(applist);
}
