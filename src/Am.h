/*-----------------------------------------------------------------------------
  Am.h
  
  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
  (c) Oliver Mai 1995
-----------------------------------------------------------------------------*/

#ifndef AM_H
#define AM_H

#include <sys/param.h>
#include "Common.h"

/*--FmMain---------------------------------------------------------------------*/

void getRootGeom(Widget w);
void quit(XtPointer, int save);

/*--FmAw---------------------------------------------------------------------*/

#define MAXAPPSTRINGLEN MAXPATHLEN
#define ERR_POPUP 0
#define STDERR 1
#define DTICON -2

typedef struct
{
    char *name;
    char *directory;
    char *fname;
    char *icon;
    char *push_action;
    char *drop_action;
    int objType;
    IconRec icon_pm;
    Boolean loaded, remove_file;
    Widget form, toggle, label;
} AppRec, *AppList;

typedef struct _AppWindowRec
{
    char appfile[MAXPATHLEN];
    Widget shell, form, icon_box;
    Widget appboxPopup, buttonPopup;
    AppList apps;
    int n_apps;
    Boolean iconic, iconBoxCreated, modified, blocked, update, drag_source,
	readable;
    struct stat stats;
    struct _AppWindowRec *next;
} AppWindowRec, *AppWindowList;

typedef struct
{
    AppRec app;
    Pixmap label_mask, drop_pixmap;
    Widget shell, cont, popup;
    Dimension x, y, width, height;
    Boolean moved, save;
}  DTIconRec;

typedef struct
{
    union { AppWindowRec *aw; DTIconRec *dticon; } win;
    int item;
} AppSpecRec;

typedef struct
{
    AppWindowRec *aw;
    AppRec *app;
} NewAppRec;

typedef struct
{
    AppWindowRec *aw;
    String name;
    IconRec icon;
} NewAppBoxRec;
 
extern AppWindowList app_windows;

void atomInit(void);
IconRec defaultIcon(char *name, char *directory, char *fname);
void setTitle(AppWindowRec *aw);
AppWindowRec *createApplicationWindow(String newpath, FILE *fin, GeomRec *geom);
AppWindowRec *newApplicationWindow(String newpath, FILE *fin);
int saveApplicationWindow(AppWindowRec *aw, FILE *fout);
int readApplicationWindow(AppWindowRec *aw, GeomRec *geom, FILE *fin);
void closeApplicationCB(Widget w, XtPointer client_data, XtPointer call_data);
void closeDialog(AppWindowRec *aw);
void removeAppDialog(AppWindowRec *aw, int i);
void overwriteAppsDialog(AppWindowRec *aw);
void removeProc(XtPointer awp, int save);
void exitProc(XtPointer, int answer);
void closeProc(XtPointer awp, int save);
void changeIconProc(XtPointer awi, int conf);
void newAppProc(XtPointer nar, int conf);
void newAppBoxProc(XtPointer apr, int conf);
void changeAppProc(XtPointer chs, int save);
void writeApplicationDataProc(XtPointer data, int conf);
void createApplicationDisplay(AppWindowRec *aw);
Dimension setApplGeom(AppWindowRec *aw);
void resizeAppl(Widget icon_box, XtPointer client_data, XEvent *event);
void appIconifyHandler(Widget shell, XtPointer client_data, XEvent *event);
Boolean updateApplicationDisplay(AppWindowRec *aw);
void appUpdate(void);
void readApplicationIcon(Widget shell, AppRec *app, int errDispType);
void readApplicationData(AppWindowRec *aw);
void writeApplicationData(AppWindowRec *aw);
void overwriteApplicationData(AppWindowRec *aw);
void insertNewApp(AppWindowRec *aw, int i, AppList *applist, int n_apps);
void freeApplicationResources(AppRec *app);

/*--FmAwCb-------------------------------------------------------------------*/

void appEdit(AppSpecRec *awi);
void usrObjEdit(AppSpecRec *awi);
void buttonPopup(Widget w, XEvent *event, String *params, Cardinal *num_params);

FmCallbackProc 
  appInstallCb, appOpenCb, appEditCb, appRemoveCb, appSaveCb, appLoadCb, appOpenFileWinCb, appCloseCb, appboxNewCb, appExitCb, appEditCfgCb;


/*--FmAwActions--------------------------------------------------------------*/

typedef struct
{
    AppWindowRec *aw;
    String newpath;
} change_app_path_struct;

int findAppItem(AppWindowRec *aw, Widget button);
int findAppItemByName(AppWindowRec *aw, String name);
AppWindowRec *findAppWidget(Widget w, int *i);
AppWindowRec *findAppWidgetByForm(Widget form);

void openApp(Widget w, Boolean newAppWin);
void runApp(Widget w, XtPointer client_data, XtPointer call_data);
void appEndMove(int i);

/*--FmAwPopup----------------------------------------------------------------*/

void installNewPopup();
void installExistingPopup();
void createInstallPopup();

/*--FmDragDrop-------------------------------------------------------------------*/

Boolean appConvProc(Widget w, Atom *selection, Atom *target, Atom *type_return, XtPointer *value_return, unsigned long *length_return, int *format_return);
void appDropFinish(Widget w, XtPointer client_data, XtPointer call_data);
void startAppDrag(Widget w, XEvent *event, String *params, Cardinal *num_params);
void handleAppDrop(Widget w, XtPointer client_data, XtPointer call_data);
void appTransProc(Widget w, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format);
void handlePushDrop(Widget w, XtPointer client_data, XtPointer call_data);
void pushTransProc(Widget w, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format);
void startDTIconDrag(Widget w, XEvent *event, String *params, Cardinal *num_params);

/*--FmAwObjects-------------------------------------------------------------------*/

typedef struct
{
    Atom atom;
    String atom_name, push_action, label, icon, drag_icon;
} UserObjectRec;

typedef struct
{
    int nr;
    UserObjectRec *obj;
} UserObjectList;

extern UserObjectList usrObjects;

#define APPLICATION -1

void readUserObjects(void);
int getObjectType(String push_action);
String getPushAction(int objType);
void makeObjectButton(AppSpecRec *awi, int objType, char *buffer, unsigned long len);
int dupObject(Widget shell, AppRec *dest, AppRec *src);

/*--FmIcons--------------------------------------------------------------*/

extern DTIconRec **dtIcons;
extern n_dtIcons;

void appOnDeskCb(Widget w, XtPointer client_data, XtPointer call_data);
void removeIconProc(XtPointer awp, int conf);
void changeDTIconProc(XtPointer awp, int conf);
void writeDTIconsProc(XtPointer allp, int conf);
DTIconRec *findIcon(Widget w);
void writeDTIcons(Boolean all);
void overwriteDTIcons(Boolean all);
void readDTIcons(void);
void updateIconDisplay(void);
/* This one is implemented in FmDialogs.c: */
void removeIconDialog(DTIconRec *dticon);
void overwriteIconsDialog(Boolean all);

/*--FmMonitor--------------------------------------------------------------*/

extern int n_monacts;

void initMonitor(void);
void freeMonacts(void);
void appTimeoutCb(XtPointer data, XtIntervalId *id);

#endif
