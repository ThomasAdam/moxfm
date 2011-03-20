/*---------------------------------------------------------------------------
  Module FmIcons

  (c) Oliver Mai 1995, 1996
  (c) Albert Graef 1996
  
  Functions & data for handling desktop icons.
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/extensions/shape.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/PushBG.h>
#include <Xm/Label.h>
#include <Xm/SeparatoG.h>
#include <Xm/DragDrop.h>
#include <Xm/MwmUtil.h>

#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
#include <X11/Xmu/Editres.h>
#endif
#endif

#include "Am.h"
#include "Fm.h"

#define MARGIN 8
#define DTICONWIDTH 80
#define DTICONHEIGHT 80

/*  RBW - 2001/08/15  */
void CatchEntryLeave2(Widget,XtPointer,XEvent*,Boolean *ctd);

static void moveIconCb(Widget w, XEvent *event, String *params, Cardinal *num_params);
static void openIconCb(Widget w, XEvent *event, String *params, Cardinal *num_params);
static void iconOpenCb(Widget w, XtPointer client_data, XtPointer call_data);
static void iconFileEditCb(Widget w, XtPointer client_data, XtPointer call_data);
static void iconFileViewCb(Widget w, XtPointer client_data, XtPointer call_data);
static void iconSaveCb(Widget w, XtPointer client_data, XtPointer call_data);
static void iconPopup(Widget w, XEvent *event, String *params, Cardinal *num_params);
static int parseIcon(FILE *fp, char **name, char **directory, char **fname, char **icon, char **push_action, char **drop_action, char **xp, char **yp);
static void checkVertPos(DTIconRec *dticon, int *x, int *y);
static void checkHorPos(DTIconRec *dticon, int *x, int *y);
static Boolean tryPosition(DTIconRec *dticon);
static void iconEditCb(Widget w, XtPointer client_data, XtPointer call_data);
static void iconRemoveCb(Widget w, XtPointer client_data, XtPointer call_data);
static DTIconRec *createDTIcon(Widget appshell, AppRec *app, int x, int y);
static void displayDTIcon(DTIconRec *dticon);
static void placeDTIcon(DTIconRec *dticon);

/* Implemented in FmDragDrop.c: */
AppList *applistFromFiles(FileWindowRec *fw);


DTIconRec **dtIcons = NULL;
int n_dtIcons = 0;
static XtTranslations trans;
static int x0 = -1, y0;
static Boolean dticons_readable, dticons_blocked;
static struct stat dticons_stats;

/*-----------------------------------------------------------------------------
  STATIC DATA 
-----------------------------------------------------------------------------*/

static MenuItemRec popup_menu[] = {
    { "Open", &xmPushButtonGadgetClass, 0, NULL, NULL, iconOpenCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Edit...", &xmPushButtonGadgetClass, 0, NULL, NULL, iconEditCb, NULL, NULL, NULL },
    { "Save icon", &xmPushButtonGadgetClass, 0, NULL, NULL, iconSaveCb, NULL, NULL, NULL },
    { "Remove from desktop", &xmPushButtonGadgetClass, 0, NULL, NULL, iconRemoveCb, NULL, NULL, NULL },
    { NULL, NULL}
};

static MenuItemRec file_popup_menu[] = {
    { "Open", &xmPushButtonGadgetClass, 0, NULL, NULL, iconOpenCb, NULL, NULL, NULL },
    { "Edit file", &xmPushButtonGadgetClass, 0, NULL, NULL, iconFileEditCb, NULL, NULL, NULL },
    { "View file", &xmPushButtonGadgetClass, 0, NULL, NULL, iconFileViewCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Edit...", &xmPushButtonGadgetClass, 0, NULL, NULL, iconEditCb, NULL, NULL, NULL },
    { "Save icon", &xmPushButtonGadgetClass, 0, NULL, NULL, iconSaveCb, NULL, NULL, NULL },
    { "Remove from desktop", &xmPushButtonGadgetClass, 0, NULL, NULL, iconRemoveCb, NULL, NULL, NULL },
    { NULL, NULL}
};

static Arg drop_args[] = {
    { XmNimportTargets, 0 },
    { XmNnumImportTargets, 2 },
    { XmNdropSiteOperations, XmDROP_COPY | XmDROP_MOVE | XmDROP_LINK },
    { XmNanimationStyle, XmDRAG_UNDER_PIXMAP },
    { XmNanimationPixmap, 0 },
    { XmNdropProc, (XtArgVal) handlePushDrop }
};

static char icon_file_header[] = "#XFM\n\
# moxfm desktop icons file\n\
# written by moxfm  %s\
#\n\
##########################################\n\
\n";

/*-----------------------------------------------------------------------------
  Translation table
-----------------------------------------------------------------------------*/

static char icon_translations[] = "\
#override   <Btn1Down>   : moveIconCb()\n\
            <Btn1Motion> : moveIconCb()\n\
            <Btn1Up>(2)  : openIconCb()\n\
            <Btn1Up>     : moveIconCb()\n\
            <Btn2Down> : startDTIconDrag()\n";

static char form_translations[] = "\
#override   <Btn3Down>   : iconPopup()\n";

/*-----------------------------------------------------------------------------
  Action table
-----------------------------------------------------------------------------*/

static XtActionsRec icon_actions[] = {
    { "moveIconCb",	moveIconCb },
    { "openIconCb",	openIconCb },
    { "iconPopup",	iconPopup },
    { "startDTIconDrag",startDTIconDrag }
};

/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static void moveIconCb(Widget w, XEvent *event, String *params,
		       Cardinal *num_params)
{
 Window root, child;
 static DTIconRec *dticon;
 static int x_offset, y_offset;
 int x, y;
 unsigned int mask;

 if (event->xany.type == ButtonPress)
 {
     if (!(dticon = findIcon(w)))
	 return;
     dticons_blocked = True;
     XQueryPointer(dpy, XtWindow(dticon->shell), &root, &child, &x, &y,
		   &x_offset, &y_offset, &mask);
 }
 else if (event->xany.type == ButtonRelease)
 {
     if (dticon->moved)
     {
	 if (resources.auto_save)
	     writeDTIcons(True);
	 dticon->moved = False;
	 dticons_blocked = False;
     }
 }
 else
 {
     dticon->x = event->xbutton.x_root - x_offset;
     dticon->y = event->xbutton.y_root - y_offset;
     dticon->moved = True;
     XtVaSetValues(dticon->shell,
		   XmNx, (Dimension) dticon->x,
		   XmNy, (Dimension) dticon->y, NULL);
 }
}

/*-----------------------------------------------------------------------------*/

static void openIconCb(Widget w, XEvent *event, String *params,
		       Cardinal *num_params)
{
     dticons_blocked = False;
     openApp(w, True);
}

/*-----------------------------------------------------------------------------*/

static void iconOpenCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 DTIconRec *dticon = (DTIconRec *) client_data;

 openApp(dticon->app.form, True);
}

/*-----------------------------------------------------------------------------*/

static void iconFileEditCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppRec *app = &((DTIconRec *) client_data)->app;
 char path[MAXPATHLEN];

 if (app->directory[0])
 {
     strcpy(path, app->directory);
     fnexpand(path);
 }
 else  strcpy(path, user.home);

 doEdit(path, app->fname);
}

/*-----------------------------------------------------------------------------*/

static void iconFileViewCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppRec *app = &((DTIconRec *) client_data)->app;
 char path[MAXPATHLEN];

 if (app->directory[0])
 {
     strcpy(path, app->directory);
     fnexpand(path);
 }
 else  strcpy(path, user.home);

 doView(path, app->fname);
}

/*-----------------------------------------------------------------------------*/

static void iconEditCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppSpecRec *awi = (AppSpecRec *) XtMalloc(sizeof(AppSpecRec));

 awi->win.dticon = (DTIconRec *) client_data;
 awi->item = DTICON;
 dticons_blocked = True;
 if (awi->win.dticon->app.objType == APPLICATION)
     appEdit(awi);
 else  usrObjEdit(awi);
}

/*-----------------------------------------------------------------------------*/

static void iconSaveCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 DTIconRec *dticon = (DTIconRec *) client_data;

 dticon->save = True;
 writeDTIcons(False);
}

/*-----------------------------------------------------------------------------*/

static void iconPopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 DTIconRec *dticon;

 if (!(dticon = findIcon(w)))
     return;
 XmMenuPosition(dticon->popup, (XButtonPressedEvent *)event);
 XtManageChild(dticon->popup);
}

/*-----------------------------------------------------------------------------*/

static void iconRemoveCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 dticons_blocked = True;
 removeIconDialog((DTIconRec *) client_data);
}

/*-------------------------------------------------------------------------*/

static int parseIcon(FILE *fp, char **name, char **directory, char **fname, char **icon, char **push_action, char **drop_action, char **xp, char **yp)
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
  if (!(*xp = split(NULL, ':')))
    return -1;
  if (!(*yp = split(NULL, ':')))
    return -1;

  return l;
}

/*-----------------------------------------------------------------------------*/

static void checkVertPos(DTIconRec *dticon, int *x, int *y)
{
 if (*y > 0 && *y <= winInfo.rootHeight - dticon->height)
     return;
 if (*y < 0)
     *y = winInfo.rootHeight - dticon->height;
 else  *y = 0;

 if (*x < winInfo.rootWidth / 2)
     *x += DTICONWIDTH;
 else  *x -= DTICONWIDTH;
}

/*-----------------------------------------------------------------------------*/

static void checkHorPos(DTIconRec *dticon, int *x, int *y)
{
 if (*x > 0 && *x <= winInfo.rootWidth - dticon->width)
     return;
 if (*x < 0)
     *x = winInfo.rootWidth - dticon->width;
 else  *x = 0;

 if (*y < winInfo.rootHeight / 2)
     *y += DTICONHEIGHT;
 else  *y -= DTICONHEIGHT;
}

/*-----------------------------------------------------------------------------*/

static Boolean tryPosition(DTIconRec *dticon)
{
 int i, x, y;
 Dimension xmax, ymax, ximax, yimax;

 xmax = dticon->x + dticon->width;
 ymax = dticon->y + dticon->height;
 for (i=0; i<n_dtIcons; i++)
 {
     if (dtIcons[i] == dticon)  continue;
     ximax = dtIcons[i]->x + dtIcons[i]->width;
     yimax = dtIcons[i]->y + dtIcons[i]->height;
     if (dtIcons[i]->x < xmax && ximax > dticon->x &&
	 dtIcons[i]->y < ymax && yimax > dticon->y)
     {
	 switch (resources.icon_align)
	 {
	     case Up:
		 x = dticon->x;
		 y = dtIcons[i]->y - dticon->height;
		 checkVertPos(dticon, &x, &y);
		 break;
	     case Down:
		 x = dticon->x;
		 y = dtIcons[i]->y + dtIcons[i]->height;
		 checkVertPos(dticon, &x, &y);
		 break;
	     case Left:
		 x = dtIcons[i]->x - dticon->width;
		 y = dticon->y;
		 checkHorPos(dticon, &x, &y);
		 break;
	     case Right:
		 x = dtIcons[i]->x + dtIcons[i]->width;
		 y = dticon->y;
		 checkHorPos(dticon, &x, &y);
	 }
	 dticon->x = x;
	 dticon->y = y;
	 return False;
     }
 }
 return True;
}

/*-----------------------------------------------------------------------------*/

static void placeDTIcon(DTIconRec *dticon)
{
 Dimension width;
 XmString labelstr;
 String icon_pos;

 if (x0 == -1)
 {
     /* Seems to be necessary at least under HP-UX: */
     icon_pos = XtNewString(resources.icon_pos);
     sscanf(icon_pos, "%d%d", &x0, &y0);
     XTFREE(icon_pos);
     if (x0 < 0)  x0 += winInfo.rootWidth - DTICONWIDTH;
     if (y0 < 0)  y0 += winInfo.rootHeight - DTICONHEIGHT;
 }
 dticon->x = x0;
 dticon->y = y0;
 dticon->width = dticon->app.icon_pm.width;
 dticon->height = dticon->app.icon_pm.height;
 labelstr = XmStringCreateLocalized(dticon->app.name);
 width = XmStringWidth((XmFontList) resources.icon_font, labelstr) + 2;
 dticon->height += XmStringWidth((XmFontList) resources.icon_font, labelstr) + 4;
 if (width > dticon->width)
     dticon->width = width;
 dticon->height += 2 * MARGIN;
 dticon->width += 2 * MARGIN;
 while (!tryPosition(dticon));
 dticon->x += MARGIN;
 dticon->y += MARGIN;
}

/*-----------------------------------------------------------------------------*/

static DTIconRec *createDTIcon(Widget appshell, AppRec *app, int x, int y)
{
 DTIconRec *newDTIcon = (DTIconRec *) XtMalloc(sizeof(DTIconRec));

 newDTIcon->app = *app;
 readApplicationIcon(appshell, &newDTIcon->app, ERR_POPUP);
 newDTIcon->label_mask = None;
 newDTIcon->drop_pixmap = None;
 newDTIcon->moved = False;
 newDTIcon->save = False;
 if (x != -1)
 {
     newDTIcon->x = x;
     newDTIcon->y = y;
 }
 else placeDTIcon(newDTIcon);

 trans = XtParseTranslationTable(icon_translations);
 XtAppAddActions(app_context, icon_actions, XtNumber(icon_actions));

 newDTIcon->shell = XtVaAppCreateShell(NULL, "dticon", topLevelShellWidgetClass, dpy, XmNmwmDecorations, 0, XmNmwmFunctions, 0, XmNtitle, "desktop icon", XmNx, newDTIcon->x, XmNy, newDTIcon->y, XmNallowShellResize, True, XmNoverrideRedirect, resources.override_redirect, NULL);

#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
  XtAddEventHandler(newDTIcon->shell, (EventMask)0, True, _XEditResCheckMessages,
		    NULL);
#endif
#endif

 /* This widget seems to be necessary in order to prevent segfaults in connection with
  popup menus under Linux w/ MetroLink Motif 1.2.4 */
 newDTIcon->cont = XtVaCreateManagedWidget("container", xmFormWidgetClass, newDTIcon->shell, XmNborderWidth, 0, XmNshadowThickness, 0, XmNhighlightThickness, 0, XmNx, 0, XmNy, 0, NULL);
/* XtRealizeWidget(newDTIcon->shell);*/

 return newDTIcon;
}

/*-----------------------------------------------------------------------------*/

static void displayDTIcon(DTIconRec *dticon)
{
 XGCValues xgcv;
 GC shapeGC;
 Window shellwin;
 XmString labelstr;
 Dimension labelwidth, labelheight, togglewidth, toggleheight;
 Dimension tgl_offs_x, lbl_offs_x, lbl_offs_y;
 Atom targets[2];
 Boolean is_file = False;
 int i;

 targets[0] = dragAtoms.file;
 targets[1] = dragAtoms.filelist;
 drop_args[0].value = (XtArgVal) targets;

 dticon->app.form = XtVaCreateManagedWidget("form", xmFormWidgetClass, dticon->cont, XmNtranslations, XtParseTranslationTable(form_translations), XmNbackground, resources.dticon_color, XmNforeground, resources.label_color, XmNborderWidth, 0, XmNshadowThickness, 0, XmNhighlightThickness, 0, XmNx, 0, XmNy, 0, NULL);

 if (dticon->app.fname[0])
 {
     char path[MAXPATHLEN];
     struct stat stats;
     int errfl = 0;

     if (dticon->app.directory[0])
     {
	 strcpy(path, dticon->app.directory);
	 fnexpand(path);
	 errfl = chdir(path);
     }
     else  errfl = chdir(user.home);
     if (!errfl && !(stat(dticon->app.fname, &stats)) && S_ISREG(stats.st_mode))
	 is_file = True;
 }
 if (is_file)
 {
     for (i=0; i < XtNumber(file_popup_menu); i++)
	 file_popup_menu[i].callback_data = dticon;
     dticon->popup = BuildMenu(dticon->app.form, XmMENU_POPUP, "Button Actions", 0, False, file_popup_menu);
     if (resources.auto_save)
	 XtVaSetValues(file_popup_menu[5].object, XmNsensitive, False, NULL);
 }
 else
 {
     for (i=0; i < XtNumber(file_popup_menu); i++)
	 popup_menu[i].callback_data = dticon;
     dticon->popup = BuildMenu(dticon->app.form, XmMENU_POPUP, "Button Actions", 0, False, popup_menu);
     if (resources.auto_save)
	 XtVaSetValues(popup_menu[3].object, XmNsensitive, False, NULL);
 }

 labelstr = XmStringCreateLocalized(dticon->app.name);
 labelwidth = XmStringWidth((XmFontList) resources.icon_font, labelstr) + 2;
 togglewidth = dticon->app.icon_pm.width;
 toggleheight = dticon->app.icon_pm.height;
 if (labelwidth > togglewidth)
 {
     tgl_offs_x = (labelwidth - togglewidth) / 2;
     lbl_offs_x = 0;
 }
 else
 {
     tgl_offs_x = 0;
     lbl_offs_x = (togglewidth - labelwidth) / 2;
 }
 lbl_offs_y = dticon->app.icon_pm.height + 2;

 dticon->app.toggle = XtVaCreateManagedWidget("picture", xmLabelWidgetClass, dticon->app.form, XmNlabelType, XmPIXMAP, XmNlabelPixmap, dticon->app.icon_pm.bm, XmNtranslations, trans, XmNborderWidth, 0, XmNhighlightThickness, 0, XmNbackground, resources.dticon_color, XmNforeground, resources.label_color, XmNx, tgl_offs_x, XmNy, 0, NULL);
/*  RBW - 2001/08/15  */
XtInsertEventHandler(dticon->app.toggle,
                     EnterWindowMask|LeaveWindowMask,
                     False,
                     (XtEventHandler)CatchEntryLeave2,
                     (XtPointer)0,
                     XtListHead);

 dticon->app.label = XtVaCreateManagedWidget("label", xmLabelWidgetClass, dticon->app.form, XmNlabelString, labelstr, XmNfontList, resources.icon_font, XmNtranslations, trans, XmNbackground, resources.dticon_color, XmNforeground, resources.label_color, XmNborderWidth, 0, XmNhighlightThickness, 0, XmNx, lbl_offs_x, XmNy, lbl_offs_y, NULL);
/*  RBW - 2001/08/15  */
XtInsertEventHandler(dticon->app.label,
                     EnterWindowMask|LeaveWindowMask,
                     False,
                     (XtEventHandler)CatchEntryLeave2,
                     (XtPointer)0,
                     XtListHead);

 XmStringFree(labelstr);

 XtRealizeWidget(dticon->shell);
 shellwin = XtWindow(dticon->shell);

 XtVaGetValues(dticon->app.label, XmNwidth, &labelwidth, XmNwidth, &labelheight, NULL);
 labelheight += 2;
 dticon->label_mask = XCreatePixmap(dpy, shellwin, labelwidth, labelheight, 1);
 shapeGC = XCreateGC(dpy, dticon->label_mask, 0, &xgcv);
 XSetForeground(dpy, shapeGC, 1);
 XFillRectangle(dpy, dticon->label_mask, shapeGC, 0, 0, labelwidth, labelheight);
 XFreeGC(dpy, shapeGC);
 if (dticon->app.icon_pm.mask == None)
 {
     dticon->app.icon_pm.mask = XCreatePixmap(dpy, shellwin, dticon->app.icon_pm.width, dticon->app.icon_pm.height, 1);
     shapeGC = XCreateGC(dpy, dticon->app.icon_pm.mask, 0, &xgcv);
     XSetForeground(dpy, shapeGC, 1);
     XFillRectangle(dpy, dticon->app.icon_pm.mask, shapeGC, 0, 0, togglewidth, toggleheight);
     XFreeGC(dpy, shapeGC);
 }
 XShapeCombineMask(dpy, shellwin, ShapeBounding, tgl_offs_x + 2, 2, dticon->app.icon_pm.mask, ShapeSet);
 XShapeCombineMask(dpy, shellwin, ShapeBounding, lbl_offs_x + 2, lbl_offs_y + 2, dticon->label_mask, ShapeUnion);
 XShapeCombineMask(dpy, XtWindow(dticon->app.form), ShapeClip, tgl_offs_x + 2, 2, dticon->app.icon_pm.mask, ShapeSet);
 XShapeCombineMask(dpy, XtWindow(dticon->app.form), ShapeClip, lbl_offs_x + 2, lbl_offs_y + 2, dticon->label_mask, ShapeUnion);
 XShapeCombineMask(dpy, XtWindow(dticon->app.toggle), ShapeClip, 2, 2, dticon->app.icon_pm.mask, ShapeSet);

 XtVaGetValues(dticon->shell, XmNwidth, &dticon->width, XmNheight, &dticon->height, NULL);

 dticon->drop_pixmap = XCreatePixmap(dpy, shellwin, dticon->width, dticon->height, DefaultDepth(dpy, DefaultScreen(dpy)));
 shapeGC = XCreateGC(dpy, dticon->drop_pixmap, 0, &xgcv);
 XSetForeground(dpy, shapeGC, resources.drop_color);
 XFillRectangle(dpy, dticon->drop_pixmap, shapeGC, 0, 0, dticon->width, dticon->height);
 XFreeGC(dpy, shapeGC);
 drop_args[4].value = (XtArgVal) dticon->drop_pixmap;

 if (dticon->app.drop_action[0])
     XmDropSiteRegister(dticon->app.form, drop_args, XtNumber(drop_args));

}

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

DTIconRec *findIcon(Widget w)
{
 int i;
 DTIconRec *dticon;

 for (i=0; i<n_dtIcons; i++)
 {
     dticon = dtIcons[i];
     if (dticon->app.form == w || dticon->app.toggle == w || dticon->app.label == w)
	 return dticon;
 }
 return NULL;
}

/*-----------------------------------------------------------------------------*/

void removeIconProc(XtPointer awp, int conf)
{
 DTIconRec *dticon = (DTIconRec *) awp;
 int i;

 dticons_blocked = False;
 if (conf == YES)
 {
     XtDestroyWidget(dticon->shell);
     freeApplicationResources(&dticon->app);
     if (dticon->label_mask != None)
	 freePixmap(dticon->label_mask);
     if (dticon->drop_pixmap != None)
	 freePixmap(dticon->drop_pixmap);
     for (i=0; i<n_dtIcons; i++)
     {
	 if (dtIcons[i] == dticon)
	 {
	     dtIcons[i] = dtIcons[--n_dtIcons];
	     break;;
	 }
     }
     XTFREE(dticon);
     dtIcons = (DTIconRec **) XTREALLOC(dtIcons, n_dtIcons * sizeof(DTIconRec *));
     if (resources.auto_save)  writeDTIcons(True);
 }
}

/*-----------------------------------------------------------------------------*/

void changeDTIconProc(XtPointer awp, int conf)
{
 DTIconRec *dticon = ((AppSpecRec *) awp)->win.dticon;

 XTFREE(awp);
 dticons_blocked = False;
 if (conf == YES) {
     XtDestroyWidget(dticon->app.form);
     if (dticon->app.loaded)  freeIcon(dticon->app.icon_pm);
     if (dticon->label_mask != None)
	 freePixmap(dticon->label_mask);
     if (dticon->drop_pixmap != None)
	 freePixmap(dticon->drop_pixmap);
     dticon->app.objType = getObjectType(dticon->app.push_action);
     readApplicationIcon(getAnyShell(), &dticon->app, ERR_POPUP);
     displayDTIcon(dticon);
     if (resources.auto_save)  writeDTIcons(True);
 }
}

/*-----------------------------------------------------------------------------*/

void appOnDeskCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppWindowRec *aw = (AppWindowRec *) client_data;
 AppRec *app, newapp;
 Widget button;
 int item;

 XtVaGetValues(aw->form, XmNuserData, (XtPointer) &button, NULL);
 item = findAppItem(aw, button);
 app = &aw->apps[item];
 if (app->objType == APPLICATION)
 {
     newapp.name = XtNewString(app->name);
     newapp.directory = XtNewString(app->directory);
     newapp.fname = XtNewString(app->fname);
     newapp.icon = XtNewString(app->icon);
     newapp.push_action = XtNewString(app->push_action);
     newapp.drop_action = XtNewString(app->drop_action);
     newapp.objType = APPLICATION;
     newapp.remove_file = False;
 }
 else  dupObject(aw->shell, &newapp, app);
 dtIcons = (DTIconRec **) XTREALLOC(dtIcons, (n_dtIcons + 1) * sizeof(DTIconRec *));
 dtIcons[n_dtIcons] = createDTIcon(aw->shell, &newapp, -1, -1);
 displayDTIcon(dtIcons[n_dtIcons++]);
 if (resources.auto_save)  writeDTIcons(True);
}

/*-----------------------------------------------------------------------------*/

void fileOnDeskCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppList *appl;
 AppRec newapp;
 FileWindowRec *fw = (FileWindowRec *) client_data;

 appl = applistFromFiles(fw);
 newapp.name = XtNewString(appl[0]->name);
 newapp.directory = XtNewString(appl[0]->directory);
 newapp.fname = XtNewString(appl[0]->fname);
 newapp.icon = XtNewString(appl[0]->icon);
 newapp.push_action = appl[0]->push_action;
 newapp.drop_action = appl[0]->drop_action;
 newapp.objType = APPLICATION;
 newapp.remove_file = False;
 dtIcons = (DTIconRec **) XTREALLOC(dtIcons, (n_dtIcons + 1) * sizeof(DTIconRec *));
 dtIcons[n_dtIcons] = createDTIcon(fw->shell, &newapp, -1, -1);
 displayDTIcon(dtIcons[n_dtIcons++]);
 if (resources.auto_save)  writeDTIcons(True);
 XTFREE(appl[0]);
 XTFREE(appl);
}

/*-----------------------------------------------------------------------------*/

void overwriteDTIcons(Boolean all)
{
    FILE *fp;
    int i;
    time_t t;
    char name[2*MAXAPPSTRINGLEN], directory[2*MAXAPPSTRINGLEN],
	fname[2*MAXAPPSTRINGLEN], icon[2*MAXAPPSTRINGLEN],
	push_action[2*MAXAPPSTRINGLEN], drop_action[2*MAXAPPSTRINGLEN];

    if (!(fp = fopen(resources.dticon_file, "w")))
	{
	    sysError(getAnyShell(), "Error saving desktop icons:");
	    return;
	}

    time(&t);
    fprintf(fp, icon_file_header, ctime(&t));

    for (i=0; i<n_dtIcons; i++) {
	if (all || dtIcons[i]->save) {
	    expand(name, dtIcons[i]->app.name, "\\:");
	    expand(directory, dtIcons[i]->app.directory, "\\:");
	    expand(fname, dtIcons[i]->app.fname, "\\:");
	    expand(icon, dtIcons[i]->app.icon, "\\:");
	    expand(push_action, dtIcons[i]->app.push_action, "\\:");
	    expand(drop_action, dtIcons[i]->app.drop_action, "\\:");
	    fprintf(fp, "%s:%s:%s:%s:%s:%s:%d:%d\n", name, directory, fname,
		    icon, push_action, drop_action, dtIcons[i]->x,
		    dtIcons[i]->y);
	    dtIcons[i]->save = True;
	    dtIcons[i]->app.remove_file = False;
	}
    }
  
    if (fclose(fp))
	sysError(getAnyShell(), "Error saving desktop icons:");

    dticons_readable = !stat(resources.dticon_file, &dticons_stats);

}

/*-----------------------------------------------------------------------------*/

void writeDTIconsProc(XtPointer allp, int conf)
{
    dticons_blocked = False;
    if (conf == YES)
	overwriteDTIcons(*(Boolean *)allp);
    XTFREE(allp);
}

/*-----------------------------------------------------------------------------*/

void writeDTIcons(Boolean all)
{
    struct stat cur;
    if (!stat(resources.dticon_file, &cur) &&
	(!dticons_readable || cur.st_ctime > dticons_stats.st_ctime ||
	 cur.st_mtime > dticons_stats.st_mtime)) {
	dticons_blocked = True;
	overwriteIconsDialog(all);
    } else
	overwriteDTIcons(all);
}

/*-----------------------------------------------------------------------------*/

void readDTIcons(void)
{
 FILE *fp;
 AppRec app;
 char *name, *directory, *fname, *icon, *push_action, *drop_action, *xp, *yp;
 char s[MAXAPPSTRINGLEN];
 int i, p, x, y;

 XtAppAddActions(app_context, icon_actions, XtNumber(icon_actions));
 trans = XtParseTranslationTable(icon_translations);

 dticons_blocked = False;

 if (stat(resources.dticon_file, &dticons_stats) ||
     !(fp = fopen(resources.dticon_file, "r")))
 {
     dticons_readable = False;
     error(getAnyShell(), "Cannot read desktop icons from", resources.dticon_file);
     return;
 }

 dticons_readable = True;

 for (i=0; (p = parseIcon(fp, &name, &directory, &fname, &icon, &push_action, &drop_action, &xp, &yp)) > 0; i++)
 {
     dtIcons = (DTIconRec **) XTREALLOC(dtIcons, (i+1) * sizeof(DTIconRec *));
     app.name = XtNewString(strparse(s, name, "\\:"));
     app.directory = XtNewString(strparse(s, directory, "\\:"));
     app.fname = XtNewString(strparse(s, fname, "\\:"));
     app.icon = XtNewString(strparse(s, icon, "\\:"));
     app.push_action = XtNewString(strparse(s, push_action, "\\:"));
     app.drop_action = XtNewString(strparse(s, drop_action, "\\:"));
     app.objType = getObjectType(app.push_action);
     app.remove_file = False;
     if ((x = atoi(strparse(s, xp, "\\:"))) < 0)
	 x += winInfo.rootWidth - DTICONWIDTH;
     if ((y = atoi(strparse(s, yp, "\\:"))) < 0)
	 y += winInfo.rootHeight - DTICONHEIGHT;

     dtIcons[i] = createDTIcon(getAnyShell(), &app, x, y);
     dtIcons[i]->save = True;
     displayDTIcon(dtIcons[i]);
 }
 
 if (p == -1)
     error(getAnyShell(), "Error reading desktop icons from", resources.dticon_file);

 n_dtIcons = i;

 if (fclose(fp))
     sysError(getAnyShell(), "Error reading desktop icons:");
}

/*---------------------------------------------------------------------------*/

/* Function for automatically changing desktop icons, called from appUpdate().
Written by Albert Graef */

void updateIconDisplay(void)
{
  struct stat cur;
  Boolean reread = False;
  if (dticons_blocked) return;
  if (resources.check_application_files)
      if (dticons_readable)
	  reread = stat(resources.dticon_file, &cur) || cur.st_ctime >
	      dticons_stats.st_ctime || cur.st_mtime > dticons_stats.st_mtime;
      else
	  reread = !stat(resources.dticon_file, &cur);
  if (reread) {
      FILE *fp;
      AppRec *app;
      char *name, *directory, *fname, *icon, *push_action, *drop_action,
	  *xp, *yp;
      char *nname, *ndirectory, *nfname, *nicon, *npush_action, *ndrop_action;
      char s[MAXAPPSTRINGLEN];
      int i, j, p, x, y;
      zzz();
      dticons_readable = !stat(resources.dticon_file, &dticons_stats);
      if (!(fp = fopen(resources.dticon_file, "r")))
	  {
	      dticons_readable = False;
	      error(getAnyShell(), "Cannot read desktop icons from",
		    resources.dticon_file);
	      i = 0;
	      goto remove;
	  }
      /* update existing icons/append new ones */
      for (i=0; (p = parseIcon(fp, &name, &directory, &fname, &icon,
			       &push_action, &drop_action, &xp, &yp)) > 0;
		i++) {
	  if ((x = atoi(strparse(s, xp, "\\:"))) < 0)
	      x += winInfo.rootWidth - DTICONWIDTH;
	  if ((y = atoi(strparse(s, yp, "\\:"))) < 0)
	      y += winInfo.rootHeight - DTICONHEIGHT;
	  nname = XtNewString(strparse(s, name, "\\:"));
	  ndirectory = XtNewString(strparse(s, directory, "\\:"));
	  nfname = XtNewString(strparse(s, fname, "\\:"));
	  nicon = XtNewString(strparse(s, icon, "\\:"));
	  npush_action = XtNewString(strparse(s, push_action, "\\:"));
	  ndrop_action = XtNewString(strparse(s, drop_action, "\\:"));
	  if (i < n_dtIcons) {
	      DTIconRec *dticon = dtIcons[i];
	      app = &dticon->app;
	      if (!(x != dticon->x ||
		    y != dticon->y ||
		    strcmp(nname, app->name) ||
		    strcmp(ndirectory, app->directory) ||
		    strcmp(nfname, app->fname) ||
		    strcmp(nicon, app->icon) ||
		    strcmp(npush_action, app->push_action) ||
		    strcmp(ndrop_action, app->drop_action))) {
		  XTFREE(nname);
		  XTFREE(ndirectory);
		  XTFREE(nfname);
		  XTFREE(nicon);
		  XTFREE(npush_action);
		  XTFREE(ndrop_action);
		  continue;
	      }
	      XtDestroyWidget(dticon->app.form);
	      if (dticon->label_mask != None)
		  freePixmap(dticon->label_mask);
	      if (dticon->drop_pixmap != None)
		  freePixmap(dticon->drop_pixmap);
	      freeApplicationResources(&dticon->app);
	  } else {
	      dtIcons = (DTIconRec **) XTREALLOC(dtIcons, (i+1) * sizeof(DTIconRec *));
	      dtIcons[i] = (DTIconRec *) XtMalloc(sizeof(DTIconRec));
	  }
	  app = &dtIcons[i]->app;
	  app->name = nname;
	  app->directory = ndirectory;
	  app->fname = nfname;
	  app->icon = nicon;
	  app->push_action = npush_action;
	  app->drop_action = ndrop_action;
	  app->objType = getObjectType(app->push_action);
	  app->remove_file = False;
	  dtIcons[i]->x = x;
	  dtIcons[i]->y = y;
	  readApplicationIcon(getAnyShell(), app, ERR_POPUP);
	  dtIcons[i]->label_mask = None;
	  dtIcons[i]->drop_pixmap = None;
	  dtIcons[i]->moved = False;
	  dtIcons[i]->save = True;
	  displayDTIcon(dtIcons[i]);
      }
    remove:
      /* remove any remaining icons */
      for (j = i; j < n_dtIcons; j++) {
	  DTIconRec *dticon = dtIcons[j];
	  XtDestroyWidget(dticon->shell);
	  freeApplicationResources(&dticon->app);
	  if (dticon->label_mask != None)
	      freePixmap(dticon->label_mask);
	  if (dticon->drop_pixmap != None)
	      freePixmap(dticon->drop_pixmap);
	  XTFREE(dticon);
      }

      if (i < n_dtIcons)
	  dtIcons = (DTIconRec **)
	      XTREALLOC(dtIcons, i * sizeof(DTIconRec *));
      n_dtIcons = i;

      wakeUp();

      if (dticons_readable && p == -1)
	  error(getAnyShell(), "Error reading desktop icons from",
		resources.dticon_file);

      if (dticons_readable && fclose(fp))
	  sysError(getAnyShell(), "Error reading desktop icons:");
  }
}



void CatchEntryLeave2(Widget w, XtPointer cld, XEvent* ev, Boolean *ctd)
{
/*
    RBW - 2001/08/15 - eat extraneous LeaveNotify/EnterNotify events caused by
    some window managers when a ButtonPress happens, so that the Xt translation
    mechanism for double-clicks doesn't get confused. (The events typically
    result from the window manager's having an ill-synchronized button grab
    on a parent window.) The workaround is based on a similar "fix" by Till
    Straumann for xfm.
    This function is installed as an event handler for both the file window
    icon and text widgets (in FmFw.c) and the desktop icons/icon labels (in
    FmIcons.c).
*/
if (ev->xcrossing.mode == NotifyGrab || ev->xcrossing.mode == NotifyUngrab)  {
  *ctd = False;  /* Eat this event */
  }  else  {
  *ctd = True;  /* Don't eat this event */
  }
return;
}
