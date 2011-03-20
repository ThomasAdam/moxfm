/*---------------------------------------------------------------------------
 Module FmMain

 (c) S. Marlow 1990-92
 (c) A. Graef 1994, 1996
 (c) R. Vogelgesang 1994 (`*BourneShells' stuff)
 (c) O. Mai 1995, 1996 (Motif "port")

 main module for file manager    
---------------------------------------------------------------------------*/

#define MOXFMVERSION "1.0.1"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef SVR4
#include <sys/systeminfo.h>
#endif

#ifdef _AIX
#include <sys/resource.h>
#endif

#include <sys/wait.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <Xm/AtomMgr.h>
#include <Xm/RepType.h>

#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
#include <X11/Xmu/Editres.h>
#endif
#endif

#include "Am.h"
#include "Fm.h"

#define XmRDisplayType "DisplayType"
#define XmRSortType "SortType"
#define XmRDirectionType "Direction"
#define XmRNamePolicyType "NamePolicy"
#define ZERO    ((Cardinal)0)

/*---------------------------------------------------------------------------
  Public variables
---------------------------------------------------------------------------*/

/* program name */
char *progname;

/* information about the user */
UserInfo user;

/* application resource values */
Resources resources;

/* application context */
XtAppContext app_context;

/* Top level shell */
Widget topShell;

/* Display */
Display *dpy;

/* Atoms */
Atom wm_delete_window;
AtomRec dragAtoms;

/* Endianness of the system */
Boolean little_endian;

/* Moxfm version number */
String version = MOXFMVERSION;
/*---------------------------------------------------------------------------
  Command line options
---------------------------------------------------------------------------*/

static XrmOptionDescRec options[] = {
  { "-appmgr", ".appmgr", XrmoptionNoArg, "True" },
  { "-filemgr", ".filemgr", XrmoptionNoArg, "True" },
  { "-dir", ".startDir", XrmoptionSepArg, NULL },
  { "-ignorestartup", ".ignoreStartup", XrmoptionNoArg, "False" },
  { "-version", ".version", XrmoptionNoArg, "True" }
};

/*---------------------------------------------------------------------------
  Application Resources
---------------------------------------------------------------------------*/

static XtResource resource_list[] = {
  { "appmgr", "Appmgr", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, appmgr), XmRImmediate, (XtPointer) False },
  { "filemgr", "Filemgr", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, filemgr), XmRImmediate, (XtPointer) False },
  { "ignoreStartup", "IgnoreStartup", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, ignore_startup), XmRImmediate, (XtPointer) False },
  { "version", "Version", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, version), XmRImmediate, (XtPointer) False },
  { "startDir", "StartDir", XmRString, sizeof(String), XtOffsetOf(Resources, start_dir_r), XmRString, ""},
  { "startupFile", "StartupFile", XmRString, sizeof(String), XtOffsetOf(Resources, startup_file_r), XmRString, "~/.moxfm-startup"},
  { "initAppGeometry", "InitAppGeometry", XmRString, sizeof(String),
      XtOffsetOf(Resources, init_app_geometry), XmRString, "330x360"},
  { "initFileGeometry", "InitFileGeometry", XmRString, sizeof(String),
      XtOffsetOf(Resources, init_file_geometry), XmRString, "500x450" },
  { "appGeometry", "AppGeometry", XmRString, sizeof(String),
      XtOffsetOf(Resources, app_geometry), XmRString, "300x500" },
  { "fileGeometry", "FileGeometry", XmRString, sizeof(String),
      XtOffsetOf(Resources, file_geometry), XmRString, "500x400" },
  { "firstIconPos", "FirstIconPos", XmRString, sizeof(String),    XtOffsetOf(Resources, icon_pos), XmRString, "+0+0" },
  { "iconFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, icon_font), XmRString, XtDefaultFont },
  { "filenameFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, filename_font), XmRString, XtDefaultFont },
  { "buttonFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, button_font), XmRString, XtDefaultFont },
  { "menuFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, menu_font), XmRString, XtDefaultFont },
  { "labelFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, label_font), XmRString, XtDefaultFont },
  { "statusFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, status_font), XmRString, XtDefaultFont },
  { "boldFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, bold_font), XmRString, XtDefaultFont },
  { "cellFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, cell_font), XmRString, XtDefaultFont },
  { "ttyFont", XmCFontList, XmRFontList, sizeof(XmFontList *), 
      XtOffsetOf(Resources, tty_font), XmRString, XtDefaultFont },
  { "appIconWidth", "Width", XmRInt, sizeof(int),
      XtOffsetOf(Resources, app_icon_width), XmRImmediate, (XtPointer) 48 },
  { "appIconHeight", "Height", XmRInt, sizeof(int),
      XtOffsetOf(Resources, app_icon_height), XmRImmediate, (XtPointer) 40 },
  { "fileIconWidth", "Width", XmRInt, sizeof(int),
      XtOffsetOf(Resources, file_icon_width), XmRImmediate, (XtPointer) 48 },
  { "fileIconHeight", "Height", XmRInt, sizeof(int),
      XtOffsetOf(Resources, file_icon_height), XmRImmediate, (XtPointer) 40 },
  { "treeIconWidth", "Width", XmRInt, sizeof(int),
      XtOffsetOf(Resources, tree_icon_width), XmRImmediate, (XtPointer) 48 },
  { "treeIconHeight", "Height", XmRInt, sizeof(int),
      XtOffsetOf(Resources, tree_icon_height), XmRImmediate, (XtPointer) 32 },
  { "bitmapPath", "Path", XmRString, sizeof(String),
      XtOffsetOf(Resources, bitmap_path_r), XmRString, "/usr/include/X11/bitmaps" },
  { "pixmapPath", "Path", XmRString, sizeof(String),
      XtOffsetOf(Resources, pixmap_path_r), XmRString, "/usr/include/X11/pixmaps" },
  { "applCfgPath", "Path", XmRString, sizeof(String),
      XtOffsetOf(Resources, appcfg_path_r), XmRString, "~/.moxfm/" },
  { "tmpObjectDir", "TmpObjectDir", XmRString, sizeof(String), XtOffsetOf(Resources, obj_dir_r), XmRString, "~/.moxfm/tmpobj" },
  { "mainApplicationFile", "ConfigFile",  XmRString, sizeof(String), XtOffsetOf(Resources, app_mainfile_r), XmRString, "~/.moxfm/Main" }, 
  { "configFile", "ConfigFile",  XmRString, sizeof(String),
      XtOffsetOf(Resources, cfg_file_r), XmRString, "~/.moxfm/xfmrc" },
  { "monitorFile", "ConfigFile",  XmRString, sizeof(String),
      XtOffsetOf(Resources, mon_file_r), XmRString, "~/.moxfm/xfmmon" },
  { "devFile", "ConfigFile",  XmRString, sizeof(String),
      XtOffsetOf(Resources, dev_file_r), XmRString, "~/.moxfm/moxfmdev" },
  { "DTIconFile", "ConfigFile", XmRString, sizeof(String), XtOffsetOf(Resources, dticon_file_r), XmRString, "~/.moxfm/Desktop" },
#ifdef MAGIC_HEADERS
  { "magicFile", "ConfigFile",  XmRString, sizeof(String),
      XtOffsetOf(Resources, magic_file_r), XmRString, "~/.moxfm/magic" },
  { "noMagicConfigFile", "ConfigFile",  XmRString, sizeof(String),
      XtOffsetOf(Resources, nomgc_cfg_file_r), XmRString, "~/.moxfm/xfmrc-nomagic" },
#endif
  { "userObjectFile", "UserObjectFile", XmRString, sizeof(String), XtOffsetOf(Resources, usrobj_file_r), XmRString, "~/.moxfm/targets" },
  { "applBoxIcon", "IconFile",  XmRString, sizeof(String),
      XtOffsetOf(Resources, applbox_icon_r), XmRString, "apps.xpm" },
  { "autoSave", "AutoSave", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, auto_save), XmRImmediate, (XtPointer) True },
  { "saveOnExit", "SaveOnExit", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, save_on_exit), XmRImmediate, (XtPointer) False },
  { "checkFiles", "CheckFiles", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, check_files), XmRImmediate, (XtPointer) True },
  { "checkApplicationFiles", "CheckApplicationFiles", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, check_application_files), XmRImmediate, (XtPointer) True },
  { "showCopyInfo", "ShowCopyInfo", XmRBoolean, sizeof(Boolean),
     XtOffsetOf(Resources, copy_info), XmRImmediate, (XtPointer) True },
  { "confirmDeletes", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_deletes), XmRImmediate, (XtPointer) True },
  { "confirmDeleteFolder", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_delete_folder), XmRImmediate,
      (XtPointer) True },
  { "confirmMoves", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_moves), XmRImmediate, (XtPointer) True },
  { "confirmCopies", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_copies), XmRImmediate, (XtPointer) True },
  { "confirmLinks", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_links), XmRImmediate, (XtPointer) True },
  { "confirmOverwrite", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_overwrite), XmRImmediate,
      (XtPointer) True },
  { "confirmQuit", "Confirm", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_quit), XmRImmediate, (XtPointer) True },
  { "echoActions", "Echo", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, echo_actions), XmRImmediate, (XtPointer) False },
  { "showHidden", "ShowHidden", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, show_hidden), XmRImmediate, (XtPointer) False },
  { "dirsFirst", "DirsFirst", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, dirs_first), XmRImmediate, (XtPointer) True },
  { "showOwner", "ShowOwner", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_owner), XmRImmediate, (XtPointer) True },
  { "showDate", "ShowDate", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_date), XmRImmediate, (XtPointer) True },
  { "showPermissions", "ShowPermissions", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_perms), XmRImmediate, (XtPointer) True },
  { "showLength", "ShowLength", XmRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_length), XmRImmediate, (XtPointer) True },
  { "startFromXterm", "StartFromXterm", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, start_from_xterm), XmRImmediate, (XtPointer) False },
  { "keepXterm", "KeepXterm", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, keep_xterm), XmRImmediate, (XtPointer) False },
  { "suppressWarnings", "SuppressWarnings", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, suppress_warnings), XmRImmediate, (XtPointer) False },
  { "newWinOnDirPopup", "NewWinOnDirPopup", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, newwin_on_dirpopup), XmRImmediate, (XtPointer) False },
  { "iconsOverrideRedirect", "IconsOverrideRedirect", XmRBoolean, sizeof(Boolean), XtOffsetOf(Resources, override_redirect), XmRImmediate, (XtPointer) False },
  { "initialDisplayType", "InitialDisplayType", XmRDisplayType, sizeof(DisplayType), XtOffsetOf(Resources, initial_display_type),
      XmRImmediate, (XtPointer) Icons },
  { "defaultSortType", "DefaultSortType", XmRSortType, 
     sizeof(SortType), XtOffsetOf(Resources, default_sort_type),
      XmRImmediate, (XtPointer) SortByName },
  { "passedNamePolicy", "PassedNamePolicy", XmRNamePolicyType, sizeof(NamePolicyType), XtOffsetOf(Resources, passed_name_policy), XmRImmediate, (XtPointer) RelativePath },
  { "iconAlignment", "IconAlignment", XmRDirectionType, sizeof(Direction), XtOffsetOf(Resources, icon_align), XmRImmediate, (XtPointer) Down },
  { "doubleClickTime", "DoubleClickTime", XmRInt, sizeof(int),
     XtOffsetOf(Resources, double_click_time), XmRImmediate,
     (XtPointer) 300 },
  { "updateInterval", "UpdateInterval", XmRInt, sizeof(int),
      XtOffsetOf(Resources, update_interval), XmRImmediate,
      (XtPointer) 10000 },
  { "defaultEditor", "DefaultEditor", XmRString, sizeof(String),
      XtOffsetOf(Resources, default_editor), XmRString, NULL },
  { "defaultViewer", "DefaultViewer", XmRString, sizeof(String),
      XtOffsetOf(Resources, default_viewer), XmRString, NULL },
  { "xterm", "Xterm", XmRString, sizeof(String),
      XtOffsetOf(Resources, xterm), XmRString, "xterm" },
  { "BourneShells", "ShellList", XmRString, sizeof(String),
      XtOffsetOf(Resources, sh_list), XmRString, NULL },
  { "labelColor", "LabelColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, label_color), XmRString, "black" },
  { "selectColor", "SelectColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, select_color), XmRString, "red" },
  { "dropColor", "DropColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, drop_color), XmRString, "lemon chiffon" },
  { "filenameColor", "FilenameColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, filename_color), XmRString, "black"},
  { "linknameColor", "FilenameColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, linkname_color), XmRString, "black" },
  { "textDirColor", "TextDirColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, textdir_color), XmRString, "black" },
  { "dticonColor", "DtIconColor", XmRPixel, sizeof(Pixel), XtOffsetOf(Resources, dticon_color), XmRString, "grey" },
};

/*---------------------------------------------------------------------------
 Fallback resources
---------------------------------------------------------------------------*/

static String fallback_resources[] = {
  "*Command.cursor : hand2",
  "*MenuButton.cursor : hand2",
  "*viewport.forceBars: true",
  "*popup form*bitmap.borderWidth : 0",
  "*popup form*label.borderWidth : 0",
  "*button box.orientation : horizontal",
  "*button box.borderWidth: 0",
  "*form*icon box*Label.borderWidth : 0",
  "*form*icon box.Command.borderWidth : 0",
  "*form*icon box.Form.borderWidth : 0",
  "*form*icon box*Toggle.borderWidth : 1",
  "*chmod*Label.borderWidth : 0",
  "*info*Label.borderWidth : 0",
  "*error*Label.borderWidth : 0",
  "*confirm*Label.borderWidth : 0",
  "*appGeometry : 300x300",
NULL,
};

/*-----------------------------------------------------------------------------
  Signal handler - clears up Zombie processes
  I'll probably extend this in the future to do something useful.
-----------------------------------------------------------------------------*/

static struct sigaction sigcld, sigterm;

#ifndef hpux
static struct sigaction sigsegv, sigbus;
#endif

static void sigcldHandler(int dummy)
{
 waitpid(-1, NULL, WNOHANG);
}

/*-----------------------------------------------------------------------------
  Signal handler - Calls quit on TERM signal
-----------------------------------------------------------------------------*/

static void sigtermHandler(int dummy)
{
 quit(NULL, resources.auto_save);
}

/*-----------------------------------------------------------------------------
  Signal handler - Writes message on SEGV or BUS signal
-----------------------------------------------------------------------------*/

#ifndef hpux

static void sigfatalHandler(int sig)
{
 static const String msg = "\nmoxfm: Internal Error!\n\n"
   "You have found a bug in moxfm.\n\n"
   "Please submit a bug report to ag@muwiinfa.geschichte.uni-mainz.de.\n"
   "You should include the following information:\n\n"
   " -  what you were doing when moxfm crashed \n"
   " -  if the crash is reproduceable\n"
   " -  which version of moxfm you are using (%s)\n"
   " -  your workstation's operating system\n"
   " -  if there were any installation problems\n"
   " -  in case you recompiled moxfm yourself, which Motif version\n"
   "    you linked it with\n"
   " -  if possible, you should examine the core file\n\n"
   "Thanks a lot for your cooperation!\n";
 fprintf(stderr, msg, version);
 signal(sig, (void (*)(int))SIG_DFL);
 unmountAll();
 kill(getpid(), sig);
}

#endif

/*---------------------------------------------------------------------------
  Resource converter functions
---------------------------------------------------------------------------*/

static void CvtStringToDisplayType(XrmValue *args, Cardinal *n_args, XrmValue *fromVal, XrmValue *toVal)
{
  static DisplayType d;

  if (!(strcmp(fromVal->addr, "Icons")))
    d = Icons;
  else if (!(strcmp(fromVal->addr, "Text")))
    d = Text;
  else {
    XtStringConversionWarning(fromVal->addr, XmRDisplayType);
    return;
  }
  
  toVal->addr = (caddr_t) &d;
  toVal->size = sizeof(DisplayType);
}

/*---------------------------------------------------------------------------*/
 
static void CvtStringToSortType(XrmValue *args, Cardinal *n_args,
				XrmValue *fromVal, XrmValue *toVal)
{
  static SortType d;

  if (!(strcmp(fromVal->addr, "SortByName")))
    d = SortByName;
  else if (!(strcmp(fromVal->addr, "SortBySize")))
    d = SortBySize;
  else if (!(strcmp(fromVal->addr, "SortByDate")))
    d = SortByMTime;
  else {
    XtStringConversionWarning(fromVal->addr, XmRSortType);
    return;
  }
  
  toVal->addr = (caddr_t) &d;
  toVal->size = sizeof(SortType);
}

/*---------------------------------------------------------------------------*/

static void CvtStringToDirection(XrmValue *args, Cardinal *n_args, XrmValue *fromVal, XrmValue *toVal)
{
 static Direction d;

 if (!(strcmp(fromVal->addr, "Up")))
     d = Up;
 else if (!(strcmp(fromVal->addr, "Down")))
     d = Down;
 else if (!(strcmp(fromVal->addr, "Left")))
     d = Left;
 else if (!(strcmp(fromVal->addr, "Right")))
     d = Right;
 else
 {
     XtStringConversionWarning(fromVal->addr, XmRDirectionType);
     return;
 }
  
  toVal->addr = (caddr_t) &d;
  toVal->size = sizeof(Direction);
}

/*---------------------------------------------------------------------------*/

static void CvtStringToNamePolicy(XrmValue *args, Cardinal *n_args, XrmValue *fromVal, XrmValue *toVal)
{
 static NamePolicyType d;

 if (!(strcmp(fromVal->addr, "FileName")))
     d = FileName;
 else if (!(strcmp(fromVal->addr, "RelativePath")))
     d = RelativePath;
 else if (!(strcmp(fromVal->addr, "AbsolutePath")))
     d = AbsolutePath;
 else
 {
     XtStringConversionWarning(fromVal->addr, XmRNamePolicyType);
     return;
 }
  
  toVal->addr = (caddr_t) &d;
  toVal->size = sizeof(NamePolicyType);
}

/*---------------------------------------------------------------------------*/
/* Create Atoms (mostly needed for Drag'n'Drop) */

void atomInit(void)
{
  wm_delete_window = XmInternAtom(dpy, "WM_DELETE_WINDOW", False);
  if (wm_delete_window == None)  abortXfm("Couldn't initialize client message handler");
  dragAtoms.file = XmInternAtom(dpy, "FILE_NAME", False);
  dragAtoms.filelist = XmInternAtom(dpy, "FILE_LIST", False);
  dragAtoms.string = XmInternAtom(dpy, "STRING", False);
  dragAtoms.appl = XmInternAtom(dpy, "APPLICATION", False);
  dragAtoms.delete = XmInternAtom(dpy, "DELETE", False);
  dragAtoms.drop = XmInternAtom(dpy, "_MOTIF_DROP", False);
}
/*---------------------------------------------------------------------------*/

void abortXfm(String message)
{
  fprintf(stderr,"%s: %s -- abort\n", progname, message);
  exit(1);
}

/*---------------------------------------------------------------------------*/

void getRootGeom(Widget w)
{
 Window root;
 int buffer;
 unsigned int ubuffer;

 XGetGeometry(dpy, XtWindow(w), &root, &buffer, &buffer, &ubuffer, &ubuffer, &ubuffer, &ubuffer);
 XGetGeometry(dpy, root, &root, &buffer, &buffer, &winInfo.rootWidth, &winInfo.rootHeight, &ubuffer, &ubuffer);
 sscanf(resources.app_geometry, "%dx%d", &winInfo.appWidth, &winInfo.appHeight);
 sscanf(resources.file_geometry, "%dx%d", &winInfo.fileWidth, &winInfo.fileHeight);
}

/*---------------------------------------------------------------------------*/

void noWarnings(String msg)
{
}

/*---------------------------------------------------------------------------
  `Moxfm.BourneShells' related functions  
---------------------------------------------------------------------------*/  

int shell_test(UserInfo *ui)
{
  int pipe_fd[2];
  int p;
  char val[3];

  if (pipe(pipe_fd) < 0) {
    perror("Can't create pipe");
    exit(1);
  }

  p = fork();
  if (p < 0) {
    perror("Can't fork");
    exit(1);
  }

  if (!p) {       /* child; exec the shell w/ test args */
    dup2(pipe_fd[1], fileno(stdout));
    if (close(pipe_fd[0]) == -1) {
      perror("(child) Can't close pipe");
      exit(1);
    }
    execlp(ui->shell, ui->shell, "-c", "echo $*", "1", NULL);
    perror("Exec failed");
    exit(1);
  } else {        /* parent; read and check the child's output */
    if (close(pipe_fd[1]) == -1) {
      perror("(parent) Can't close pipe");
      exit(1);
    }
    val[0] = '\0';
    while ((p = read(pipe_fd[0], val, 3)) < 0) {
      if (errno != EINTR) {
	perror("Reading child's output failed");
	exit(1);
      }
    }
    if (p == 3)
      return -1;
    ui->arg0flag = (val[0] != '1');
    return 0;
  }
}

char *get_first(char *s)
{
  char *p;

  p = strtok(s, ",");
  if (p != NULL)
    while (isspace(*p))
      p++;
  return p;
}

char *get_next()
{
  char *p;
  
  p = strtok((char *) NULL, ",");
  if (p != NULL)
    while (isspace(*p))
      p++;
  return p;
}

void init_arg0flag()
{
  if (resources.sh_list == NULL || !strcmp(resources.sh_list, "AUTO")) {
    if (shell_test(&user) == -1) {
      fprintf(stderr, "Shell auto detection failed.\nRead the moxfm(1) manual page about the Moxfm.BourneShells resource.\n");
      exit(1);
    }
  } else {
    char *p;

    for (p = get_first(resources.sh_list); p != NULL; p = get_next()) {
      if ((user.arg0flag = !strcmp(p, user.shell)))
        return;
    }
  }
}

/*---------------------------------------------------------------------------*/

Boolean startup(void)
{
 FILE *fin;
 int i, n_awins, n_fwins;
 Boolean started = False, ok = True, got_root_geom = False;

 if (resources.ignore_startup || !(exists(resources.startup_file)))  return False;
 if (!(fin = fopen(resources.startup_file, "r")))  return False;
 if (1 == fread(&n_awins, sizeof(int), 1, fin))
 {
     for (i=0; i<n_awins; i++)
     {
	 if (newApplicationWindow(NULL, fin))
	     started = True;
	 else
	 {
	     ok = False;
	     break;
	 }
     }
     if (started)
     {
	 getRootGeom(app_windows->shell);
	 got_root_geom = True;
     }
 }
 else  ok = False;
 if (ok && (1 == fread(&n_fwins, sizeof(int), 1, fin)))
 {
     for (i=0; i<n_fwins; i++)
     {
	 if (newFileWindow(NULL, NULL, fin))
	     started = True;
	 else
	 {
	     ok = False;
	     break;
	 }
     }
     if (!got_root_geom && started)
	 getRootGeom(file_windows->shell);
 }
 if (mntable.n_dev &&
     (1 == fread(&mntable.x, sizeof(Dimension), 1, fin) &&
      1 == fread(&mntable.y, sizeof(Dimension), 1, fin) &&
      1 == fread(&mntable.iconic, sizeof(Boolean), 1, fin)))
     mntable.show = True;
 fclose(fin);
 if (!ok)
     error(getAnyShell(), "Error reading startup file", resources.startup_file);
 return started;
}

/*---------------------------------------------------------------------------
  Main function
---------------------------------------------------------------------------*/

void main(int argc, char *argv[])
{
  Widget fwShell;
  size_t len;
  char *s, firstchar;
  Boolean started;
  int endiantest;

  progname = argv[0];

  /* get some information about the user */
  user.uid = getuid();
  user.gid = getgid();

  user.name = getenv("USER");
  if (!user.name || !user.name[0])
  {
      user.name = getenv("LOGNAME");
      if (!user.name || !user.name[0])
      {
	  user.name = getenv("LOGIN");
	  if ((!user.name || !user.name[0]) && !resources.suppress_warnings)
	      fprintf(stderr, "moxfm warning: Unable to determine user name\n");
      }
  }

#ifdef SVR4
  sysinfo(SI_HOSTNAME, user.hostname, MAXHOSTLEN - 1);
#else
  if (gethostname(user.hostname, MAXHOSTLEN - 1) == EINVAL)
      strcpy(user.hostname, "localhost");
#endif	/* SVR4 */

  if ((s = getenv("HOME")))
      strcpy(user.home, s);
  else  getwd(user.home);

  if ((s = getenv("SHELL")))
    strcpy(user.shell, s);
  else
    strcpy(user.shell, "/bin/sh");

  user.umask = umask(0);
  umask(user.umask);
  user.umask = 0777777 ^ user.umask;

  endiantest = 1;
  if (*((char *) &endiantest) == 1)
      little_endian = True;
  else
      little_endian = False;

  XtSetLanguageProc (NULL, NULL, NULL);
  /* initialise the application and create the application shell */
  topShell = XtVaAppInitialize(&app_context, "Moxfm", options,
			       XtNumber(options), &argc, argv,
			       fallback_resources, NULL);
#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
  /* add support for editres messages */
  XtAddEventHandler(topShell, (EventMask)0, True, _XEditResCheckMessages,
		    NULL);
#endif
#endif

  dpy = XtDisplay(topShell);

  /* make sure we can close-on-exec the display connection */
  if (fcntl(ConnectionNumber(dpy), F_SETFD, 1) == -1)
    abortXfm("Couldn't mark display connection as close-on-exec");

  /* register resource converters */
  XtAppAddConverter(app_context, XmRString, XmRDisplayType, CvtStringToDisplayType, NULL, ZERO);
  XtAppAddConverter(app_context, XmRString, XmRSortType, CvtStringToSortType, NULL, ZERO);
  XtAppAddConverter(app_context, XmRString, XmRDirectionType, CvtStringToDirection, NULL, ZERO);
  XtAppAddConverter(app_context, XmRString, XmRNamePolicyType, CvtStringToNamePolicy, NULL, ZERO);
  XmRepTypeInstallTearOffModelConverter();

  /* get the application resources */
  XtGetApplicationResources(topShell, &resources, resource_list, XtNumber(resource_list), NULL, ZERO);

  /* -version: print version number and exit: */
  if (resources.version)
  {
      printf("moxfm version %s\n", version);
      exit(0);
  }

  /* Option -dir implies option -filemgr */
  if (resources.start_dir_r[0])  resources.filemgr = True;

  /* if -appmgr, -filemgr or -dir have been specified, the startup file is ignored */
  if (resources.appmgr || resources.filemgr)
      resources.ignore_startup = True;

  /* default is to launch both application and file manager */
  if (!resources.appmgr && !resources.filemgr)
    resources.appmgr = resources.filemgr = True;

  /* set the multi-click time */
  XtSetMultiClickTime(dpy, resources.double_click_time);

  /* possibly suppress X toolkit warnings */
  if (resources.suppress_warnings)
      XtAppSetWarningHandler(app_context, noWarnings);

  /* set up signal handlers */
  sigcld.sa_handler = sigcldHandler;
  sigemptyset(&sigcld.sa_mask);
  sigcld.sa_flags = 0;
  sigaction(SIGCHLD, &sigcld, NULL);
  sigterm.sa_handler = sigtermHandler;
  sigemptyset(&sigterm.sa_mask);
  sigterm.sa_flags = 0;
  sigaction(SIGTERM, &sigterm, NULL);
#ifndef hpux
  sigsegv.sa_handler = sigfatalHandler;
  sigemptyset(&sigsegv.sa_mask);
  sigsegv.sa_flags = 0;
  sigaction(SIGSEGV, &sigsegv, NULL);
  sigbus.sa_handler = sigfatalHandler;
  sigemptyset(&sigbus.sa_mask);
  sigbus.sa_flags = 0;
  sigaction(SIGBUS, &sigbus, NULL);
#endif

  /* initialize atoms */
  atomInit();

  /* check the user's shell; needs signal handlers (to avoid a zombie) */
  init_arg0flag();
  
  XtVaGetValues(topShell, XmNbackground, &winInfo.background, NULL);

  /* create all the bitmaps & cursors needed */
  readBitmaps();

  /* initialise the file windows module */
  strcpy(resources.cfg_file, resources.cfg_file_r);
  fnexpand(resources.cfg_file);
  strcpy(resources.mon_file, resources.mon_file_r);
  fnexpand(resources.mon_file);
  strcpy(resources.app_mainfile, resources.app_mainfile_r);
  fnexpand(resources.app_mainfile);
  strcpy(resources.dev_file, resources.dev_file_r);
  fnexpand(resources.dev_file);
  strcpy(resources.appcfg_path, resources.appcfg_path_r);
  fnexpand(resources.appcfg_path);
  strcpy(resources.dticon_file, resources.dticon_file_r);
  fnexpand(resources.dticon_file);
#ifdef MAGIC_HEADERS
  strcpy(resources.magic_file, resources.magic_file_r);
  fnexpand(resources.magic_file);
  strcpy(resources.nomgc_cfg_file, resources.nomgc_cfg_file_r);
  fnexpand(resources.nomgc_cfg_file);
#endif
  strcpy(resources.pixmap_path, resources.pixmap_path_r);
  fnexpand(resources.pixmap_path);
  strcpy(resources.bitmap_path, resources.bitmap_path_r);
  fnexpand(resources.bitmap_path);
  firstchar = resources.start_dir_r[0];
  if ((firstchar) && firstchar != '/' && firstchar != '~')
  {
      getcwd(resources.start_dir, MAXPATHLEN);
      len = strlen(resources.start_dir);
      if (resources.start_dir[len-1] != '/')  strcpy(&resources.start_dir[len], "/");
  }
  else resources.start_dir[0] = 0;
  strcat(resources.start_dir, resources.start_dir_r);
  fnexpand(resources.start_dir);
  contractPath(resources.start_dir);

  chdir(user.home);

  if (!(exists(resources.app_mainfile)) && !(exists(resources.appcfg_path)))
      installDialog();

  readDevices(resources.dev_file);
  initFileWindows();

  strcpy(resources.startup_file, resources.startup_file_r);
  fnexpand(resources.startup_file);

  strcpy(resources.usrobj_file, resources.usrobj_file_r);
  fnexpand(resources.usrobj_file);
  
  strcpy(resources.obj_dir, resources.obj_dir_r);
  fnexpand(resources.obj_dir);

  strcpy(resources.applbox_icon, resources.applbox_icon_r);
  fnexpand(resources.applbox_icon);

  readUserObjects();

  /* initialize the monitoring facility -- this should come before the
     rest of the startup procedure so that monitoring actions can take
     effect before application and file windows are displayed */
  initMonitor();
  doMonitor();

  started = startup();

  if (mntable.n_dev && ((!started && resources.appmgr && resources.filemgr) || (started && mntable.show)))
      displayMountTable();

  /* create the applications window */
  if (!started && resources.appmgr)
  {
      newApplicationWindow(NULL, NULL);
      getRootGeom(app_windows->shell);
  }
  if (!started && resources.filemgr)
  {
  /* create the file window */
      fwShell = newFileWindow((resources.start_dir[0])? resources.start_dir : user.home, NULL, NULL);
      if (!resources.appmgr)
      {
	  if (fwShell == None)  exit(1);
	  getRootGeom(fwShell);
      }
  }

  /* create the desktop icons */
  if (resources.appmgr && exists(resources.dticon_file))  readDTIcons();

  /* application update timeout */
  if (resources.check_application_files && resources.auto_save && resources.update_interval > 0)
      XtAppAddTimeOut(app_context, resources.update_interval, appTimeoutCb, NULL);

 /* collect & process events */
  XtAppMainLoop(app_context);
}

/*---------------------------------------------------------------------------*/

void quit(XtPointer xp, int save)
{
#ifdef DMALLOC
  UserObjectRec *obj;
  DeviceRec *dev;
  int i;
#endif

  if (save == CANCEL)  return;
  while (app_windows)
      closeProc(app_windows, ((save == YES) && app_windows->modified) ? YES : NO);
  if (resources.save_on_exit)
  {
      if (saveWindows())
	  fprintf(stderr, "Error writing startup file %s\n", resources.startup_file);
  }
  unmountAll();
#ifdef DMALLOC	/* free memory, so dmalloc won't complain about memory leaks */
  XTFREE(icons);
  while (file_windows)
      closeFileProc(file_windows);
  freeTypesForDmalloc();
  for (i=0; i<mntable.n_dev; i++)
  {
      dev = &mntable.devices[i];
      XTFREE(dev->label);
      XTFREE(dev->special);
      XTFREE(dev->def_mpoint);
      if (dev->mpoint)  XTFREE(dev->mpoint);
      XTFREE(dev->mount_act);
      XTFREE(dev->umount_act);
  }
  for (i=0; i < n_dtIcons; i++)
  {
      freeApplicationResources(&dtIcons[i]->app);
      XTFREE(dtIcons[i]);
  }
  if (n_dtIcons)  XTFREE(dtIcons);
  for (i=0; i < usrObjects.nr; i++)
  {
      obj = &usrObjects.obj[i];
      XTFREE(obj->atom_name);
      XTFREE(obj->label);
      XTFREE(obj->push_action);
      XTFREE(obj->icon);
      XTFREE(obj->drag_icon);
      XTFREE(obj);
  }
  if (mntable.n_dev)  XTFREE(mntable.devices);
  if (n_monacts)  freeMonacts();
  XTFREE(cpy.copies);
#endif	/* DMALLOC */

  XtDestroyApplicationContext(app_context);
  exit(0);
}
