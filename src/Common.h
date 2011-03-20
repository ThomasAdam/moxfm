/*
    This is a minor update to moxfm 1.0 to allow it to compile on more recent
    GNU/Linux/GCC systems without warnings that the getwd function is dangerous
    and should not be used. Caveat emptor and YMMV.
    - Robert Woodside,  14 August, 2001
*/

/*-----------------------------------------------------------------------------
  Common.h
  
  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

 declarations and definitions included by all modules
-----------------------------------------------------------------------------*/

#ifndef COMMON_H
#define COMMON_H

#ifdef DEBUG_MALLOC
#include <malloc.h>
#endif

#ifdef _AIX
#pragma alloca
#else
#ifdef hpux
#pragma alloca
#else
#ifdef __NetBSD__
#pragma alloca
#else
#include <alloca.h>
#endif	/* __NetBSD__ */
#endif	/* hpux */
#endif	/* _AIX */

#include <stdio.h>
#include <sys/types.h> /* just in case */
#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>
#include <unistd.h>
#include <Xm/Xm.h>
#include <X11/xpm.h>

#ifdef DMALLOC
#include <dmalloc.h>

#ifdef XtMalloc
#undef XtMalloc
#endif
#define XtMalloc malloc
#ifdef XtRealloc
#undef XtRealloc
#endif
#define XtRealloc realloc
#ifdef XtFree
#undef XtFree
#endif
#define XtFree free

#endif   /* DMALLOC */

/* some systems define SVR4 but not SYSV */
#ifdef SVR4
#ifndef SYSV
#define SYSV
#endif
#endif

/*  RBW - 2001/08/14  */
#ifdef _SVID_SOURCE
#define SYSV
#endif

#ifdef SYSV
#define getwd(s) getcwd(s,MAXPATHLEN)
#endif

/* ULTRIX apparently doesn't define these */
#ifdef ultrix
#define S_ISLNK(mode) (mode & S_IFMT) == S_IFLNK
#define S_ISSOCK(mode) (mode & S_IFMT) == S_IFSOCK
#endif

/* for compatibility with BSDI */
#define fnmatch xfnmatch

/* The following is necessary on the only HP UX box I have access to.
Don't know if it is required on all those machines. OM */
#ifdef hpux
#ifdef FILENAME_MAX
#undef FILENAME_MAX
#endif
#endif

#ifndef FILENAME_MAX
#define FILENAME_MAX 1024
#endif

extern Widget topShell;

/*---FmExec---------------------------------------------------------------*/

void doEdit(char *directory, char *fname);
void doView(char *directory, char *fname);

/*---FmBitmaps---------------------------------------------------------------*/

/* File pixmaps */

#define FILES_BM 0
#define DIR_BM 1
#define UPDIR_BM 2
#define FILE_BM 3
#define EXEC_BM 4
#define SYMLNK_BM 5
#define DIRLNK_BM 6
#define EXECLNK_BM 7
#define BLACKHOLE_BM 8

/* Application icons: */

#define ICON_BM 9
#define APPMGR_BM 10
#define APPBOX_BM 11
#define DISK_BM 12

/* Pushbutton icons: */

#define GLASSES_BM 13
#define HOME_BM 14
#define NEWFILEWIN_BM 15
#define NEWAPPWIN_BM 16
#define GOUP_BM 17
#define BACKARR_BM 18

/* Hardcoded bitmaps stop here: */

#define END_BM 19

#define freePixmap(icon)  XtAppAddWorkProc(app_context, (XtWorkProc) freePixmapProc, (XtPointer) icon)

typedef struct
{
    Pixmap bm, mask;
    Dimension width, height;
} IconRec;

extern Cursor cur;
extern IconRec *icons;

void readBitmaps(void);
IconRec readIcon(Widget shell, char *name);
void freeIcon(IconRec icon);
Boolean freePixmapProc(Pixmap icon_bm);

/*--FmDialogs----------------------------------------------------------------*/

#define YES 0
#define CANCEL 1
#define NO 2
#define ALL 3

typedef struct
{
    String label;
    String *value;
} TextFieldRec, *TextFieldList;

void confirm(Widget shell, String question, void (*answerHandlerCB)(XtPointer, int), XtPointer client_data);
void ask(Widget shell, String question, void (*answerHandlerCB)(XtPointer, int), XtPointer client_data);
void saveDialog(Widget shell, XtPointer client_data);
void lastSaveDialog(Widget shell);
void exitDialog(Widget shell);
void textFieldDialog(Widget shell, TextFieldList fields, void (*callback)(XtPointer, int), XtPointer callback_data, Pixmap icon);
void closeFileCB(Widget w, XtPointer client_data, XtPointer call_data);

/*--FmErrors-----------------------------------------------------------------*/

void error(Widget shell, String line1, String line2);
void sysError(Widget shell, String label);
void abortXfm(String message);

/*--FmExec-------------------------------------------------------------------*/

typedef struct {
  String pattern, command;
} ExecMapRec;

extern ExecMapRec *exec_map;
extern int n_exec_maps;

char **makeArgv(char *action, int n_args, String *arguments);
char **expandArgv(char **argv);
void freeArgv(char **argv);
void executeApplication(char *path, char *directory, char **argv);

/*--FmMain-------------------------------------------------------------------*/

#define MAXHOSTLEN 65

/* Structure containing information about the user */
typedef struct {
  int uid, gid;
  char *name;
  char home[MAXPATHLEN];
  char shell[MAXPATHLEN];
  char hostname[MAXHOSTLEN];
  int arg0flag;
  mode_t umask;
} UserInfo;

extern Boolean little_endian;
extern String version;

Boolean startup(void);
void noWarnings(String);

/*--FmFw-------------------------------------------------------------------*/

/* enumerated arguments passed to functions */
typedef enum { Files, Directories, All } FilterType;
typedef enum { SortByName, SortBySize, SortByMTime } SortType;
typedef enum { /*Tree,*/ Icons, Text } DisplayType;
typedef enum { Up, Down, Left, Right } Direction;

typedef void FmCallbackProc(Widget w, XtPointer client_data, XtPointer call_data);
typedef void FmActionProc(Widget w, XEvent *event, String *params, Cardinal *num_params);

/*--FmDragDrop-------------------------------------------------------------------*/

typedef enum { FileName, RelativePath, AbsolutePath } NamePolicyType;

/*--Resources-------------------------------------------------------------------*/

typedef struct {
  Boolean appmgr, filemgr, ignore_startup, version;
  String init_app_geometry, init_file_geometry, app_geometry, file_geometry, icon_pos;
  XmFontList icon_font, button_font, menu_font, label_font, status_font, bold_font, cell_font, tty_font, filename_font;
  int app_icon_width, app_icon_height, file_icon_width, file_icon_height,
    tree_icon_width, tree_icon_height;
  String app_mainfile_r, cfg_file_r, mon_file_r, dev_file_r, appcfg_path_r, start_dir_r, startup_file_r, obj_dir_r, usrobj_file_r, dticon_file_r, applbox_icon_r;
  String bitmap_path_r, pixmap_path_r;
#ifdef MAGIC_HEADERS
  String magic_file_r, nomgc_cfg_file_r;
#endif
  Boolean confirm_deletes, confirm_delete_folder, confirm_moves, confirm_copies, confirm_links, confirm_overwrite, confirm_quit;
  Boolean echo_actions;
  SortType default_sort_type;
  DisplayType initial_display_type;
  NamePolicyType passed_name_policy;
  Direction icon_align;
  Boolean start_from_xterm, keep_xterm;  Boolean show_hidden, dirs_first, show_owner, show_perms, show_date, show_length, newwin_on_dirpopup, override_redirect;
  char cfg_file[MAXPATHLEN], mon_file[MAXPATHLEN], app_mainfile[MAXPATHLEN], dev_file[MAXPATHLEN], dticon_file[MAXPATHLEN], appcfg_path[MAXPATHLEN], start_dir[MAXPATHLEN], obj_dir[MAXPATHLEN], usrobj_file[MAXPATHLEN], startup_file[MAXPATHLEN], bitmap_path[MAXPATHLEN], pixmap_path[MAXPATHLEN], applbox_icon[MAXPATHLEN];
#ifdef MAGIC_HEADERS
  char magic_file[MAXPATHLEN], nomgc_cfg_file[MAXPATHLEN];
#endif
  Boolean auto_save, check_files, check_application_files, copy_info,
      save_on_exit, suppress_warnings;
  int double_click_time, update_interval;
  String default_editor, default_viewer, xterm, sh_list;
  Pixel label_color, select_color, drop_color, filename_color, linkname_color, textdir_color, dticon_color;
} Resources;

typedef struct
{
    unsigned int rootWidth, rootHeight;
    Dimension appXPos, appYPos, fileXPos, fileYPos;
    int appWidth, appHeight, fileWidth, fileHeight;
    Pixel background;
} XPropRec;

typedef struct
{
    Atom appl;
    Atom file;
    Atom filelist;
    Atom string;
    Atom delete;
    Atom drop;
} AtomRec;

extern char *progname;
extern Resources resources;
extern XtAppContext app_context;
extern UserInfo user;
extern Display *dpy;
extern XPropRec winInfo;
extern AtomRec dragAtoms;
extern Atom wm_delete_window;

/*---FmFwActions-------------------------------------------------------------*/

void raiseIconsCb(Widget w, XtPointer client_data, XtPointer call_data);
void saveIconsCb(Widget w, XtPointer client_data, XtPointer call_data);

/*---FmFwActions-------------------------------------------------------------*/

void newAppWinCb(Widget w, XtPointer client_data, XtPointer call_data);
void mountTableCb(Widget w, XtPointer client_data, XtPointer call_data);

/*--FmMount---------------------------------------------------------------------*/

typedef struct
{
    String label, special, def_mpoint, mpoint;
    String mount_act, umount_act;
    Boolean slow_dev, mounted, manmounted, cp_req;
    int n_req;
    Widget state, button;
    Boolean sensitive;
} DeviceRec;

typedef struct
{
    int n_dev;
    DeviceRec *devices;
    Widget dialog;
    int x, y;
    Boolean iconic, show, slow_dev;
} MountRec;

extern MountRec mntable;

/*--FmMenus------------------------------------------------------------------*/

/* structures containing information required to set up a menu */
typedef struct _MenuItemRec
{
    char *label;
    WidgetClass *class;
    char mnemonic;
    char *accelerator;
    char *accel_text;
    void (*callback)(Widget, XtPointer, XtPointer);
    XtPointer callback_data;
    struct _MenuItemRec *subitems;
    Widget object;
} MenuItemRec, *MenuItemList;

Widget BuildMenu(Widget parent, int type, char *title, char mnemonic, Boolean tear_off, MenuItemList items);

/* structures containing information required to set up a button */
typedef struct {
  String button_name;
  String button_label;
  FmCallbackProc *callback;
} ButtonRec, *ButtonList;


/* structure for creating a popup questionaire */
typedef struct {
  String label;
  String value;
  Cardinal length;
  Widget widget;
} QuestionRec, *QuestionList;

void zzz(void), wakeUp(void);

/*--FmOps--------------------------------------------------------------------*/

char *split(char *s, char c);
char *expand(char *s, char *t, char *c);
char *strparse(char *s, char *t, char *c);
Boolean fnmultimatch(String pattern, String word);
int fnmatch(String pattern, String name);
char *fnexpand(char *fn);
int prefix(char *s, char *t);
int exists(char *path);
char *searchPath(char *s1, char *p, char *s2);
int create(char *path, mode_t mode);

/*--FmUtils--------------------------------------------------------------------*/

#ifdef NO_MEMMOVE
void *memmove(void *s1, const void *s2, size_t n);
#endif

typedef struct
{
    Dimension x, y, width, height;
    Boolean iconic;
} GeomRec;

int writeGeometry(Widget shell, Boolean iconic, FILE *fout);
int readGeometry(GeomRec *geom, FILE *fin);
int saveWindows(void);
void saveStartupCb(Widget w, XtPointer client_data, XtPointer call_data);
void performAction(Widget shell, Pixmap icon_bm, String action, String directory, int n_args, String *arguments);
Boolean lastWindow(void);
Widget getAnyShell(void);
int percent(long part, long whole);

/*--FmMonitor--------------------------------------------------------------*/

void reinitMonitor(void);
void doMonitor(void);

/*---------------------------------------------------------------------------*/

/* Horrible kludge to avoid warnings, as XtFree is designed to take a (char *)*/
#define XTFREE(p) XtFree((void *)(p))
#define XTREALLOC(p,n) XtRealloc((void *)(p),(n))

#endif
