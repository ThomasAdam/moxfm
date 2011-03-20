/*-----------------------------------------------------------------------------
  Module FmDragDrop.c                                                             

  (c) Oliver Mai 1995, 1996
                                                                           
  functions for drag'n'drop                                                 
-----------------------------------------------------------------------------*/

#include <string.h>
#include <ctype.h>
#include <Xm/Xm.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>
#include <X11/xpm.h>

#include "Am.h"
#include "Fm.h"

#define MAXDNDSTRINGLEN 250000	/* Maximum size of files
 which may be transferred as STRINGs, hope this is OK on most systems. OM */

#if XmVersion >= 2000		/* To work around Motif 2.0 bug with repainting drop sites, might not be present on non-Linux systems */
#define REFRESH_BUG
#endif

char *DNDbuffer;
static IconRec icon;
static Boolean icon_loaded;

typedef struct
{
    union
    {
	AppWindowRec *aw;
	DTIconRec *dticon;
    } win;
    String name;	/* == NULL for desktop icons */
} AppNameRec;

typedef struct
{
    FileWindowRec *fw;
    SelFileNamesRec *files;
    Widget dropTarget;
} FileSrcRec;

#ifdef REFRESH_BUG

#define APPLICATION_WINDOW 1
#define DESKTOP_ICON 2
#define FILE_WINDOW 3

typedef struct
{
    union
    {
	FileWindowRec *fw;
	AppWindowRec *aw;
	DTIconRec *dticon;
    } win;
    int type;
    Widget form, toggle, label;
} UpdateRec;
#endif	/* REFRESH_BUG */

typedef struct
{
    String directory;
    Boolean free_dir;
    Widget shell;
    unsigned char operation;
#ifdef REFRESH_BUG
    UpdateRec *upd;
#endif
} FileTargetRec;

typedef struct
{
    String directory;
    Widget shell;
    String drop_action;
    Boolean free_drop;
    String first_arg;
    IconRec icon_pm;
#ifdef REFRESH_BUG
    UpdateRec *upd;
#endif
} AppDropRec;

static Arg drop_activate[] = {
    { XmNdropSiteActivity, XmDROP_SITE_ACTIVE }
};

static Arg drop_inactivate[] = {
    { XmNdropSiteActivity, XmDROP_SITE_INACTIVE }
};

static void write2buffer(void *target, void *src, size_t size);
static unsigned long fillDNDbuffer(AppList *applist, int n_apps);
static void readDNDbuffer(char *field[], char **src);
static void dndRemoveProc(AppWindowRec *aw, int i);
static SelFileNamesRec *filesFromBuffer(char *buffer, Widget shell);
static SelFileNamesRec *filesFromName(char *filename, Widget shell);
static int initAppDrag(Widget parent, AppRec *app, Arg *args, Boolean dticonflag);
#ifdef REFRESH_BUG
static Boolean widgetUpdate(UpdateRec *upd);
#endif

AppList *applistFromFiles(FileWindowRec *fw);

/*-------------------------------------------------------------------------*/

static void write2buffer(void *target, void *src, size_t size)
{
 if (little_endian)
 {
     size_t i, j;

     for (i=0, j=size-1; i < size; i++, j--)
	 ((char *) target)[i] = ((char *) src)[j];
 }
 else  memcpy(target, src, size);
}

/*-------------------------------------------------------------------------*/

static unsigned long fillDNDbuffer(AppList *applist, int n_apps)
{
 AppRec *appl;
 unsigned long length = sizeof(int) + 1;
 char *ptr;
 int i;

 for (i=0; i < n_apps; i++)
     length += strlen(applist[i]->name) +
       strlen(applist[i]->directory) +
       strlen(applist[i]->fname) + strlen(applist[i]->icon) +
       strlen(applist[i]->push_action) +
       strlen(applist[i]->drop_action) + 6;
 ptr = DNDbuffer = (char *) XtMalloc(length * sizeof(char));
 write2buffer(ptr, &n_apps, sizeof(int));  ptr += sizeof(int);
 for (i=0; i < n_apps; i++)
 {
     appl = applist[i];
     strcpy(ptr, appl->name);  ptr += strlen(ptr);
     strcpy(++ptr, appl->directory);  ptr += strlen(ptr);
     strcpy(++ptr, appl->fname);  ptr += strlen(ptr);
     strcpy(++ptr, appl->icon);  ptr += strlen(ptr);
     strcpy(++ptr, appl->push_action);  ptr += strlen(ptr);
     strcpy(++ptr, appl->drop_action); ptr += strlen(ptr) + 1;
 }
 return length;
}

/*-------------------------------------------------------------------------*/

static void readDNDbuffer(char *field[], char **src)
{
 *field = XtNewString(*src);
 *src += strlen(*src) + 1;
}

/*-------------------------------------------------------------------------*/

static int initAppDrag(Widget parent, AppRec *app, Arg *args, Boolean dticonflag)
{
 Widget drag_icon;
 String iconfile;
 static Atom targets[3];
 int i, ntargets, op = XmDROP_COPY | XmDROP_LINK;

 if (app->objType == APPLICATION)
 {
     if (!app->loaded || !app->icon[0])
	 iconfile = "";
     else  iconfile = app->icon;
 }
 else  iconfile = usrObjects.obj[app->objType].drag_icon;

 if (!iconfile[0])
 {
     icon_loaded = False;
     if (app->objType == APPLICATION)
	 icon = defaultIcon(app->name, app->directory, app->fname);
     else  icon.bm = None;
 }
 else
 {
     icon = readIcon(parent, iconfile);
     icon_loaded = True;
 }

 if (icon.bm != None)
 {
     i = 0;
     XtSetArg(args[i], XmNpixmap, icon.bm); i++;
     XtSetArg(args[i], XmNdepth, DefaultDepth(dpy, DefaultScreen(dpy))); i++;
/*     if (icon.mask != None) XtSetArg(args[i], XmNmask, icon.mask); i++;*/
     XtSetArg(args[i], XmNwidth, icon.width); i++;
     XtSetArg(args[i], XmNheight, icon.height); i++;
     drag_icon = XmCreateDragIcon(parent, "drag_icon", args, i);
 }

 i=0;
 XtSetArg(args[i], XmNblendModel, XmBLEND_ALL); i++;
 XtSetArg(args[i], XmNcursorBackground, winInfo.background); i++;
 XtSetArg(args[i], XmNcursorForeground, resources.label_color); i++;
 if (icon.bm != None)
 {
     XtSetArg(args[i], XmNsourcePixmapIcon, drag_icon); i++;
 }
 if  (!dticonflag)  op |= XmDROP_MOVE;
 XtSetArg(args[i], XmNdragOperations, op); i++;
 XtSetArg(args[i], XmNconvertProc, appConvProc); i++;
 ntargets = 1;
 if (app->objType != APPLICATION)
     targets[0] = usrObjects.obj[app->objType].atom;
 else
 {
     targets[0] = dragAtoms.appl;
     if (dticonflag && app->fname[0])
     {
	 ntargets = 3;
	 targets[1] = dragAtoms.filelist;
	 targets[2] = dragAtoms.file;
     }
 }
 XtSetArg(args[i], XmNexportTargets, (XtPointer) targets); i++;
 XtSetArg(args[i], XmNnumExportTargets, ntargets); i++;

 return i;
}

/*-------------------------------------------------------------------------*/

void startAppDrag(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 Widget dc;
 AppNameRec *awi;
 Arg args[15];
 int item, n;

 awi = (AppNameRec *) XtMalloc(sizeof(AppNameRec));
 if (!(awi->win.aw = findAppWidget(w, &item)))
 {
     XTFREE(awi);
     return;
 }
 awi->name = awi->win.aw->apps[item].name;

 if (awi->win.aw->blocked)
 {
     XTFREE(awi);
     return;
 }

 n = initAppDrag(awi->win.aw->shell, &awi->win.aw->apps[item], args, False);
 XtSetArg(args[n], XmNclientData, (XtPointer) awi); n++;
 dc = XmDragStart(awi->win.aw->form, event, args, n);
 XtAddCallback(dc, XmNdragDropFinishCallback, appDropFinish, NULL);
 awi->win.aw->drag_source = True;
}

/*-------------------------------------------------------------------------*/

void startDTIconDrag(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 Widget dc;
 AppNameRec *awi;
 Arg args[15];
 int n;

 awi = (AppNameRec *) XtMalloc(sizeof(AppNameRec));
 if (!(awi->win.dticon = findIcon(w)))
 {
     XTFREE(awi);
     return;
 }
 awi->name = NULL;

 XmDropSiteUpdate(awi->win.dticon->app.form, drop_inactivate, XtNumber(drop_inactivate));

 n = initAppDrag(awi->win.dticon->shell, &awi->win.dticon->app, args, True);
 XtSetArg(args[n], XmNclientData, (XtPointer) awi); n++;
 dc = XmDragStart(awi->win.dticon->shell, event, args, n);
 XtAddCallback(dc, XmNdragDropFinishCallback, appDropFinish, NULL);
 awi->win.aw->drag_source = True;
}

/*-------------------------------------------------------------------------*/

Boolean appConvProc(Widget w, Atom *selection, Atom *target, Atom *type_return, XtPointer *value_return, unsigned long *length_return, int *format_return)
{
 AppNameRec *awi;
 AppRec *app;
 Atom TARGETS = XmInternAtom(dpy, "TARGETS", False);
 Boolean res = False;
 int item;

 if (*selection != dragAtoms.drop)  return False;
 XtVaGetValues(w, XmNclientData, &awi, NULL);
 if (!awi)  return False;
 if (awi->name)
 {
     item = findAppItemByName(awi->win.aw, awi->name);
     if (item == -1)  return False;
     app = &awi->win.aw->apps[item];
 }
 else  app = &awi->win.dticon->app;

 if (*target == dragAtoms.appl)
 {
     *length_return = fillDNDbuffer(&app, 1) * sizeof(char);
     *type_return =dragAtoms.appl;
     *value_return = DNDbuffer;
     *format_return = 8;
     res = True;
 }
 else if (*target == dragAtoms.file)
 {
     if (!awi->name)	/* file target is not supported for application windows */
     {
	 size_t len;
	 char dir[MAXPATHLEN];

	 if (app->directory[0])
	 {
	     strcpy(dir, app->directory);
	     fnexpand(dir);
	 }
	 else  strcpy(dir, user.home);
	 if (dir[strlen(dir) - 1] != '/')
	     strcat(dir, "/");
	 len = strlen(dir) + strlen(app->fname) + 1;
	 DNDbuffer = (char *) XtMalloc(len * sizeof(char));
	 strcpy(DNDbuffer, dir);
	 strcat(DNDbuffer, app->fname);
	 *length_return = len * sizeof(char);
	 *type_return = dragAtoms.file;
	 *value_return = DNDbuffer;
	 *format_return = 8;
	 res = True;
     }
     else  res = False;
 }
 else if (*target == dragAtoms.filelist)
 {
     if (!awi->name)	/* filelist target is not supported for application windows */
     {
	 char dir[MAXPATHLEN];
	 size_t len, pos;
	 int n = 1;

	 if (app->directory[0])
	 {
	     strcpy(dir, app->directory);
	     fnexpand(dir);
	 }
	 else  strcpy(dir, user.home);
	 len = strlen(user.hostname) + strlen(dir) + sizeof(int) / sizeof(char) + strlen(app->fname) + 3;
	 DNDbuffer = (char *) XtMalloc(len * sizeof(char));
	 strcpy(DNDbuffer, user.hostname);
	 pos = strlen(user.hostname) + 1;
	 strcpy(&DNDbuffer[pos], dir);
	 pos += strlen(dir) + 1;
	 write2buffer(&DNDbuffer[pos], &n, sizeof(int));
	 pos += sizeof(int) / sizeof(char);
	 strcpy(&DNDbuffer[pos], app->fname);
	 *length_return = len * sizeof(char);
	 *type_return = dragAtoms.filelist;
	 *value_return = DNDbuffer;
	 *format_return = 8;
	 res = True;
     }
     else  res = False;
 }
 else if (*target == dragAtoms.delete)
 {
     if (awi->name)	/* delete target is not supported for desktop icons */
     {
	 dndRemoveProc(awi->win.aw, item);
	 *type_return = XmInternAtom(dpy, "NULL", False);
	 *value_return = NULL;
	 *length_return = 0;
	 *format_return = 8;
	 res = True;
     }
     else  res = False;
 }
 else if (*target == TARGETS)	/* This target is required by ICCC */
 {
     Atom MULTIPLE = XmInternAtom(dpy, "MULTIPLE", False);
     Atom TIMESTAMP = XmInternAtom(dpy, "TIMESTAMP", False);
     Atom *targs = (Atom *) XtMalloc((5 + usrObjects.nr) * sizeof(Atom));
     int i, n;

     targs[0] = dragAtoms.appl;
     targs[1] = TARGETS;
     targs[2] = MULTIPLE;	/* supported in the Intrinsics */
     targs[3] = TIMESTAMP;	/* supported in the Intrinsics */
     n = 4;
     for (i=0; i < usrObjects.nr; i++)
	 targs[n++] = usrObjects.obj[i].atom;
     if (awi->name)	/* Application window */
	 targs[n++] = dragAtoms.delete;
     else			/* Desktop icon */
     {
	 targs[n++] = dragAtoms.filelist;
	 targs[n++] = dragAtoms.file;
     }
     *type_return = XA_ATOM;
     *value_return = targs;
     *length_return = n * sizeof(Atom) >> 2;
     *format_return = 8 * sizeof(XtPointer);
     res = True;
 }
 else		/* This should mean it's a user specified target */
 {
     FILE *fin;
     struct stat stats;
     int i;

     for (i=0; i < usrObjects.nr; i++)
     {
	 if (*target == usrObjects.obj[i].atom)
	 {
	     if (chdir(app->directory) || !app->fname[0])
		 res = False;
	     else if (stat(app->fname, &stats) || (stats.st_mode & S_IFMT) != S_IFREG || stats.st_size > MAXDNDSTRINGLEN)
		 res = False;
	     else if (!(fin = fopen(app->fname, "r")))
		 res = False;
	     else if (!(DNDbuffer = (char *) XtMalloc((stats.st_size) * sizeof(char))))
	     {
		 fclose(fin);
		 res = False;
	     }
	     else if (fread(DNDbuffer, sizeof(char), stats.st_size, fin) != stats.st_size)
	     {
		 XTFREE(DNDbuffer);
		 /*	 DNDbuffer = NULL;*/
		 fclose(fin);
		 res = False;
	     }
	     else
	     {
		 *length_return = stats.st_size;
		 *type_return = usrObjects.obj[i].atom;
		 *value_return = DNDbuffer;
		 *format_return = 8;
		 res = True;
	     }
	     chdir(user.home);
	     break;
	 }
     }
 }

 return res;
}

/*-------------------------------------------------------------------------*/

void appDropFinish(Widget w, XtPointer client_data, XtPointer call_data)
{
 Widget drag_icon = NULL;
 AppNameRec *awi;

 XtVaGetValues(w, XmNsourcePixmapIcon, &drag_icon, XmNclientData, &awi, NULL);
 if (drag_icon)
 {
     XtDestroyWidget(drag_icon);
     if (icon_loaded)  freeIcon(icon);
 }
 if (!(awi->name))
     XmDropSiteUpdate(awi->win.dticon->app.form, drop_activate, XtNumber(drop_activate));

 awi->win.aw->drag_source = False;
 XTFREE(awi);
}

/*-------------------------------------------------------------------------*/

void dndRemoveProc(AppWindowRec *aw, int i)
{
 size_t size;

 if (aw->apps[i].objType != APPLICATION)
 {
     chdir(aw->apps[i].directory);
     unlink(aw->apps[i].fname);
     chdir(user.home);
     aw->apps[i].remove_file = False;
 }
 freeApplicationResources(&aw->apps[i]);
 if ((size = (aw->n_apps -i-1) * sizeof(AppRec)))
     memmove(&aw->apps[i], &aw->apps[i+1], size);
 aw->n_apps--;
 aw->apps = (AppRec *) XTREALLOC(aw->apps, aw->n_apps * sizeof(AppRec));
 aw->modified = True;
 aw->update = True;
/*
 XtAppAddWorkProc(app_context, (XtWorkProc)  updateApplicationDisplay, aw);
 */
 updateApplicationDisplay(aw);
 if (resources.auto_save)
     writeApplicationData(aw);
}

/*-------------------------------------------------------------------------*/

void handleAppDrop(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmDropProcCallback dropData = (XmDropProcCallback) call_data;
 Widget dc = dropData->dragContext;
 Atom *exportTargets;
 Cardinal numExportTargets;
 XmDropTransferEntryRec transEntry[2];
 AppSpecRec *awi;
 Boolean applDrop = False;
 int objType = APPLICATION;
 Arg args[5];
 Window win, child, root;
 Widget button;
 int i, j, x, y, x_win, y_win;
 unsigned int mask;

 awi = (AppSpecRec *) XtMalloc(sizeof(AppSpecRec));
 if (!(awi->win.aw = findAppWidgetByForm(w)))
 {
     XTFREE(awi);
     return;
 }
 win = XtWindow(awi->win.aw->icon_box);
 XQueryPointer(dpy, win, &root, &child, &x, &y, &x_win, &y_win, &mask);
 XTranslateCoordinates(dpy, win, win, x_win, y_win, &x_win, &y_win, &child);
 if (child != None)
     XTranslateCoordinates(dpy, win, child, x_win, y_win, &x_win, &y_win, &child);
 if (child != None)
 {
     button = XtWindowToWidget(dpy, child);
     awi->item = findAppItem(awi->win.aw, button);
 }
 else  awi->item = awi->win.aw->n_apps;
 XtVaGetValues(dc, XmNexportTargets, &exportTargets, XmNnumExportTargets, &numExportTargets, NULL);
 for (i=0; i < numExportTargets; i++)
 {
     if (exportTargets[i] == dragAtoms.appl)
     {
	 applDrop = True;
	 break;
     }
     if (objType == APPLICATION)
     {
	 for (j=0; j < usrObjects.nr; j++)
	 {
	     if (exportTargets[i] == usrObjects.obj[j].atom)
	     {
		 objType = j;
		 break;
	     }
	 }
     }
 }

 i = 0;
 if ((!applDrop && objType == APPLICATION) || awi->win.aw->blocked || dropData->dropAction != XmDROP || (dropData->operation != XmDROP_COPY && dropData->operation != XmDROP_MOVE && dropData->operation != XmDROP_LINK))
 {
     XtSetArg(args[i], XmNtransferStatus, XmTRANSFER_FAILURE); i++;
     XtSetArg(args[i], XmNnumDropTransfers, 0); i++;
     XTFREE(awi);
 }
 else
 {
     if (applDrop)
	 transEntry[0].target = dragAtoms.appl;
     else
	 transEntry[0].target = usrObjects.obj[objType].atom;
     transEntry[0].client_data = awi;
     if (dropData->operation == XmDROP_MOVE)
     {
	 transEntry[1].target = dragAtoms.delete;
	 transEntry[1].client_data = NULL;
	 XtSetArg(args[i], XmNnumDropTransfers, 2);
     }
     else  XtSetArg(args[i], XmNnumDropTransfers, 1);
      i++;
     XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
     XtSetArg(args[i], XmNtransferProc, appTransProc); i++;
 }
 XmDropTransferStart(dc, args, i);
}

/*-------------------------------------------------------------------------*/

void appTransProc(Widget w, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format)
{
 char *data = (char *) value;
 char *src = data;
 AppSpecRec *awi = (AppSpecRec *) client_data;
 AppList *applist;
 AppRec *app;
 int i, n_apps;

 if (*type == dragAtoms.appl)
 {
     write2buffer(&n_apps, src, sizeof(int));
     applist = (AppList *) XtMalloc(n_apps * sizeof(AppList));
     src += sizeof(int);
     for (i=0; i < n_apps; i++)
     {
	 applist[i] = app = (AppRec *) XtMalloc(sizeof(AppRec));
	 readDNDbuffer(&app->name, &src);
	 readDNDbuffer(&app->directory, &src);
	 readDNDbuffer(&app->fname, &src);
	 readDNDbuffer(&app->icon, &src);
	 readDNDbuffer(&app->push_action, &src);
	 readDNDbuffer(&app->drop_action, &src);
	 app->remove_file = False;
	 app->objType = APPLICATION;
     }
     insertNewApp(awi->win.aw, awi->item, applist, n_apps);
     XTFREE(awi);
 }
 else
 {
     for (i=0; i < usrObjects.nr; i++)
     {
	 if (*type == usrObjects.obj[i].atom)
	 {
	     makeObjectButton(awi, i, data, *length);
	     XTFREE(awi);
	     break;
	 }
     }
 }
}

/*-------------------------------------------------------------------------*/

void handlePushDrop(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmDropProcCallback dropData = (XmDropProcCallback) call_data;
 Widget dc = dropData->dragContext, shell;
 Atom *exportTargets;
 Cardinal numExportTargets;
 XmDropTransferEntryRec transEntry[2];
 AppDropRec *ad;
 AppSpecRec *awi;
 AppWindowRec *aw;
 DTIconRec *dticon;
 AppRec *app;
 FileTargetRec *fo;
 int objType = APPLICATION;
 Boolean blocked, is_dticon, applDrop = False, fileDrop = False, fileListDrop = False;
 Arg args[5];
 int i, j, item;

 if ((aw = findAppWidget(w, &item)))
 {
     app = &aw->apps[item];
     shell = aw->shell;
     blocked = aw->blocked;
     is_dticon = False;
 }
 else if ((dticon = findIcon(w)))
 {
     app = &dticon->app;
     shell = dticon->shell;
     blocked = False;
     is_dticon = True;
 }
 else  return;

 XtVaGetValues(dc, XmNexportTargets, &exportTargets, XmNnumExportTargets, &numExportTargets, NULL);
 for (i=0; i < numExportTargets; i++)
 {
     if (exportTargets[i] == dragAtoms.appl)
	 applDrop = True;
     else if (exportTargets[i] == dragAtoms.file)
     {
	 fileDrop = True;
	 applDrop = False;
	 objType = APPLICATION;
     }
     else if (exportTargets[i] == dragAtoms.filelist)
     {
	 fileListDrop = True;
	 applDrop = fileDrop = False;
	 objType = APPLICATION;
	 break;
     }
     else if (!applDrop && !fileDrop && objType == APPLICATION)
     {
	 for (j=0; j < usrObjects.nr; j++)
	 {
	     if (exportTargets[i] == usrObjects.obj[j].atom)
	     {
		 objType = j;
		 break;
	     }
	 }
     }
 }

 i = 0;
 if ((!fileListDrop && !applDrop && !fileDrop && objType == APPLICATION) || blocked || (is_dticon && !fileListDrop && !fileDrop) || dropData->dropAction != XmDROP || (dropData->operation != XmDROP_COPY && dropData->operation != XmDROP_MOVE && dropData->operation != XmDROP_LINK))
 {
     XtSetArg(args[i], XmNtransferStatus, XmTRANSFER_FAILURE); i++;
     XtSetArg(args[i], XmNnumDropTransfers, 0); i++;
 }
 else if (applDrop || objType != APPLICATION)
 {
     awi = (AppSpecRec *) XtMalloc(sizeof(AppSpecRec));
     awi->win.aw = aw;
     awi->item = item;
     if (applDrop)
	 transEntry[0].target = dragAtoms.appl;
     else
	 transEntry[0].target = usrObjects.obj[objType].atom;
     transEntry[0].client_data = awi;
     if (dropData->operation == XmDROP_MOVE)
     {
	 transEntry[1].target = dragAtoms.delete;
	 transEntry[1].client_data = NULL;
	 XtSetArg(args[i], XmNnumDropTransfers, 2);
     }
     else  XtSetArg(args[i], XmNnumDropTransfers, 1);
     i++;
     XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
     XtSetArg(args[i], XmNtransferProc, appTransProc); i++;
 }
 else
 {
#ifdef REFRESH_BUG
     UpdateRec *upd = (UpdateRec *) XtMalloc(sizeof(UpdateRec));
     if (is_dticon)
     {
	 upd->type = DESKTOP_ICON;
	 upd->win.dticon = dticon;
	 upd->label = app->label;
     }
     else
     {
	 upd->type = APPLICATION_WINDOW;
	 upd->win.aw = aw;
	 upd->label = None;
     }
     upd->form = app->form;
     upd->toggle = app->toggle;
#endif	/* REFRESH_BUG */
     if (fileListDrop || fileDrop)
     {
	 size_t dirlen = strlen(app->directory);
	 if (!(strcmp(app->drop_action, "COPY")))
	 {
	     fo = (FileTargetRec *) XtMalloc(sizeof(FileTargetRec));
	     fo->directory = (String) XtMalloc((dirlen + strlen(app->fname) + 2) * sizeof(char));
	     strcpy(fo->directory, app->directory);
	     if (fo->directory[dirlen - 1] != '/')
		 strcat(fo->directory, "/");
	     strcat(fo->directory, app->fname);
	     fo->free_dir = True;
	     fo->shell = shell;
	     fo->operation = dropData->operation;
#ifdef REFRESH_BUG
	     fo->upd = upd;
#endif
	     transEntry[0].target = (fileListDrop)? dragAtoms.filelist: dragAtoms.file;
	     transEntry[0].client_data = fo;
	     XtSetArg(args[i], XmNnumDropTransfers, 1); i++;
	     XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
	     XtSetArg(args[i], XmNtransferProc, fileTransProc); i++;
	 }
	 else
	 {
	     ad = (AppDropRec *) XtMalloc(sizeof(AppDropRec));
	     ad->directory = app->directory;
	     ad->shell = shell;
	     ad->drop_action = app->drop_action;
	     ad->first_arg = app->fname;
	     ad->icon_pm = app->icon_pm;
	     ad->free_drop = False;
#ifdef REFRESH_BUG
	     ad->upd = upd;
#endif
	     transEntry[0].target = (fileListDrop)? dragAtoms.filelist: dragAtoms.file;
	     transEntry[0].client_data = ad;
	     XtSetArg(args[i], XmNnumDropTransfers, 1); i++;
	     XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
	     XtSetArg(args[i], XmNtransferProc, pushTransProc); i++;
	 }
     }
 }
 XmDropTransferStart(dc, args, i);
}

/*-------------------------------------------------------------------------*/

void pushTransProc(Widget w, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format)
{
 char *data = (char *) value;
 AppDropRec *ad = (AppDropRec *) client_data;
 char directory[MAXPATHLEN];

 if (ad->directory[0])
 {
     strcpy(directory, ad->directory);
     fnexpand(directory);
 }
 else  directory[0] = 0;
 if (*type == dragAtoms.file)
 {
     String argv[2];
     int i = 0;

     if (ad->first_arg)
	 argv[i++] = XtNewString(ad->first_arg);
     argv[i++] = XtNewString(data);
     if (!directory[0])  strcpy(directory, user.home);
     performAction(ad->shell, ad->icon_pm.bm, ad->drop_action, directory, i, argv);
     if (ad->free_drop)  XTFREE(ad->drop_action);
     XTFREE(ad);
 }
 else if (*type == dragAtoms.filelist)
 {
     SelFileNamesRec *files;
     Boolean fullpath = False;
     size_t size;
     String *argv, relPath;
     int i, j = 0;

     files = filesFromBuffer(data, ad->shell);
     if (files->from_remote)
     {
	 error(ad->shell, "Dragged files reside on remote host.", NULL);
	 if (ad->free_drop)  XTFREE(ad->drop_action);
	 XTFREE(ad);
	 return;
     }
     if (!directory[0] || resources.passed_name_policy == FileName)
	 strcpy(directory, files->directory);
     else if (dirComp(directory, files->directory))
     {
	 fullpath = True;
	 if (resources.passed_name_policy == RelativePath)
	 {
	     relPath = relativePath(files->directory, directory);
	     XTFREE(files->directory);
	     files->directory = relPath;
	 }
     }

     argv = (String *) XtMalloc((files->n_sel + 1) * sizeof(String));
     if (ad->first_arg[0])
     {
	 if (resources.passed_name_policy == FileName && ad->directory[0])	/* i.e. we're in the wrong directory */
	 {
	     char path[MAXPATHLEN];

	     strcpy(path, ad->directory);
	     fnexpand(path);
	     if (path[strlen(path) - 1] != '/')
		 strcat(path, "/");
	     strcat(path, ad->first_arg);
	     argv[j++] = XtNewString(path);
	 }
	 else  argv[j++] = XtNewString(ad->first_arg);
     }
     for (i=0; i < files->n_sel; i++, j++)
     {
	 if (fullpath)
	 {
	     size = (strlen(files->directory) + strlen(files->names[i]) + 1) * sizeof(char);
	     argv[j] = (String) XtMalloc(size);
	     strcpy(argv[j], files->directory);
	     strcat(argv[j], files->names[i]);
	 }
	 else
	     argv[j] = XtNewString(files->names[i]);
     }
     performAction(ad->shell, ad->icon_pm.bm, ad->drop_action, directory, j, argv);

#ifdef REFRESH_BUG
     XtAppAddWorkProc(app_context, (XtWorkProc) widgetUpdate, (XtPointer) ad->upd);
#endif
     if (ad->free_drop)  XTFREE(ad->drop_action);
     XTFREE(argv);
     freeSelFiles(files);
     XTFREE(ad);
 }
}

/*-------------------------------------------------------------------------*/

SelFileNamesRec *filesFromBuffer(char *buffer, Widget shell)
{
 SelFileNamesRec *files = (SelFileNamesRec *) XtMalloc(sizeof(SelFileNamesRec));
 char *ptr;
 size_t len;
 int i;

 if (strcmp(buffer, user.hostname))
     files->from_remote = True;
 else files->from_remote = False;
 ptr = &buffer[strlen(buffer) + 1];
 len = strlen(ptr);
 if (ptr[len - 1] == '/')
     files->directory = XtNewString(ptr);
 else
 {
     files->directory = (String) XtMalloc((len + 2) * sizeof(char));
     strcpy(files->directory, ptr);
     strcat(files->directory, "/");
 }
 ptr = &ptr[len + 1];
 write2buffer(&files->n_sel, ptr, sizeof(int));
 files->first = 0;
 files->names = (String *) XtMalloc(files->n_sel * sizeof(String));
 ptr += sizeof(int);
 for (i=0; i < files->n_sel; i++)
 {
     files->names[i] = XtNewString(ptr);
     ptr += strlen(ptr) + 1;
 }
 files->target = NULL;
 files->conf_ovwr = resources.confirm_overwrite;
 files->update = False;
 files->shell = shell;
 files->op = 0;
 if (files->n_sel == 1)  files->icon.bm = None;
 else  files->icon = icons[FILES_BM];
 return files;
}

/*-------------------------------------------------------------------------*/

SelFileNamesRec *filesFromName(char *filename, Widget shell)
{
 SelFileNamesRec *files = (SelFileNamesRec *) XtMalloc(sizeof(SelFileNamesRec));
 int i, len = strlen(filename);

 files->n_sel = 1;
 files->first = 0;
 files->names = (String *) XtMalloc(sizeof(String));
 files->target = NULL;
 files->icon.bm = None;
 files->conf_ovwr = resources.confirm_overwrite;
 files->update = False;
 files->shell = shell;
 files->from_remote = False;
 files->op = 0;
 for (i = len-2; i>=0; i--)
 {
     if (filename[i] == '/')  break;
 }
 files->names[0] = XtNewString(&filename[i+1]);
 if (i < 0 )  files->directory = XtNewString("");
 else
 {
     files->directory = (String) XtMalloc((i + 2) * sizeof(char));
     strncpy(files->directory, filename, (size_t) i+1);
     files->directory[i+1] = 0;
 }
 return files;
}

/*-------------------------------------------------------------------------*/

AppList *applistFromFiles(FileWindowRec *fw)
{
 AppList *applist;
 AppRec *app;
 FileRec *file;
 SelectionRec *selected = &fw->selected;
 char push[MAXPATHLEN], drop[MAXPATHLEN + 3];
 int i;

 applist = (AppList *) XtMalloc(selected->n_sel * sizeof(AppList));
 for (i=0; i < selected->n_sel; i++)
 {
     applist[i] = app = (AppRec *) XtMalloc(sizeof(AppRec));
     file = fw->files[selected->file[i].nr];
     app->name = app->fname = file->name;
     app->directory = fw->directory;
     if (S_ISDIR(file->stats.st_mode))
     {
	 if (file->type)
	     app->icon = file->type->icon;
	 else
	     app->icon = "";
	 app->push_action = XtNewString("OPEN");
	 app->drop_action = XtNewString("COPY");
     }
     else if (permission(&file->stats, P_EXECUTE))
     {
	 if (file->type)  app->icon = file->type->icon;
	 else  app->icon = "";
	 strcpy(push, "exec ./");
	 strcpy(&push[7], app->fname);
	 app->push_action = XtNewString(push);
	 strcpy(drop, push);
	 strcat(drop, " $*");
	 app->drop_action = XtNewString(drop);
     }
     else if (file->type)
     {
	 app->icon = file->type->icon;
	 app->push_action = XtNewString(file->type->push_action);
	 if (!(strcmp(app->push_action, "LOAD")))
	     app->icon = resources.applbox_icon;
	 app->drop_action = XtNewString(file->type->drop_action);
     }
     else
     {
	 app->icon = "";
	 app->push_action = XtNewString("EDIT");
	 app->drop_action = XtNewString("");
     }
 }
 return applist;
}

/*-------------------------------------------------------------------------*/

void startFileDrag(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 Widget drag_icon, dc;
 FileSrcRec *fs = (FileSrcRec *) XtMalloc(sizeof(FileSrcRec));
 Atom targets[4];
 Arg args[15];
 int i;

 fs->dropTarget = None;
 fs->fw = findFileWidgetByForm(w);
 if (!fs->fw)
 {
     if (!(fs->fw = findFileWidget(w, &i)))
	 return;
     if (getSelNr(fs->fw, i) == -1)  fileSelect(fs->fw, i);
     if (fs->fw->files[i]->drop_site)
     {
	 fs->dropTarget = w;
	 XmDropSiteUpdate(w, drop_inactivate, XtNumber(drop_inactivate));
     }
 }
 if (!(fs->files = getSelFiles(fs->fw)))
 {
     XTFREE(fs);
     return;
 }

 icon = fs->files->icon;
 if (icon.bm != None)
 {
     i = 0;
     XtSetArg(args[i], XmNpixmap, icon.bm); i++;
     XtSetArg(args[i], XmNdepth, DefaultDepth(dpy, DefaultScreen(dpy))); i++;
/*     if (icon.mask != None) XtSetArg(args[i], XmNmask, icon.mask); i++;*/
     XtSetArg(args[i], XmNwidth, icon.width); i++;
     XtSetArg(args[i], XmNheight, icon.height); i++;
     drag_icon = XmCreateDragIcon(fs->fw->form, "drag_icon", args, i);
 }

 i=0;
 XtSetArg(args[i], XmNblendModel, XmBLEND_ALL); i++;
 XtSetArg(args[i], XmNcursorBackground, winInfo.background); i++;
 XtSetArg(args[i], XmNcursorForeground, resources.label_color); i++;
 if (icon.bm != None)
 {
     XtSetArg(args[i], XmNsourcePixmapIcon, drag_icon); i++;
 }
 XtSetArg(args[i], XmNdragOperations, XmDROP_COPY | XmDROP_MOVE | XmDROP_LINK); i++;
 XtSetArg(args[i], XmNconvertProc, fileConvProc); i++;
 XtSetArg(args[i], XmNexportTargets, targets); i++;
 targets[0] = dragAtoms.filelist;
 targets[1] = dragAtoms.appl;
 targets[2] = dragAtoms.file;
 targets[3] = dragAtoms.string;
 if (fs->files->n_sel == 1)
     XtSetArg(args[i], XmNnumExportTargets, 4);
 else
     XtSetArg(args[i], XmNnumExportTargets, 2);
 i++;
 XtSetArg(args[i], XmNclientData, (XtPointer) fs); i++;
 dc = XmDragStart(fs->fw->form, event, args, i);
 XtAddCallback(dc, XmNdragDropFinishCallback, fileDropFinish, NULL);
 fs->fw->drag_source = True;
 file_drag = True;
}

/*-------------------------------------------------------------------------*/

Boolean fileConvProc(Widget w, Atom *selection, Atom *target, Atom *type_return, XtPointer *value_return, unsigned long *length_return, int *format_return)
{
 FileSrcRec *fs;
 SelFileNamesRec *files;
 AppList *applist;
 Atom TARGETS = XmInternAtom(dpy, "TARGETS", False);
 String filename;
 Boolean res = False;
 int i;

 if (*selection != dragAtoms.drop)  return False;
 XtVaGetValues(w, XmNclientData, &fs, NULL);
 files = fs->files;
 if (!files)  return False;

 if (*target == dragAtoms.file)
 {
     filename = (String) XtMalloc((strlen(files->directory) + strlen(files->names[0]) + 2) * sizeof(char));
     strcpy(filename, files->directory);
     if (filename[strlen(filename)-1] != '/')
	 strcat(filename, "/");
     strcat(filename, files->names[0]);
     *length_return = strlen(filename) * sizeof(char);
     *type_return =dragAtoms.file;
     *value_return = DNDbuffer = filename;
     *format_return = 8;
     res = True;
 }
 else if (*target == dragAtoms.filelist)
 {
     char *ptr;
     size_t size;

     size = (strlen(user.hostname) + strlen(files->directory) + 2) * sizeof(char) + sizeof(int);
     for (i=0; i<files->n_sel; i++)
	 size += (strlen(files->names[i]) + 1) * sizeof(char);
     DNDbuffer = (char *) XtMalloc(size);
     strcpy(DNDbuffer, user.hostname);
     ptr = &DNDbuffer[strlen(DNDbuffer)] + 1;
     strcpy(ptr, files->directory);
     ptr = &ptr[strlen(ptr)] + 1;
     write2buffer(ptr, &files->n_sel, sizeof(int));
     ptr += sizeof(int);
     for (i=0; i < files->n_sel; i++)
     {
	 strcpy(ptr, files->names[i]);
	 ptr += strlen(ptr) + 1;
     }
     *length_return = size;
     *type_return = dragAtoms.filelist;
     *value_return = DNDbuffer;
     *format_return = 8;
     res = True;
 }
 else if (*target == dragAtoms.appl)
 {
     applist = applistFromFiles(fs->fw);
     *length_return = fillDNDbuffer(applist, files->n_sel) * sizeof(char);
     *type_return =dragAtoms.appl;
     *value_return = DNDbuffer;
     *format_return = 8;
     res = True;
     for (i=0; i < files->n_sel; i++)
     {
	 XTFREE(applist[i]->push_action);
	 XTFREE(applist[i]->drop_action);
	 XTFREE(applist[i]);
     }
     XTFREE(applist);
 }
 else if (*target == dragAtoms.string)
 {
     FILE *fin;
     struct stat stats;
     size_t size;

     if (chdir(files->directory))
	 res = False;
     else if (stat(files->names[0], &stats) || (stats.st_mode & S_IFMT) != S_IFREG || (size = stats.st_size) > MAXDNDSTRINGLEN)
	 res = False;
     else if (!(fin = fopen(files->names[0], "r")))
	 res = False;
     else if (!(DNDbuffer = (char *) XtMalloc((stats.st_size) * sizeof(char))))
     {
	 fclose(fin);
	 res = False;
     }
     else if (fread(DNDbuffer, sizeof(char), size, fin) != size)
     {
	 XTFREE(DNDbuffer);
/*	 DNDbuffer = NULL;*/
	 fclose(fin);
	 res = False;
     }
     else
     {
	 Boolean is_ascii = True;
	 fclose(fin);
	 for (i=0; i<1024 && i<size; i++)
	 {
	     if (!(DNDbuffer[i]) || !isascii(DNDbuffer[i]))
	     {
		 is_ascii = False;
		 break;
	     }
	 }
	 if (is_ascii)
	 {
	     *length_return = size;
	     *type_return = dragAtoms.string;
	     *value_return = DNDbuffer;
	     *format_return = 8;
	     res = True;
	 }
	 else
	 {
	     XTFREE(DNDbuffer);
	     res = False;
	 }
     }
     chdir(user.home);
 }
 else if (*target == TARGETS)	/* This target is required by ICCC */
 {
     Atom MULTIPLE = XmInternAtom(dpy, "MULTIPLE", False);
     Atom TIMESTAMP = XmInternAtom(dpy, "TIMESTAMP", False);
     Atom *targs = (Atom *) XtMalloc(6*sizeof(Atom));;

     targs[0] = dragAtoms.filelist;
     targs[1] = dragAtoms.file;
     targs[2] = dragAtoms.string;
     targs[3] = TARGETS;
     targs[4] = MULTIPLE;	/* supported in the Intrinsics */
     targs[5] = TIMESTAMP;	/* supported in the Intrinsics */
     *type_return = XA_ATOM;
     *value_return = targs;
     *length_return = 5 * sizeof(Atom) >> 2;
     *format_return = 8 * sizeof(XtPointer);
     res = True;
 }

 return res;
}

/*-------------------------------------------------------------------------*/

void fileDropFinish(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileSrcRec *fs;
 Widget drag_icon = NULL;

 XtVaGetValues(w, XmNsourcePixmapIcon, &drag_icon, XmNclientData, &fs, NULL);
 if (drag_icon)
     XtDestroyWidget(drag_icon);
 if (fs->dropTarget != None)
     XmDropSiteUpdate(fs->dropTarget, drop_activate, XtNumber(drop_activate));
 fs->fw->drag_source = False;
 file_drag = False;
 if (fs->fw->update)  markForUpdate(fs->fw->directory, CHECK_FILES);
 intUpdate(CHECK_DIR);
 freeSelFiles(fs->files);
 XTFREE(fs);
}

/*-------------------------------------------------------------------------*/

void handleFileWinDrop(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmDropProcCallback dropData = (XmDropProcCallback) call_data;
 Widget dc = dropData->dragContext;
 Atom *exportTargets;
 Cardinal numExportTargets;
 XmDropTransferEntryRec transEntry[2];
 FileTargetRec *fo = (FileTargetRec *) XtMalloc(sizeof(FileTargetRec));
 FileWindowRec *fw;
 Boolean fileDrop = False, fileListDrop = False;
 Arg args[5];
 int i;

 XtVaGetValues(dc, XmNexportTargets, &exportTargets, XmNnumExportTargets, &numExportTargets, NULL);
 for (i=0; i < numExportTargets; i++)
 {
     if (exportTargets[i] == dragAtoms.file)
	 fileDrop = True;
     else if (exportTargets[i] == dragAtoms.filelist)
     {
	 fileListDrop = True;
	 fileDrop = False;
	 break;
     }
 }
 if (!(fw = findFileWidgetByForm(w)))
     return;

 i = 0;
 if ((!fileListDrop && !fileDrop) || fw->drag_source || !permission(&fw->stats, P_WRITE) || dropData->dropAction != XmDROP || (dropData->operation != XmDROP_COPY && dropData->operation != XmDROP_MOVE && dropData->operation != XmDROP_LINK))
 {
     XTFREE(fo);
     XtSetArg(args[i], XmNtransferStatus, XmTRANSFER_FAILURE); i++;
     XtSetArg(args[i], XmNnumDropTransfers, 0); i++;
 }
 else
 {
     fo->directory = fw->directory;
     fo->free_dir = False;
     fo->shell = fw->shell;
     fo->operation = dropData->operation;
#ifdef REFRESH_BUG
     fo->upd = (UpdateRec *) XtMalloc(sizeof(UpdateRec));
     fo->upd->win.fw = fw;
     fo->upd->type = FILE_WINDOW;
     fo->upd->form = w;
     fo->upd->toggle = fo->upd->label = None;
#endif

     transEntry[0].target = (fileListDrop)? dragAtoms.filelist : dragAtoms.file;
     transEntry[0].client_data = fo;
     XtSetArg(args[i], XmNnumDropTransfers, 1); i++;
     XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
     XtSetArg(args[i], XmNtransferProc, fileTransProc); i++;
 }
 XmDropTransferStart(dc, args, i);
}

/*-------------------------------------------------------------------------*/

void fileTransProc(Widget w, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format)
{
 char *data = (char *) value;
 FileTargetRec *fo = (FileTargetRec *) client_data;
 SelFileNamesRec *files = NULL;

 if (*type == dragAtoms.filelist)
     files = filesFromBuffer(data, fo->shell);
 else if (*type == dragAtoms.file)
     files = filesFromName(data, fo->shell);

 if (files)
 {
     if (files->from_remote)
	 error(fo->shell, "Sorry!", "Copying files from remote host is not yet implemented.");
     else
     {
	 if (fo->free_dir)  files->target = fo->directory;
	 else  files->target = XtNewString(fo->directory);

	 switch (fo->operation)
	 {
	     case XmDROP_COPY:	files->op = COPY; break;
	     case XmDROP_MOVE:	files->op = MOVE; break;
	     case XmDROP_LINK:	files->op = LINK;
	 }
	 moveFilesDialog(files);
     }
#ifdef REFRESH_BUG
     XtAppAddWorkProc(app_context, (XtWorkProc) widgetUpdate, (XtPointer) fo->upd);
#endif
     XTFREE(fo);
 }
}

/*-------------------------------------------------------------------------*/

void handleFileDrop(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmDropProcCallback dropData = (XmDropProcCallback) call_data;
 Widget dc = dropData->dragContext;
 Atom *exportTargets;
 Cardinal numExportTargets;
 XmDropTransferEntryRec transEntry[2];
 FileTargetRec *fo = (FileTargetRec *) XtMalloc(sizeof(FileTargetRec));
 FileWindowRec *fw;
 FileRec *file;
 Boolean fileDrop = False, fileListDrop = False;
 Arg args[5];
 int i;

 XtVaGetValues(dc, XmNexportTargets, &exportTargets, XmNnumExportTargets, &numExportTargets, NULL);
 for (i=0; i < numExportTargets; i++)
 {
     if (exportTargets[i] == dragAtoms.file)
	 fileDrop = True;
     else if (exportTargets[i] == dragAtoms.filelist)
     {
	 fileListDrop = True;
	 fileDrop = False;
	 break;
     }
 }
 if (!(fw = findFileWidget(w, &i)))
     return;
 file = fw->files[i];

 i = 0;
 if ((!fileListDrop && !fileDrop) || dropData->dropAction != XmDROP || (dropData->operation != XmDROP_COPY && dropData->operation != XmDROP_MOVE && dropData->operation != XmDROP_LINK))
 {
     XTFREE(fo);
     XtSetArg(args[i], XmNtransferStatus, XmTRANSFER_FAILURE); i++;
     XtSetArg(args[i], XmNnumDropTransfers, 0); i++;
 }
 else
 {
#ifdef REFRESH_BUG
     UpdateRec *upd = (UpdateRec *) XtMalloc(sizeof(UpdateRec));
     upd->win.fw = fw;
     upd->type = FILE_WINDOW;
     upd->form = w;
     upd->toggle = upd->label = None;
#endif
     if (S_ISDIR(file->stats.st_mode))
     {
	 size_t len = strlen(fw->directory);

	 fo->directory = (String) XtMalloc((len + strlen(file->name) + 2) * sizeof(char));
	 strcpy(fo->directory, fw->directory);
	 if (fo->directory[len - 1] != '/')
	     strcat(fo->directory, "/");
	 strcat(fo->directory, file->name);
	 fo->free_dir = True;
	 fo->shell = fw->shell;
	 fo->operation = dropData->operation;
#ifdef REFRESH_BUG
	 fo->upd = upd;
#endif
	 transEntry[0].target = (fileListDrop)? dragAtoms.filelist : dragAtoms.file;
	 transEntry[0].client_data = fo;
	 XtSetArg(args[i], XmNnumDropTransfers, 1); i++;
	 XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
	 XtSetArg(args[i], XmNtransferProc, fileTransProc); i++;
     }
     else if (file->type && file->type->drop_action[0])
     {
	 AppDropRec *ad = (AppDropRec *) XtMalloc(sizeof(AppDropRec));

	 ad->directory = fw->directory;
	 ad->shell = fw->shell;
	 ad->drop_action = file->type->drop_action;
	 ad->free_drop = False;
	 ad->first_arg = file->name;
	 ad->icon_pm = file->icon;
#ifdef REFRESH_BUG
	 ad->upd = upd;
#endif

	 transEntry[0].target = (fileListDrop)? dragAtoms.filelist : dragAtoms.file;
	 transEntry[0].client_data = ad;
	 XtSetArg(args[i], XmNnumDropTransfers, 1); i++;
	 XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
	 XtSetArg(args[i], XmNtransferProc, pushTransProc); i++;
     }
     else	/* This should mean file is executable */
     {
	 AppDropRec *ad = (AppDropRec *) XtMalloc(sizeof(AppDropRec));

	 ad->directory = fw->directory;
	 ad->shell = fw->shell;
	 ad->drop_action = (String) XtMalloc((strlen(file->name) + 6) * sizeof(char));
	 sprintf(ad->drop_action, "./%s $*", file->name);
	 ad->free_drop = True;
	 ad->first_arg = "";
	 ad->icon_pm.bm = None;
#ifdef REFRESH_BUG
	 ad->upd = upd;
#endif
	 transEntry[0].target = (fileListDrop)? dragAtoms.filelist : dragAtoms.file;
	 transEntry[0].client_data = ad;
	 XtSetArg(args[i], XmNnumDropTransfers, 1); i++;
	 XtSetArg(args[i], XmNdropTransfers, transEntry); i++;
	 XtSetArg(args[i], XmNtransferProc, pushTransProc); i++;
     }
 }
 XmDropTransferStart(dc, args, i);
}

/*-------------------------------------------------------------------------*/

#ifdef REFRESH_BUG

static Boolean widgetUpdate(UpdateRec *upd)
{
 AppWindowRec *awi = app_windows;
 FileWindowRec *fwi = file_windows;
 int i;
 Boolean exists = False;

 switch (upd->type)
 {
     case FILE_WINDOW:
	 while (fwi && fwi != upd->win.fw)  fwi = fwi->next;
	 if (fwi)  exists = True;
	 break;
     case APPLICATION_WINDOW:
	 while (awi && awi != upd->win.aw)  awi = awi->next;
	 if (awi)  exists = True;
	 break;
     default:
	 for (i=0; i < n_dtIcons; i++)
	 {
	     if (dtIcons[i] == upd->win.dticon)
	     {
		 exists = True;
		 break;
	     }
	 }
 }

 if (exists)
 {
     if (upd->form != None && XtIsWidget(upd->form))
	 XClearArea(dpy, XtWindow(upd->form), 0, 0, 0, 0, True);
     if (upd->toggle != None && XtIsWidget(upd->toggle))
	 XClearArea(dpy, XtWindow(upd->toggle), 0, 0, 0, 0, True);
     if (upd->label != None && XtIsWidget(upd->label))
	 XClearArea(dpy, XtWindow(upd->label), 0, 0, 0, 0, True);
 }

 XTFREE(upd);
 return True;
}

#endif
