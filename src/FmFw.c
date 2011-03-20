/*-----------------------------------------------------------------------------
  Module FmFw.c                                                             

  (c) Simon Marlow 1991
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996
                                                                           
  functions & data for creating a file window, and various functions        
  related to file windows                                                   
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <time.h>
#include <string.h>

#ifdef SVR4
#ifndef USE_STATVFS
#define USE_STATVFS
#endif
#endif	/* SVR4 */

#ifdef USE_STATVFS
#include <sys/types.h>	/* apparently needed on SunOS 4.x */
#include <sys/statvfs.h>
#else
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/mount.h>
#else
#ifdef OSF1
#include <sys/vfs_proto.h>
#else
#include <sys/vfs.h>
#endif	/* OSF1 */
#endif	/* __NetBSD__ */
#endif	/* USE_STATVFS */

#ifdef _AIX
#include <sys/resource.h>
#include <sys/statfs.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/Protocols.h>
#include <Xm/TextF.h>
#include <Xm/Frame.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>

#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
#include <X11/Xmu/Editres.h>
#endif
#endif

#include "Fm.h"

#define MAXCFGLINELEN 1024
#define FW_WIDTH 400
#define FW_HEIGHT 300
#define TEXT_PADDING 10
#define BORDER_WIDTH 1
#define SPACING 3
#define MARGIN_HEIGHT 3

/*-----------------------------------------------------------------------------
  PUBLIC DATA                                       
-----------------------------------------------------------------------------*/
/*  RBW - 2001/08/15  */
extern void CatchEntryLeave2(Widget,XtPointer,XEvent*,Boolean *ctd);

FileWindowList file_windows = NULL;
Boolean file_drag = False;

static int n_types = 0, n_mgctypes = 0;
static TypeList types, mgctypes;
static Boolean bitmaps_read = False;
static Dimension icon_item_height = 0, text_item_height = 0;

CopyInfoRec copyInfo;

static Arg drop_activate[] = {
    { XmNdropSiteActivity, XmDROP_SITE_ACTIVE }
};

static Arg drop_inactivate[] = {
    { XmNdropSiteActivity, XmDROP_SITE_INACTIVE }
};

/*-----------------------------------------------------------------------------
  Translation tables
-----------------------------------------------------------------------------*/

static XtTranslations form_translations, dirfield_translations;

static char form_translations_s[] = "\
#override <Btn2Down>	: startFileDrag()\n\
          <Btn3Down>	: filePopup()\n";

static char file_translations_s[] = "#override\
          Ctrl<Btn1Down> : fileToggleCb()\n\
          Shift<Btn1Down>: selectBlockCb()\n\
          <Btn1Down>	 : fileSelectCb()\n\
	  <Btn2Down>	 : startFileDrag()\n\
          <Btn1Up>(2)	 : fileOpenCb()\n";

static char dirfield_translations_s[] = "\
#override <Btn3Down>	: parentsPopup()\n";

/*-----------------------------------------------------------------------------
  Action table
-----------------------------------------------------------------------------*/

static XtActionsRec file_actions[] = {
    { "startFileDrag",	startFileDrag },
    { "filePopup",	filePopup },
    { "fileSelectCb",	fileSelectCb },
    { "fileToggleCb",	fileToggleCb },
    { "selectBlockCb",	selectBlockCb },
    { "fileOpenCb",	fileOpenCb },
    { "parentsPopup",	parentsPopup }
};

/*-----------------------------------------------------------------------------
  Widget Argument Lists
-----------------------------------------------------------------------------*/

static Arg menu_bar_args[] = {
    { XmNleftAttachment, XmATTACH_FORM },
    { XmNtopAttachment, XmATTACH_FORM },
    { XmNrightAttachment, XmATTACH_FORM }
};

static Arg icon_form_args[] = {
    { XmNbackground, 0 },
    { XmNwidth, 0 },
    { XmNtranslations, 0 },
    { XmNborderWidth, BORDER_WIDTH },
    { XmNborderColor, 0 },
    { XmNforeground, 0 },
};

static Arg text_form_args[] = {
    { XmNbackground, 0 },
    { XmNtranslations, 0 },
    { XmNborderWidth, BORDER_WIDTH },
    { XmNborderColor, 0 },
    { XmNforeground, 0 },
};

static Arg icon_toggle_args[] = {
    { XmNlabelType, XmPIXMAP },
    { XmNtopAttachment, XmATTACH_FORM },
    { XmNleftAttachment, XmATTACH_FORM },
    { XmNrightAttachment, XmATTACH_FORM },
    { XmNlabelPixmap, 0 },
    { XmNheight, 0 },
    { XmNfontList, 0 }  /* Seems to be necessary to avoid warnings; Motif bug? */
};

static Arg drop_form_args[] = {
    { XmNimportTargets, 0 },
    { XmNnumImportTargets, 2 },
    { XmNdropSiteOperations, XmDROP_COPY | XmDROP_MOVE | XmDROP_LINK },
    { XmNdropSiteType, XmDROP_SITE_COMPOSITE },
    { XmNdropProc, (XtArgVal) handleFileWinDrop }
};

static Arg drop_file_args[] = {
    { XmNimportTargets, 0 },
    { XmNnumImportTargets, 3 },
    { XmNdropSiteOperations, XmDROP_COPY | XmDROP_MOVE | XmDROP_LINK },
    { XmNdropProc, (XtArgVal) handleFileDrop }
};

static Arg icon_label_args[] = {
    { XmNleftAttachment, XmATTACH_FORM },
    { XmNrightAttachment, XmATTACH_FORM },
    { XmNbottomAttachment, XmATTACH_FORM },
    { XmNtopAttachment, XmATTACH_WIDGET },
    { XmNtopWidget, 0 },
    { XmNlabelString, 0 },
    { XmNfontList, 0 },
};

/*-----------------------------------------------------------------------------
  STATIC DATA 
-----------------------------------------------------------------------------*/

static MenuItemRec file_pdwn_menu[] = {
    { "New...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>N", "Ctrl+N", newFileCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Copy...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>C", "Ctrl+C", fileCopyCb, NULL, NULL, NULL },
    { "Move...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>M", "Ctrl+M", fileMoveCb, NULL, NULL, NULL },
    { "Link...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>L", "Ctrl+L", fileLinkCb, NULL, NULL, NULL },
    { "Perform action...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>P", "Ctrl+P", fileActionCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Delete", &xmPushButtonGadgetClass, 0, "Ctrl<Key>D", "Ctrl+D", fileDeleteCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Select...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>T", "Ctrl+T", selectCb, NULL, NULL, NULL },
    { "Select all", &xmPushButtonGadgetClass, 0, "Ctrl<Key>A", "Ctrl+A", selectAllCb, NULL, NULL, NULL },
    { "Deselect all", &xmPushButtonGadgetClass, 0, NULL, NULL, deselectCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Edit config file", &xmPushButtonGadgetClass, 0, NULL, NULL, editFileConfCb, NULL, NULL, NULL },
    { "Edit monitor file", &xmPushButtonGadgetClass, 0, NULL, NULL, editFileMonCb, NULL, NULL, NULL },
    { "Reload configuration", &xmPushButtonGadgetClass, 0, NULL, NULL, reloadFileConfCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "About moxfm...", &xmPushButtonGadgetClass, 0, NULL, NULL, aboutCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Close window", &xmPushButtonGadgetClass, 0, "Ctrl<Key>W", "Ctrl+W", closeFileCB, NULL, NULL, NULL },
    { "Exit", &xmPushButtonGadgetClass, 0, "Ctrl<Key>Q", "Ctrl+Q", fileExitCb, NULL, NULL },
    { NULL, NULL }
};

static MenuItemRec dir_pdwn_menu[] = {
    { "Make directory...", &xmPushButtonGadgetClass, 0, NULL, NULL, mkDirCb, NULL, NULL, NULL },
    { "Filesystem info...", &xmPushButtonGadgetClass, 0, NULL, NULL, fsInfoCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Reread files", &xmPushButtonGadgetClass, 0, "Ctrl<Key>R", "Ctrl+R", fileRefreshCb, NULL, NULL, NULL },
    { "Filter files...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>F", "Ctrl+F", dirFilterCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Go home", &xmPushButtonGadgetClass, 0, NULL, NULL, goHomeCb, NULL, NULL, NULL },
    { "Go up", &xmPushButtonGadgetClass, 0, NULL, NULL, goUpCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Enter command...", &xmPushButtonGadgetClass, 0, "Ctrl<Key>E", "Ctrl+E", commandCb, NULL, NULL, NULL },
    { NULL, NULL }
};

static MenuItemRec wdw_pdwn_menu[] = {
    { "New file window", &xmPushButtonGadgetClass, 0, NULL, NULL, newFileWinCb, NULL, NULL, NULL },
    { "New application window", &xmPushButtonGadgetClass, 0, NULL, NULL, newAppWinCb, NULL, NULL, NULL },
    { "Mount table window", &xmPushButtonGadgetClass, 0, NULL, NULL, mountTableCb, NULL, NULL, NULL },
    { "Raise desktop icons", &xmPushButtonGadgetClass, 0, "Ctrl<Key>I", "Ctrl+I", raiseIconsCb, NULL, NULL, NULL },
    { "Start xterm", &xmPushButtonGadgetClass, 0, "Ctrl<Key>X", "Ctrl+X", xtermCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Save windows", &xmPushButtonGadgetClass, 0, NULL, NULL, saveStartupCb, NULL, NULL, NULL },
    { "Save desktop icons", &xmPushButtonGadgetClass, 0, NULL, NULL, saveIconsCb, NULL, NULL, NULL },
    { NULL, NULL }
};

static MenuItemRec opt_pdwn_menu[] = {
    { "Icon view", &xmToggleButtonGadgetClass, 0, NULL, NULL, viewTypeCb, NULL, NULL, NULL },
    { "Text view", &xmToggleButtonGadgetClass, 0, NULL, NULL, viewTypeCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Sort by name", &xmToggleButtonGadgetClass, 0, NULL, NULL, sortTypeCb, NULL, NULL, NULL },
    { "Sort by size", &xmToggleButtonGadgetClass, 0, NULL, NULL, sortTypeCb, NULL, NULL, NULL },
    { "Sort by date", &xmToggleButtonGadgetClass, 0, NULL, NULL, sortTypeCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Show hidden files", &xmToggleButtonGadgetClass, 0, NULL, NULL, showHiddenCb, NULL, NULL, NULL },
    { "Show directories", &xmToggleButtonGadgetClass, 0, NULL, NULL, showDirsCb, NULL, NULL, NULL },
    { "Directories first", &xmToggleButtonGadgetClass, 0, NULL, NULL, dirsFirstCb, NULL, NULL, NULL },
#ifdef MAGIC_HEADERS
    { "Use magic headers", &xmToggleButtonGadgetClass, 0, NULL, NULL, mgcHeadersCb, NULL, NULL, NULL },
#endif
    { NULL, NULL }
};

static MenuItemRec file_popup_menu[] = {
    { "Edit", &xmPushButtonGadgetClass, 0, NULL, NULL, fileEditCb, NULL, NULL, NULL },
    { "View", &xmPushButtonGadgetClass, 0, NULL, NULL, fileViewCb, NULL, NULL, NULL },
    { "Perform action...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileActionCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Rename...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileRenameCb, NULL, NULL, NULL },
    { "Copy...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileCopyCb, NULL, NULL, NULL },
    { "Move...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileMoveCb, NULL, NULL, NULL },
    { "Link...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileLinkCb, NULL, NULL, NULL },
    { "Delete", &xmPushButtonGadgetClass, 0, NULL, NULL, fileDeleteCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Properties...", &xmPushButtonGadgetClass, 0, NULL, NULL, filePropsCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Put on desktop", &xmPushButtonGadgetClass, 0, NULL, NULL, fileOnDeskCb, NULL, NULL, NULL },
  { NULL, NULL }
};

static MenuItemRec exec_popup_menu[] = {
    { "Execute...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileExecCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Edit", &xmPushButtonGadgetClass, 0, NULL, NULL, fileEditCb, NULL, NULL, NULL },
    { "View", &xmPushButtonGadgetClass, 0, NULL, NULL, fileViewCb, NULL, NULL, NULL },
    { "Perform action...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileActionCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Rename...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileRenameCb, NULL, NULL, NULL },
    { "Copy...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileCopyCb, NULL, NULL, NULL },
    { "Move...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileMoveCb, NULL, NULL, NULL },
    { "Link...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileLinkCb, NULL, NULL, NULL },
    { "Delete", &xmPushButtonGadgetClass, 0, NULL, NULL, fileDeleteCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Properties...", &xmPushButtonGadgetClass, 0, NULL, NULL, filePropsCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Put on desktop", &xmPushButtonGadgetClass, 0, NULL, NULL, fileOnDeskCb, NULL, NULL, NULL },
    { NULL, NULL }
};

static MenuItemRec dir_popup_menu[] = {
    { "Open", &xmPushButtonGadgetClass, 0, NULL, NULL, dirOpenCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Rename...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileRenameCb, NULL, NULL, NULL },
    { "Copy...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileCopyCb, NULL, NULL, NULL },
    { "Move...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileMoveCb, NULL, NULL, NULL },
    { "Link...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileLinkCb, NULL, NULL, NULL },
    { "Perform action...", &xmPushButtonGadgetClass, 0, NULL, NULL, fileActionCb, NULL, NULL, NULL },
    { "Delete", &xmPushButtonGadgetClass, 0, NULL, NULL, fileDeleteCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Properties...", &xmPushButtonGadgetClass, 0, NULL, NULL, filePropsCb, NULL, NULL, NULL },
    { "", &xmSeparatorGadgetClass, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Put on desktop", &xmPushButtonGadgetClass, 0, NULL, NULL, fileOnDeskCb, NULL, NULL, NULL },
    { NULL, NULL }
};

/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static int longestName(FileWindowRec *fw, XmFontList fontlist);
static void dropInit(FileWindowRec *fw, FileRec *file);
static void makeDirectoryMenu(FileWindowRec *fw);
static void adjustScrollBar(FileWindowRec *fw, Dimension *item_height, int value);

static int parseType(FILE *fp, char **pattern,
#ifdef MAGIC_HEADERS
	char **magic_type,
#endif
        char **icon, char **push_action, char **drop_action, Boolean magic);

static void readFileTypes(Widget shell, String path, int *n_tps, TypeList *typelist, Boolean magic);

static int longestName(FileWindowRec *fw, XmFontList fontlist)
{
 int i,l, longest = 0;
 XmString name;

 for (i=0; i<fw->n_files; i++)
 {
     name = XmStringCreateLocalized(fw->files[i]->name);
     if ((l = XmStringWidth(fontlist, name)) > longest)
	 longest = l;
     XmStringFree(name);
 }
 return longest;
}

/*---------------------------------------------------------------------------*/

static void dropInit(FileWindowRec *fw, FileRec *file)
{
 file->drop_site = False;
 if (S_ISDIR(file->stats.st_mode))
 {
     if (permission(&file->stats, P_WRITE))  file->drop_site = True;
 }
 else if (permission(&file->stats, P_EXECUTE) && !S_ISLNK(file->stats.st_mode))
     file->drop_site = True;
 else if (file->type && file->type->drop_action[0])
     file->drop_site = True;

 if (file->drop_site)
 {
     XmDropSiteRegister(file->form, drop_file_args, XtNumber(drop_file_args));
     XmDropSiteConfigureStackingOrder(file->form, fw->form, XmABOVE);
 }
}

/*---------------------------------------------------------------------------*/

static void readFileBitmaps(Widget shell)
{
 TypeList typelist;
 int i, n, l=0, lmax;

#ifdef MAGIC_HEADERS
 lmax = 1;
#else
 lmax = 0;
#endif
 typelist = types;
 n = n_types;

 do
 {
     for (i=0; i<n; i++)
     {
	 if (!typelist[i].icon[0])
	     typelist[i].icon_pm = icons[FILE_BM];
	 else if ((typelist[i].icon_pm = readIcon(shell, typelist[i].icon)).bm == None) {
#ifdef MAGIC_HEADERS
	     if (l == 1 && !resources.suppress_warnings)
		 fprintf(stderr, "%s: can't read icon for type %s%s%s%s%s%s\n", progname,
			 typelist[i].magic_type?"<":"",
			 typelist[i].magic_type?typelist[i].magic_type:"",
			 typelist[i].magic_type?">":"",
			 typelist[i].dir<0?"*":"", typelist[i].pattern,
			 typelist[i].dir>0?"*":"");
#endif
	     if (l == 0 && !resources.suppress_warnings)
		 fprintf(stderr, "%s: can't read icon for type %s%s%s\n", progname,
			 typelist[i].dir<0?"*":"", typelist[i].pattern,
			 typelist[i].dir>0?"*":"");
	     typelist[i].icon_pm = icons[FILE_BM];
	 }
     }
     typelist = mgctypes;
     n = n_mgctypes;
 } while (l++ <= lmax);
}

/*---------------------------------------------------------------------------*/

static void readFileTypes(Widget shell, String path, int *n_tps, TypeList *typelist, Boolean magic)
{
  FILE *fp;
  char *pattern, *icon, *push_action, *drop_action;
  TypeList typel = NULL;
#ifdef MAGIC_HEADERS
  char *magic_type;
#endif
  char s[MAXCFGSTRINGLEN];
  int i, l, p;
  
  if (!(fp = fopen(path, "r")))
  {
#ifdef MAGIC_HEADERS
      if (!magic)  return;
#endif
      error(getAnyShell(), "Couldn't read configuration file", path);
      return;
  }

  for (i=0; (p = parseType(fp, &pattern,
#ifdef MAGIC_HEADERS
		&magic_type,
#endif
		&icon, &push_action, &drop_action, magic)) > 0; i++)
  {
      typel = (TypeList) XTREALLOC(typel, (i+1) * sizeof(TypeRec));
      l = strlen(pattern);
      if (pattern[0] == '*')
      {
	  typel[i].dir = -1;
	  if (magic)  strparse(s, pattern+1, "\\:<>");
	  else  strparse(s, pattern+1, "\\:");
      }
      else if (pattern[l-1] == '*')
      {
	  typel[i].dir = 1;
	  pattern[l-1] = '\0';
	  if (magic)  strparse(s, pattern, "\\:<>");
	  else  strparse(s, pattern, "\\:");
      }
      else
      {
	  typel[i].dir = 0;
	  if (magic)  strparse(s, pattern, "\\:<>");
	  else  strparse(s, pattern, "\\:");
      }
      typel[i].len = strlen(s);
      typel[i].pattern = XtNewString(s);
#ifdef MAGIC_HEADERS
      if (magic && magic_type)
	  typel[i].magic_type = XtNewString(strparse(s, magic_type, "\\:"));
      else
	  typel[i].magic_type = NULL;
#endif
      typel[i].icon = XtNewString(strparse(s, icon, "\\:"));
      typel[i].push_action = XtNewString(strparse(s, push_action, "\\:"));
      typel[i].drop_action = XtNewString(strparse(s, drop_action, "\\:"));
  }

  if (p == -1)
    error(shell, "Error in configuration file", NULL);

  *n_tps = i;
  *typelist = typel;
  
  if (fclose(fp))
    sysError(shell, "Error reading configuration file:");
}

/*---------------------------------------------------------------------------*/

#ifdef MAGIC_HEADERS
static TypeRec *fileType(char *name, char *magic_type, Boolean magic)
#else
static TypeRec *fileType(char *name)
#endif
{
  TypeList typelist = types;
  int i, l = strlen(name), n_tps = n_types;

#ifdef MAGIC_HEADERS
  if (magic)
  {
      typelist = mgctypes;
      n_tps = n_mgctypes;
  }
#endif

  for (i = 0; i < n_tps; i++) {
#ifdef MAGIC_HEADERS
    if (magic && typelist[i].magic_type) {
      if (strcmp(typelist[i].magic_type, magic_type))
        continue;
      else if (!(strcmp(typelist[i].pattern, ""))) /* Empty pattern. */
        return typelist+i;
    }
#endif
    switch (typelist[i].dir) {
    case 0:
      if (!(strcmp(name, typelist[i].pattern)))
	return typelist+i;
      break;
    case 1:
      if (!(strncmp(typelist[i].pattern, name, typelist[i].len)))
	return typelist+i;
      break;
    case -1:
      if (l >= typelist[i].len && !(strncmp(typelist[i].pattern, name+l-typelist[i].len, typelist[i].len)))
	return typelist+i;
      break;
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/

static int parseType(FILE *fp, char **pattern,
#ifdef MAGIC_HEADERS
	char **magic_type,
#endif
        char **icon, char **push_action, char **drop_action, Boolean magic)
{
  static char s[MAXCFGLINELEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXCFGLINELEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*pattern = split(s, ':')))
    return -1;
#ifdef MAGIC_HEADERS
  if (magic && **pattern == '<') {
    char *ptr;
    ptr = *pattern + 1;
    while(*ptr && (*ptr != '>' || ptr[-1] == '\\'))
	ptr++;
    if(*ptr != '>')
	return -1;
    *ptr = '\0';
    *magic_type = *pattern + 1;
    *pattern = ptr + 1;
  }
  else
    *magic_type = NULL;
#endif
  if (!(*icon = split(NULL, ':')))
    return -1;
  if (!(*push_action = split(NULL, ':')))
    return -1;
  if (!(*drop_action = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

static void makeDirectoryMenu(FileWindowRec *fw)
{
 MenuItemRec *menu;
 char dir[MAXPATHLEN];
 int i, j, n = 0, lastend;

 if (fw->dirfieldPopup != None)
     XtDestroyWidget(fw->dirfieldPopup);
 if ((menu = fw->dirfieldPopupMenu))
 {
     while (menu->label)
     {
	 XTFREE(menu->label);
	 menu++;
     }
     XTFREE(fw->dirfieldPopupMenu);
 }
 strcpy(dir, fw->directory);
 lastend = strlen(dir) - 2;
 for (i = lastend; i>=0; i--)
 {
     if (dir[i] == '/')  n++;
 }
 menu = fw->dirfieldPopupMenu = (MenuItemRec *) XtMalloc((n+1) * sizeof(MenuItemRec));
 for (i=0; i<n; i++, menu++)
 {
     for (j=lastend; j>=0 && dir[j] != '/'; j--) {};
     dir[j + 1] = 0;
     menu->label = XtNewString(dir);
     menu->class = &xmPushButtonGadgetClass;
     menu->accelerator = menu->accel_text = NULL;
     menu->callback = dirPopupCb;
     menu->callback_data = (XtPointer) fw;
     menu->subitems = NULL;
     lastend = j-2;
 }
 menu->label = NULL;
 fw->dirfieldPopup = BuildMenu(fw->dirfield, XmMENU_POPUP, "ParentDirs", 0, False, fw->dirfieldPopupMenu);
}

/*---------------------------------------------------------------------------*/

static void adjustScrollBar(FileWindowRec *fw, Dimension *item_height, int value)
{
 int slider;

 if (!*item_height)
 {
     if (!fw->n_files)  return;
     XtVaGetValues(fw->files[0]->form, XmNheight, item_height, NULL);
     *item_height += 2 * BORDER_WIDTH + SPACING;
 }
 XtVaGetValues(fw->scrollbar, XmNsliderSize, &slider, NULL);
 XmScrollBarSetValues(fw->scrollbar, value, slider, *item_height, slider - *item_height, True);
}

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

void initSelections(SelectionRec *sel)
{
 sel->file = (FileSpec *) XtMalloc((sel->n_alloc = 10) * sizeof(FileRec));
 if (!sel->file)  sel->n_alloc = 0;
 sel->n_sel = 0;
 sel->n_bytes = 0;
}

/*---------------------------------------------------------------------------*/

int getSelNr(FileWindowRec *fw, int item)
{
 int i;

 for (i=0; i < fw->selected.n_sel; i++)
 {
     if (fw->selected.file[i].nr == item)  return i;
 }
 return -1;
}

/*---------------------------------------------------------------------------*/

void unselectAll(FileWindowRec *fw)
{
 SelectionRec *sel = &fw->selected;
 int i;

 for (i=0; i < sel->n_sel; i++)
 {
     XTFREE(sel->file[i].name);
     if (fw->readable)
	 XtVaSetValues(fw->files[sel->file[i].nr]->form, XmNborderColor, winInfo.background, NULL);
 }
 sel->n_sel = 0;
 sel->n_bytes = 0;
}

/*---------------------------------------------------------------------------*/

void updateSelections(FileWindowRec *fw)
{
 SelectionRec *oldselp = &fw->selected, newsel;
 int i, j;

 newsel.n_sel = 0;  newsel.n_bytes = 0;
 newsel.file = (FileSpec *) XtMalloc((newsel.n_alloc = oldselp->n_alloc) * sizeof(FileSpec));
 if (newsel.file)
 {
     for (i=0; i < fw->n_files && oldselp->n_sel; i++)
     {
	 for (j=0; j < oldselp->n_sel; j++)
	 {
	     if (!(strcmp(fw->files[i]->name, oldselp->file[j].name)))
	     {
		 newsel.file[newsel.n_sel].nr = i;
		 newsel.file[newsel.n_sel++].name = oldselp->file[j].name;
		 newsel.n_bytes += fw->files[i]->stats.st_size;
		 memcpy(&oldselp->file[j], &oldselp->file[--oldselp->n_sel], sizeof(FileSpec));
	     }
	 }
     }
 }
 for (j=0; j < oldselp->n_sel; j++)  XTFREE(oldselp->file[j].name);
 XTFREE(oldselp->file);
 *oldselp = newsel;
}

/*---------------------------------------------------------------------------*/

SelFileNamesRec *getSelFiles(FileWindowRec *fw)
{
 SelFileNamesRec *fnames;
 FileRec *file;
 int i;

 if (!fw || !fw->selected.n_sel)  return NULL;
 fnames =  (SelFileNamesRec *) XtMalloc(sizeof(SelFileNamesRec));
 fnames->n_sel = fw->selected.n_sel;
 fnames->first = 0;
 fnames->names = (String *) XtMalloc(fnames->n_sel * sizeof(String));
 for (i=0; i < fnames->n_sel; i++)
     fnames->names[i] = XtNewString(fw->selected.file[i].name);
 fnames->directory = XtNewString(fw->directory);
 if (fnames->n_sel == 1)
 {
     file = fw->files[fw->selected.file[0].nr];
     fnames->icon = file->icon;
 }
 else  fnames->icon = icons[FILES_BM];
 fnames->target = NULL;
 fnames->conf_ovwr = resources.confirm_overwrite;
 fnames->update = False;
 fnames->shell = fw->shell;
 fnames->from_remote = False;
 fnames->op = 0;
 return fnames;
}

/*---------------------------------------------------------------------------*/

void freeSelFiles(SelFileNamesRec *fnames)
{
 int i;

 if (!fnames)  return;
 for (i=0; i < fnames->n_sel; i++)  XTFREE(fnames->names[i]);
 XTFREE(fnames->names);
 XTFREE(fnames->directory);
 if (fnames->target)  XTFREE(fnames->target);
 XTFREE(fnames);
}

/* initialise the file Windows module */
void initFileWindows(void)
{
 Atom *targets = (Atom *) XtMalloc(3 * sizeof(Atom));

 targets[0] = dragAtoms.filelist;
 targets[1] = dragAtoms.file;
 targets[2] = dragAtoms.string;
 drop_form_args[0].value = drop_file_args[0].value = (XtArgVal) targets;
 XtAppAddActions(app_context, file_actions, XtNumber(file_actions));

 form_translations = XtParseTranslationTable(form_translations_s);
 dirfield_translations = XtParseTranslationTable(dirfield_translations_s);
 icon_form_args[0].value = text_form_args[0].value = winInfo.background;
 icon_form_args[2].value = text_form_args[1].value = (XtArgVal) XtParseTranslationTable(file_translations_s);
 icon_toggle_args[5].value = (XtArgVal) resources.file_icon_height;
 icon_toggle_args[6].value = (XtArgVal) resources.icon_font;
 icon_label_args[6].value = (XtArgVal) resources.icon_font;
#ifdef MAGIC_HEADERS
 readFileTypes(topShell, resources.cfg_file, &n_mgctypes, &mgctypes, True);
 readFileTypes(topShell, resources.nomgc_cfg_file, &n_types, &types, False);
 magic_parse_file(resources.magic_file);
#else
 readFileTypes(topShell, resources.cfg_file, &n_types, &types, False);
#endif

 if (!cpy.copies && !(cpy.copies = (CopyRec *) XtMalloc((cpy.n_alc = 100) * sizeof(CopyRec))))  cpy.n_alc = 0;
 copyInfo.dialog = NULL; 
}

/*---------------------------------------------------------------------------*/
/* Create a file Window at the specified path, in the specified format */

FileWindowRec *createFileWindow(String path, FileWindowRec *parent, FILE *fin, GeomRec *geom)
{
 FileWindowRec *fw;
 Widget actionBar, statusform, button;
 char dir[MAXPATHLEN+12];
 XmString sample = XmStringCreateLocalized("8");
 Dimension height;
 MenuItemRec filePopupMenu[XtNumber(file_popup_menu)];
 MenuItemRec dirPopupMenu[XtNumber(dir_popup_menu)];
 MenuItemRec execPopupMenu[XtNumber(exec_popup_menu)];
 MenuItemRec formPopupMenu[XtNumber(file_pdwn_menu)];
 MenuItemRec filePulldownMenu[XtNumber(file_pdwn_menu)];
 MenuItemRec dirPulldownMenu[XtNumber(dir_pdwn_menu)];
 MenuItemRec windowPulldownMenu[XtNumber(wdw_pdwn_menu)];
 MenuItemRec optPulldownMenu[XtNumber(opt_pdwn_menu)];
 size_t len;
 int i;

  /* put at front of linked list */
 fw = (FileWindowRec *) XtMalloc(sizeof(FileWindowRec));
 fw->next = file_windows;
 fw->shell = None;
 if (fin)
 {
     if (readFileWindow(fw, geom, fin))
     {
	 sysError(getAnyShell(), "Error reading startup file:");
	 XTFREE(fw);
	 return NULL;
     }
 }
 else  strcpy(fw->directory, path);

 if (!(fw->getwd = absolutePath(fw->directory)))
 {
     sysError(getAnyShell(), "Can't open folder:");
     XTFREE(fw);
     return NULL;
 }

 if ((fw->dev = findDev(fw->getwd)) != -1)
     if (mountDev(fw->dev, False))
     {
	 error(getAnyShell(), "Cannot mount device on", mntable.devices[fw->dev].def_mpoint);
	 XTFREE(fw->getwd);
	 XTFREE(fw);
	 return NULL;
     }

 strcpy(dir, "moxfm - ");
 XTFREE(fw->getwd);
 if (resolvepath(fw->directory, &dir[len = strlen(dir)]))
     fw->getwd = XtNewString(&dir[len]);
 else
 {
    XTFREE(fw);
    sysError(getAnyShell(), "Can't open folder:");
    return NULL;
 }

 /* set up defaults */
 if (!fin)
 {
     geom->iconic = False;
     if (parent)
     {
	 fw->display_type = parent->display_type;
	 fw->sort_type = parent->sort_type;
	 fw->show_hidden = parent->show_hidden;
	 fw->dirs_first = parent->dirs_first;
     }
     else
     {
	 fw->display_type = resources.initial_display_type;
	 fw->sort_type = resources.default_sort_type;
	 fw->show_hidden = resources.show_hidden;
	 fw->dirs_first = resources.dirs_first;
     }
     fw->show_dirs = True;
 /* KMR */ /* AG removed inherited do_filter attribute */
     fw->do_filter = False;
#ifdef MAGIC_HEADERS
     if (n_mgctypes)  fw->magic_headers = True;
     else  fw->magic_headers = False;
#endif
     fw->dirFilter[0] = 0;
 }
 fw->files = NULL;
 fw->n_files = 0;
 fw->n_bytes = 0;
 fw->update = CHECK_DIR;
 fw->scrolled = None;
 fw->drag_source = False;
 fw->iconic = True;
 fw->dirfieldPopup = None;
 fw->dirfieldPopupMenu = NULL;
#ifdef MAGIC_HEADERS
 if (fw->dev == -1 || !n_types)
     fw->slow_device = False;
 else
     fw->slow_device = mntable.devices[fw->dev].slow_dev;
#endif
 initSelections(&fw->selected);

 fw->shell = XtVaAppCreateShell("file window", "Moxfm", topLevelShellWidgetClass, dpy, XmNiconPixmap, icons[ICON_BM].bm, XmNiconMask, icons[ICON_BM].mask, XmNtitle, dir, XmNiconName, dir, XmNdeleteResponse, XmDO_NOTHING, XmNallowShellResize, False, XmNiconic, geom->iconic, NULL);
 file_windows = fw;

 if (fin)
     XtVaSetValues(fw->shell, XmNwidth, geom->width, XmNheight, geom->height, NULL);
 else if (!winInfo.fileXPos && !winInfo.fileYPos)
     XtVaSetValues(fw->shell, XmNgeometry, resources.init_file_geometry, NULL);
 else
 {
     Dimension xPos, yPos;

     xPos = winInfo.fileXPos;
     yPos = winInfo.fileYPos;
     if (xPos < 75)  xPos += winInfo.rootWidth - winInfo.fileWidth;
     if (yPos < 50)  yPos += winInfo.rootHeight - winInfo.fileHeight;
     xPos -= 75;
     yPos -= 50;
     XtVaSetValues(fw->shell, XmNgeometry, resources.file_geometry, XmNx, xPos, XmNy, yPos, NULL);
 }

 fw->cont = XtVaCreateManagedWidget("container", xmFormWidgetClass, fw->shell, XmNbackground, winInfo.background, NULL);

 fw->menu_bar = XmCreateMenuBar(fw->cont, "menubar", menu_bar_args, XtNumber(menu_bar_args));

 actionBar = XtVaCreateManagedWidget("actionbar", xmFormWidgetClass, fw->cont, XmNbackground, winInfo.background, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, fw->menu_bar, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);

 button = XtVaCreateManagedWidget("reread button", xmPushButtonGadgetClass, actionBar, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[GLASSES_BM].bm, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(button, XmNactivateCallback, fileRefreshCb, (XtPointer) fw);

 button = XtVaCreateManagedWidget("home button", xmPushButtonGadgetClass, actionBar, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[HOME_BM].bm, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, button, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(button, XmNactivateCallback, goHomeCb, (XtPointer) fw);

 button = XtVaCreateManagedWidget("filewin button", xmPushButtonGadgetClass, actionBar, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[NEWFILEWIN_BM].bm, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, button, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(button, XmNactivateCallback, newFileWinCb, (XtPointer) fw);

 button = XtVaCreateManagedWidget("appwin button", xmPushButtonGadgetClass, actionBar, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[NEWAPPWIN_BM].bm, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, button, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(button, XmNactivateCallback, newAppWinCb, NULL);

 button = XtVaCreateManagedWidget("goup button", xmPushButtonGadgetClass, actionBar, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[GOUP_BM].bm, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, button, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(button, XmNactivateCallback, goUpCb, (XtPointer) fw);

 fw->dirfield = XtVaCreateManagedWidget("dirfield", xmTextFieldWidgetClass, actionBar, XmNfontList, (XtArgVal) resources.tty_font, XmNtranslations, dirfield_translations, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, button, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, (XtPointer) (icons[BACKARR_BM].width + 12), XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(fw->dirfield, XmNactivateCallback, changeDirCb, fw);
 XmTextFieldSetString(fw->dirfield, fw->directory);

 button = XtVaCreateManagedWidget("clear", xmPushButtonGadgetClass, actionBar, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[BACKARR_BM].bm, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, fw->dirfield, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtAddCallback(button, XmNactivateCallback, clrDirFieldCb, fw);

 height = XmStringHeight(resources.status_font, sample) + 7;
 XmStringFree(sample);

 fw->form = XtVaCreateManagedWidget("form", xmFormWidgetClass, fw->cont, XmNtranslations, form_translations, XmNheight, (XtArgVal) height, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, actionBar, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, height, NULL);

 XmDropSiteRegister(fw->form, drop_form_args, XtNumber(drop_form_args));

 statusform = XtVaCreateManagedWidget("statusform", xmFormWidgetClass, fw->cont, XmNbackground, (XtArgVal) winInfo.background, XmNforeground, resources.label_color, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, fw->form, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 10, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);

 fw->status = XtVaCreateManagedWidget("Status", xmLabelGadgetClass, statusform, XmNfontList, (XtArgVal) resources.status_font, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);

 for (i=0; i<XtNumber(file_pdwn_menu); i++)
 {
     filePulldownMenu[i] = formPopupMenu[i] = file_pdwn_menu[i];
     filePulldownMenu[i].callback_data = formPopupMenu[i].callback_data = fw;
     formPopupMenu[i].accelerator = formPopupMenu[i].accel_text = NULL;
 }
 BuildMenu(fw->menu_bar, XmMENU_PULLDOWN, "File", 'F', False, filePulldownMenu);

 for (i=0; i<XtNumber(dir_pdwn_menu); i++)
 {
     dirPulldownMenu[i] = dir_pdwn_menu[i];
     dirPulldownMenu[i].callback_data = fw;
 }
 BuildMenu(fw->menu_bar, XmMENU_PULLDOWN, "Directory", 'D', False, dirPulldownMenu);

 for (i=0; i<XtNumber(wdw_pdwn_menu); i++)
 {
     windowPulldownMenu[i] = wdw_pdwn_menu[i];
     windowPulldownMenu[i].callback_data = fw;
 }
 BuildMenu(fw->menu_bar, XmMENU_PULLDOWN, "Windows", 'W', False, windowPulldownMenu);
 if (!mntable.n_dev)
     XtVaSetValues(windowPulldownMenu[2].object, XmNsensitive, False, NULL);

 for (i=0; i<XtNumber(opt_pdwn_menu); i++)
 {
     optPulldownMenu[i] = opt_pdwn_menu[i];
     optPulldownMenu[i].callback_data = fw;
 }
 BuildMenu(fw->menu_bar, XmMENU_PULLDOWN, "Options", 'O', False, optPulldownMenu);
 XmToggleButtonGadgetSetState(fw->icon_view_button = optPulldownMenu[0].object, (fw->display_type == Icons), False);
 XmToggleButtonGadgetSetState(fw->text_view_button = optPulldownMenu[1].object, (fw->display_type == Text), False);
 for (i=0; i<2; i++)
     XtVaSetValues(optPulldownMenu[i].object, XmNindicatorType, XmONE_OF_MANY, NULL);
 XmToggleButtonGadgetSetState(fw->sort_name_button = optPulldownMenu[3].object, (fw->sort_type == SortByName), False);
 XmToggleButtonGadgetSetState(fw->sort_size_button = optPulldownMenu[4].object, (fw->sort_type == SortBySize), False);
 XmToggleButtonGadgetSetState(fw->sort_date_button = optPulldownMenu[5].object, (fw->sort_type == SortByMTime), False);
 for (i=3; i<7; i++)
     XtVaSetValues(optPulldownMenu[i].object, XmNindicatorType, XmONE_OF_MANY, NULL);
 XmToggleButtonGadgetSetState(optPulldownMenu[7].object, fw->show_hidden, False);
 XmToggleButtonGadgetSetState(optPulldownMenu[8].object, fw->show_dirs, False);
 XmToggleButtonGadgetSetState(fw->dirs_first_button = optPulldownMenu[9].object, fw->dirs_first, False);
#ifdef MAGIC_HEADERS
 XmToggleButtonGadgetSetState(fw->magic_button = optPulldownMenu[10].object, (fw->magic_headers && !fw->slow_device), False);
 if (!n_types || !n_mgctypes)
     XtVaSetValues(fw->magic_button, XmNsensitive, False, NULL);
#endif
 if (!fw->show_dirs)
     XtVaSetValues(fw->dirs_first_button, XmNsensitive, False, NULL);

 XtManageChild(fw->menu_bar);
 fw->formPopup = BuildMenu(fw->form, XmMENU_POPUP, "SelFile actions", 0, False, formPopupMenu);

 for (i=0; i<XtNumber(file_popup_menu); i++)
 {
     filePopupMenu[i] = file_popup_menu[i];
     filePopupMenu[i].callback_data = fw;
 }
 fw->filePopup = BuildMenu(fw->form, XmMENU_POPUP, "File actions", 0, False, filePopupMenu);
/* fw->formPopup = fw->filePopup;*/
 fw->var_file_items[0] = filePulldownMenu[2].object;
 fw->var_file_items[1] = filePulldownMenu[3].object;
 fw->var_file_items[2] = filePulldownMenu[4].object;
 fw->var_file_items[3] = filePulldownMenu[5].object;
 fw->var_file_items[4] = filePulldownMenu[7].object;
 fw->var_file_items[5] = formPopupMenu[2].object;
 fw->var_file_items[6] = formPopupMenu[3].object;
 fw->var_file_items[7] = formPopupMenu[4].object;
 fw->var_file_items[8] = formPopupMenu[5].object;
 fw->var_file_items[9] = formPopupMenu[7].object;
 for (i=0; i<XtNumber(dir_popup_menu); i++)
 {
     dirPopupMenu[i] = dir_popup_menu[i];
     dirPopupMenu[i].callback_data = fw;
 }
 fw->dirPopup = BuildMenu(fw->form, XmMENU_POPUP, "Directory actions", 0, False, dirPopupMenu);
 for (i=0; i<XtNumber(exec_popup_menu); i++)
 {
     execPopupMenu[i] = exec_popup_menu[i];
     execPopupMenu[i].callback_data = fw;
 }
 fw->execPopup = BuildMenu(fw->form, XmMENU_POPUP, "Executable actions", 0, False, execPopupMenu);
 makeDirectoryMenu(fw);
 if (!bitmaps_read)
 {
     readFileBitmaps(topShell);
     bitmaps_read = True;
 }

 return fw;
}

/*----------------------------------------------------------------------------*/

int saveFileWindow(FileWindowRec *fw, FILE *fout)
{
 size_t size;

 size = strlen(fw->directory);
 if (1 != fwrite(&size, sizeof(size_t), 1, fout))
     return 1;
 if (size != fwrite(fw->directory, sizeof(char), size, fout))
     return 1;
 if (1 != fwrite(&fw->display_type, sizeof(DisplayType), 1, fout))
     return 1;
 if (1 != fwrite(&fw->show_dirs, sizeof(Boolean), 1, fout))
     return 1;
 if (1 != fwrite(&fw->dirs_first, sizeof(Boolean), 1, fout))
     return 1;
 if (1 != fwrite(&fw->show_hidden, sizeof(Boolean), 1, fout))
     return 1;
 if (1 != fwrite(&fw->sort_type, sizeof(SortType), 1, fout))
     return 1;
 if (1 != fwrite(&fw->do_filter, sizeof(Boolean), 1, fout))
     return 1;
 if (fw->do_filter)
 {
     size = strlen(fw->dirFilter);
     if (1 != fwrite(&size, sizeof(size_t), 1, fout))
	 return 1;
     if (size != fwrite(fw->dirFilter, sizeof(char), size, fout))
	 return 1;
 }
#ifdef MAGIC_HEADERS
 if (1 != fwrite(&fw->magic_headers, sizeof(Boolean), 1, fout))
     return 1;
#endif
 return writeGeometry(fw->shell, fw->iconic, fout);
}

/*----------------------------------------------------------------------------*/

int readFileWindow(FileWindowRec *fw, GeomRec *geom, FILE *fin)
{
 size_t size;

 if (1 != fread(&size, sizeof(size_t), 1, fin))
     return 1;
 if (size != fread(fw->directory, sizeof(char), size, fin))
     return 1;
 fw->directory[size] = 0;
 if (1 != fread(&fw->display_type, sizeof(DisplayType), 1, fin))
     return 1;
 if (1 != fread(&fw->show_dirs, sizeof(Boolean), 1, fin))
     return 1;
 if (1 != fread(&fw->dirs_first, sizeof(Boolean), 1, fin))
     return 1;
 if (1 != fread(&fw->show_hidden, sizeof(Boolean), 1, fin))
     return 1;
 if (1 != fread(&fw->sort_type, sizeof(SortType), 1, fin))
     return 1;
 if (1 != fread(&fw->do_filter, sizeof(Boolean), 1, fin))
     return 1;
 if (fw->do_filter)
 {
     if (1 != fread(&size, sizeof(size_t), 1, fin))
	 return 1;
     if (size != fread(fw->dirFilter, sizeof(char), size, fin))
	 return 1;
     fw->dirFilter[size] = 0;
 }
#ifdef MAGIC_HEADERS
 if (1 != fread(&fw->magic_headers, sizeof(Boolean), 1, fin))
     return 1;
#endif
 return readGeometry(geom, fin);
}

/*----------------------------------------------------------------------------*/

#ifdef MAGIC_HEADERS
void getFileIcon(FileRec *file, Boolean magic)
#else
void getFileIcon(FileRec *file)
#endif
{
 IconRec icon;

#ifdef MAGIC_HEADERS
 /* determine file type first, to allow special items like directories to
      have custom icons */
 if (magic)
 {
     file->type = fileType(file->name, file->magic_type, magic);
     if (file->type)
	 icon = file->type->icon_pm;
     else
	 icon.bm = None;
 }
 else
#endif
 {
     file->type = NULL;
     icon.bm = None;
 }

 if (icon.bm == None)
 {
     /* Symbolic link to non-existent file */
     if (S_ISLNK(file->stats.st_mode))
	 icon = icons[BLACKHOLE_BM];
     else if (S_ISDIR(file->stats.st_mode))
     {
	 if (file->sym_link)
	     icon = icons[DIRLNK_BM];
	 else if (!(strcmp(file->name, "..")))
	     icon = icons[UPDIR_BM];
	 else  icon = icons[DIR_BM];
     }
     else if (file->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
     {
	 if (file->sym_link)  icon = icons[EXECLNK_BM];
	 else  icon = icons[EXEC_BM];
     }
     else
     {
#ifdef MAGIC_HEADERS
	 if (!magic)
	     file->type = fileType(file->name, NULL, magic);
#else
	 file->type = fileType(file->name);
#endif
	 if (file->type && file->type->icon_pm.bm != None)
	     icon = file->type->icon_pm;
	 else if (file->sym_link)  icon = icons[SYMLNK_BM];
	 else  icon = icons[FILE_BM];
     }
 }
 file->icon = icon;
}

/*----------------------------------------------------------------------------*/

void createIconDisplay(FileWindowRec *fw)
{
 Widget toggle;
 Dimension buttonwidth;
 FileRec *file;
 XmString label;
 int i;

 buttonwidth = setFileGeom(fw);
 for (i=0; i < fw->n_files; i++)
 {
     file = fw->files[i];

     if (getSelNr(fw, i) != -1)
	 icon_form_args[4].value = resources.select_color;
     else
	 icon_form_args[4].value = winInfo.background;
     icon_form_args[1].value = buttonwidth;

     if (file->sym_link)
	 icon_form_args[5].value = resources.linkname_color;
     else
	 icon_form_args[5].value = resources.filename_color;

     file->form = XtCreateManagedWidget("file form", xmFormWidgetClass, fw->icon_box, icon_form_args, XtNumber(icon_form_args));

#ifdef MAGIC_HEADERS
     getFileIcon(file, (fw->magic_headers && !fw->slow_device));
#else
     getFileIcon(file);
#endif
     icon_toggle_args[4].value = (XtArgVal) file->icon.bm;
     dropInit(fw, file);

/*  RBW - 2001/08/15  */
XtInsertEventHandler(file->form,
                     EnterWindowMask|LeaveWindowMask,
                     False,
                     (XtEventHandler)CatchEntryLeave2,
                     (XtPointer)0,
                     XtListHead);
     toggle = XtCreateManagedWidget("icon", xmLabelGadgetClass, file->form, icon_toggle_args, XtNumber(icon_toggle_args));


     icon_label_args[4].value = (XtArgVal) toggle;
     label = XmStringCreateLocalized(file->name);
     icon_label_args[5].value = (XtArgVal) label;

     XtCreateManagedWidget("label", xmLabelGadgetClass, file->form, icon_label_args, XtNumber(icon_label_args) );
     XmStringFree(label);
 }
}

/*----------------------------------------------------------------------------*/

void createTextDisplay(FileWindowRec *fw)
{
 FileRec *file;
 Widget label;
 struct passwd *pw;
 Dimension m_width, name_w, size_w, perm_w, own_w = 0, date_w, l;
 XmString *owners = NULL;
 XmString text;
 char name[FILENAME_MAX], s[11];
 int i;

 XtVaSetValues(fw->icon_box, XmNorientation, XmVERTICAL, XmNnumColumns, 1, NULL);
 fw->n_rows = fw->n_files;
 text = XmStringCreateLocalized("m");
 m_width = XmStringWidth(resources.filename_font, text);
 XmStringFree(text);
 name_w = longestName(fw, resources.filename_font) + 2*m_width;
 size_w = m_width * 7;
 perm_w = m_width * 11;
 date_w = m_width * 20;
  
 if (resources.show_owner)
 {
     owners = (XmString *) XtMalloc(fw->n_files * sizeof(XmString));
     own_w = 0;
     for (i=0; i<fw->n_files; i++)
     { 
      /* bug fixed by hkarhune@hydra.helsinki.fi - Thanks */
	 if ((pw = getpwuid(fw->files[i]->stats.st_uid)) == NULL)
	 {
	     char tmp[11];
 	
	     sprintf(tmp, "%lu", (unsigned long) fw->files[i]->stats.st_uid);
	     owners[i] = XmStringCreateLocalized(tmp);
	 }
	 else
	     owners[i] = XmStringCreateLocalized(pw->pw_name);
	 l = XmStringWidth(resources.filename_font, owners[i]);
	 if (l > own_w)	own_w = l;
     }
     own_w += 2*m_width;
 }

 for (i=0; i < fw->n_files; i++)
 {
     file = fw->files[i];

#ifdef MAGIC_HEADERS
     getFileIcon(file, (fw->magic_headers && !fw->slow_device));
#else
     getFileIcon(file);
#endif

     if (getSelNr(fw, i) != -1)
	 text_form_args[3].value = resources.select_color;
     else
	 text_form_args[3].value = winInfo.background;

     if (S_ISDIR(file->stats.st_mode))
	 text_form_args[4].value = resources.textdir_color;
     else if (file->sym_link)
	 text_form_args[4].value = resources.linkname_color;
     else
	 text_form_args[4].value = resources.filename_color;

     file->form = XtCreateManagedWidget("file form", xmFormWidgetClass, fw->icon_box, text_form_args, XtNumber(text_form_args));

     dropInit(fw, file);

     if (S_ISDIR(file->stats.st_mode))
     {
	 sprintf(name, "[%s]", file->name);
	 text = XmStringCreateLocalized(name);
     }
     else
	 text = XmStringCreateLocalized(file->name);
/*  RBW - 2001/08/15  */
XtInsertEventHandler(file->form,
                     EnterWindowMask|LeaveWindowMask,
                     False,
                     (XtEventHandler)CatchEntryLeave2,
                     (XtPointer)0,
                     XtListHead);

     label = XtVaCreateManagedWidget("file_name", xmLabelGadgetClass, file->form, XmNlabelString, text, XmNfontList, resources.filename_font, XmNwidth, name_w, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
     XmStringFree(text);

     if (resources.show_length)
     {
	 sprintf(s, "%lu", (unsigned long) file->stats.st_size);
	 text = XmStringCreateLocalized(s);
	 label = XtVaCreateManagedWidget("file_size", xmLabelGadgetClass, file->form, XmNlabelString, text, XmNfontList, resources.filename_font, XmNwidth, size_w, XmNalignment, XmALIGNMENT_END, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
	 XmStringFree(text);
     }

     if (resources.show_owner)
     {
	 label = XtVaCreateManagedWidget("file_owner", xmLabelGadgetClass, file->form, XmNlabelString, owners[i], XmNfontList, resources.filename_font, XmNwidth, own_w, XmNalignment, XmALIGNMENT_CENTER, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
	 XmStringFree(owners[i]);
     }

     if (resources.show_perms)
     {
	 makePermissionsString(s, file->stats.st_mode, file->sym_link);
	 text = XmStringCreateLocalized(s);
	 label = XtVaCreateManagedWidget("file_perms", xmLabelGadgetClass, file->form, XmNlabelString, text, XmNfontList, resources.filename_font, XmNwidth, perm_w, XmNalignment, XmALIGNMENT_CENTER, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
	 XmStringFree(text);
     }

     if (resources.show_date)
     {
         char *t = ctime(&file->stats.st_mtime);
	 int l = strlen(t);
	 if (*t && t[l-1] == '\n') t[l-1] = 0;
	 text = XmStringCreateLocalized(t);
	 label = XtVaCreateManagedWidget("file_date", xmLabelGadgetClass, file->form, XmNlabelString, text, XmNfontList, resources.filename_font, XmNwidth, date_w, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
	 XmStringFree(text);
     }
 }
 if (resources.show_owner)  XTFREE(owners);
}

/*----------------------------------------------------------------------------*/

Boolean chFileDir(FileWindowRec *fw, String path)
{
 char dir[MAXPATHLEN+12], *cwd;
 int d;

 if (!(cwd = absolutePath(path)))
  {
     sysError(fw->shell, "Can't open folder:");
     return False;
 }

 d = findDev(cwd);
 if (d != fw->dev)
 {
     if (mountDev(d, False))
     {
	 XTFREE(cwd);
	 error(fw->shell, "Cannot mount device on", mntable.devices[d].def_mpoint);
	 return False;
     }
     umountDev(fw->dev, False);
     fw->dev = d;
#ifdef MAGIC_HEADERS
     if (d != -1)
	 fw->slow_device = mntable.devices[d].slow_dev;
     else
	 fw->slow_device = False;
     XmToggleButtonGadgetSetState(fw->magic_button, (fw->magic_headers && !fw->slow_device), False);
#endif
 }

 XTFREE(cwd);
 strcpy(dir, "moxfm - ");
 if (!(cwd = resolvepath(path, &dir[strlen(dir)])))
 {
     sysError(fw->shell, "Can't open folder:");
     return False;
 }
 XTFREE(fw->getwd);
 fw->getwd = XtNewString(cwd);

 zzz();
 strcpy(fw->directory, path);
 unselectAll(fw);
 if (fw->readable)  freeFileList(fw->files, fw->n_files);
 readFiles(fw);
#ifdef MAGIC_HEADERS
 getMagicTypes(fw);
#endif
 fw->n_rows = 0;	/* Set Scrollbar to top */
 showFileDisplay(fw);
 XmTextFieldSetString(fw->dirfield, path);
 makeDirectoryMenu(fw);
 XtVaSetValues(fw->shell, XmNtitle, dir, XmNiconName, dir, NULL);
 wakeUp();
 return True;
}

/*----------------------------------------------------------------------------*/

Widget newFileWindow(String path, FileWindowRec *parent, FILE *fin)
{
 FileWindowRec *fw;
 GeomRec geom;

 if (!(fw = createFileWindow(path, parent, fin, &geom)))
     return None;
#if (XtSpecificationRelease > 4)
#ifndef _NO_XMU
 XtAddEventHandler(fw->shell, (EventMask)0, True, _XEditResCheckMessages,
		   NULL);
#endif
#endif
 readFiles(fw);
#ifdef MAGIC_HEADERS
 getMagicTypes(fw);
#endif
 XtAddEventHandler(fw->shell, StructureNotifyMask, True, (XtEventHandler) fileIconifyHandler, fw);
 XtAddEventHandler(fw->form, StructureNotifyMask, True, (XtEventHandler) resizeFile, fw);
/* resizeFile() is called on first exposure and on resize events: */
 XmAddWMProtocolCallback(fw->shell, wm_delete_window, closeFileCB, fw);
 if (resources.update_interval > 0)
     fw->timeoutId = XtAppAddTimeOut(app_context, resources.update_interval, timeoutCb, fw);
 else  fw->timeoutId = 0;

 XtRealizeWidget(fw->shell);
 if (fin)
     XtVaSetValues(fw->shell, XmNx, geom.x, XmNy, geom.y, NULL);
 else
     XtVaGetValues(fw->shell, XmNx, &winInfo.fileXPos, XmNy, &winInfo.fileYPos, NULL);
 return fw->shell;
}

/*---------------------------------------------------------------------------*/

/* Main procedure to create the display*/
void readFiles(FileWindowRec *fw)
{
 if ((fw->readable = readDirectory(fw)))
 {
     filterDirectory(fw, fw->show_dirs ? All : Files);
     sortDirectory(fw->files, fw->n_files, fw->sort_type, fw->dirs_first);
 }
}

/*---------------------------------------------------------------------------*/

Boolean showFileDisplay(FileWindowRec *fw)
{
 Dimension *item_height;
 int orows = fw->n_rows, scroll_value, dummy;

 if (fw->scrolled != None)
 {
     XmScrollBarGetValues(fw->scrollbar, &scroll_value, &dummy, &dummy, &dummy);
     XtDestroyWidget(fw->scrolled);
 }
 else
     scroll_value = 0;

 fw->scrolled = XtVaCreateManagedWidget("scrolled", xmScrolledWindowWidgetClass, fw->form,  XmNscrollingPolicy, XmAUTOMATIC, /*XmNvisualPolicy, XmVARIABLE, */XmNbackground, (XtArgVal) winInfo.background, XmNspacing, 0, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 XtVaGetValues(fw->scrolled, XmNverticalScrollBar, &fw->scrollbar, NULL);

 fw->icon_box = XtVaCreateWidget("icon box", xmRowColumnWidgetClass, fw->scrolled, XmNpacking, XmPACK_COLUMN, XmNorientation, XmHORIZONTAL, XmNbackground, winInfo.background, XmNforeground, resources.label_color, XmNmarginHeight, MARGIN_HEIGHT, XmNspacing, SPACING, NULL);

 if (!fw->readable)
 {
     XmDropSiteUpdate(fw->form, drop_inactivate, XtNumber(drop_inactivate));

     XtVaCreateManagedWidget("Directory is unreadable", xmLabelGadgetClass, fw->icon_box, XmNfontList, resources.label_font, NULL);
 }
 else
 {
     XmDropSiteUpdate(fw->form, drop_activate, XtNumber(drop_activate));
     chdir(fw->directory);
     switch (fw->display_type)
     {
	 case Icons:	createIconDisplay(fw);
	     item_height = &icon_item_height;
	     break;
	 case Text:	createTextDisplay(fw);
	     item_height = &text_item_height;
     }
     chdir(user.home);
 }
 updateStatus(fw);
 XtManageChild(fw->icon_box);
 if (fw->readable)
 {
     if (fw->n_rows != orows)  scroll_value = 0;
     adjustScrollBar(fw, item_height, scroll_value);
 }
 fw->update = CHECK_DIR;
 return True;
}

/*---------------------------------------------------------------------------*/

/* Update the display in the viewport */
void updateFileDisplay(FileWindowRec *fw, int reread)
{
 struct stat cur;
 FileList oldfiles = fw->files;
 int on_files = fw->n_files, i;
 Boolean reshow = False, repl_timeout = True;

 if (reread == CHECK_DIR && resources.check_files)
     repl_timeout = False;

 if (reread == RESHOW || fw->update == RESHOW || stat(fw->directory, &cur) || cur.st_ctime > fw->stats.st_ctime || cur.st_mtime > fw->stats.st_mtime)
 {
     readFiles(fw);
     reshow = True;
 }
 else if (reread == CHECK_FILES || fw->update == CHECK_FILES)
 {
     readFiles(fw);
     if (fw->n_files != on_files)  /* Should never happen, but anyway ... */
	 reshow = True;
     else
     {
	 for (i=0; i<on_files; i++)
	 {
	     if (!S_ISDIR(fw->files[i]->stats.st_mode) &&
		 (fw->files[i]->stats.st_ctime > oldfiles[i]->stats.st_ctime || fw->files[i]->stats.st_mtime > oldfiles[i]->stats.st_mtime))
		 reshow = True;
	 }
     }
 }
 else  return;

 if (reshow && !file_drag)
 {
     zzz();
#ifdef MAGIC_HEADERS
     getMagicTypes(fw);
#endif
     freeFileList(oldfiles, on_files);
     updateSelections(fw);
     showFileDisplay(fw);
     wakeUp();
 }
 else
 {
     if (fw->readable)  freeFileList(fw->files, fw->n_files);
     fw->files = oldfiles;
     fw->n_files = on_files;
     if (reshow)  fw->update = RESHOW;
     else  fw->update = CHECK_DIR;
 }
 if ((reshow || repl_timeout) && resources.update_interval > 0)
 {
     if (fw->timeoutId)
	 XtRemoveTimeOut(fw->timeoutId);
     fw->timeoutId = XtAppAddTimeOut(app_context, resources.update_interval, timeoutCb, fw);
 }
}

/*---------------------------------------------------------------------------*/

Dimension setFileGeom(FileWindowRec *fw)
{
 Dimension width, itemwidth, scrollbarwidth;
 int ncols;

 XtVaGetValues(fw->form, XmNwidth, &width, NULL);
 XtVaGetValues(fw->scrollbar, XmNwidth, &scrollbarwidth, NULL);
 XtVaSetValues(fw->dirfield, XmNwidth, width-150, NULL);

 width -= scrollbarwidth + 20;
 itemwidth = longestName(fw, resources.icon_font);
 if (itemwidth < resources.file_icon_width)
     itemwidth = resources.file_icon_width;
 ncols = width / (itemwidth + 4);
 if (!ncols)  ncols = 1;
 fw->n_rows = (fw->n_files-1)/ncols + 1;
 XtVaSetValues(fw->icon_box, XmNnumColumns, fw->n_rows, NULL);
 return itemwidth;
}

/*---------------------------------------------------------------------------*/

void resizeFile(Widget form, XtPointer client_data, XEvent *event)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 fw->n_rows = 0;	/* Set Scrollbar to top */
 updateFileDisplay(fw, RESHOW);
}

/*---------------------------------------------------------------------------*/

void fileIconifyHandler(Widget shell, XtPointer client_data, XEvent *event)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 if (event->type == MapNotify)
     fw->iconic = False;
 else if (event->type == UnmapNotify)
     fw->iconic = True;
}

/*----------------------------------------------------------------------------
  Intelligent update - only update the windows needed.
  Use markForUpdate() to explicitly mark a directory for update.
  Call intUpdate() to execute all the actions.
-----------------------------------------------------------------------------*/

void markForUpdate(String path, int reread)
{
 FileWindowRec *fw;
 struct stat stats;
 char dir[MAXPATHLEN];
 int i;

 strcpy(dir, path);
 if (stat(dir, &stats) || !S_ISDIR(stats.st_mode))
 {
     for (i = strlen(dir) - 1; i >= 0; i--)
     {
	 if (dir[i] == '/')
	 {
	     dir[i] = 0;
	     break;
	 }
     }
 }
 if (!chdir(dir) && getwd(dir))
  {
      for (fw = file_windows; fw; fw = fw->next)
	  if (!(strcmp(fw->getwd, dir)))
	      fw->update = reread;
  }
  chdir(user.home);
}

/*---------------------------------------------------------------------------*/

void intUpdate(int reread)
{
 FileWindowRec *fw;

 for (fw = file_windows; fw; fw = fw->next)
     updateFileDisplay(fw, reread);
}

/*---------------------------------------------------------------------------*/

void reloadFileConfCb(Widget w, XtPointer client_data, XtPointer calldata)
{
 FileWindowRec *fw;
 Widget shell = ((FileWindowRec *) client_data)->shell;
 int i;

 zzz();
 for (fw = file_windows; fw; fw = fw->next)
 {
     if (fw->scrolled != None)
     {
	 XtDestroyWidget(fw->scrolled);
	 fw->scrolled = None;
     }
 }

 for (i=0; i < n_types; i++)
     if (types[i].icon[0] && types[i].icon_pm.bm != icons[FILE_BM].bm)
	 freePixmap(types[i].icon_pm.bm);
#ifdef MAGIC_HEADERS
 for (i=0; i < n_mgctypes; i++)
     if (mgctypes[i].icon[0] && mgctypes[i].icon_pm.bm != icons[FILE_BM].bm)
	 freePixmap(mgctypes[i].icon_pm.bm);

 readFileTypes(shell, resources.cfg_file, &n_mgctypes, &mgctypes, True);
 readFileTypes(shell, resources.nomgc_cfg_file, &n_types, &types, False);
 magic_parse_file(resources.magic_file);

#else
 readFileTypes(shell, resources.cfg_file, &n_types, &types, False);
#endif

 readFileBitmaps(shell);

 reinitMonitor();
 doMonitor();
 for (fw = file_windows; fw; fw = fw->next)
     updateFileDisplay(fw, RESHOW);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void updateStatus(FileWindowRec *fw)
{
 char s[1024];
 XmString info;
 int i;
 long bsize, ksize, free, free_blocks;
 int res;
#ifdef USE_STATVFS
 struct statvfs stat;
#else
 struct statfs stat;
#endif

 if (fw->do_filter)
     sprintf(s, "[%s]: ", fw->dirFilter);
 else
     s[0] = 0;

 sprintf(s+strlen(s),
	 "%ld byte%s in %d item%s",
	 fw->n_bytes, fw->n_bytes == 1 ? "" : "s",
	 fw->n_files, fw->n_files == 1 ? "" : "s");
 if (fw->selected.n_sel)
 {
     for (i=0; i<NVARITEMS; i++)
	 fillIn(fw->var_file_items[i]);
     sprintf(s+strlen(s), ", %ld byte%s in %d selected item%s",
	     fw->selected.n_bytes, fw->selected.n_bytes == 1 ? "" : "s",
	     fw->selected.n_sel, fw->selected.n_sel == 1 ? "" : "s");
 }
 else
 {
     for (i=0; i<NVARITEMS; i++)
	 grayOut(fw->var_file_items[i]);
 }

#ifdef USE_STATVFS
 res = statvfs(fw->directory, &stat);
#else
 res = statfs(fw->directory, &stat);
#endif
#ifdef OSF1
 bsize = (long) stat.f_fsize / 1024L;
 if (!bsize)  ksize = 1024L / (long) stat.f_fsize;
#else
 bsize = (long) stat.f_bsize / 1024L;
 if (!bsize)  ksize = 1024L / (long) stat.f_bsize;
#endif

 if (!res)
 {
     if (geteuid())
	 free_blocks = stat.f_bavail;
     else
	 free_blocks = stat.f_bfree;

     if (bsize) free = free_blocks * bsize;
     else  free = free_blocks / ksize;

     sprintf(&s[strlen(s)], ", %ld KBytes available", free);
 }

 info = XmStringCreateLocalized(s);
 XtVaSetValues(fw->status, XmNlabelString, (XtArgVal) info, NULL);
 XmStringFree(info);
}

/*---------------------------------------------------------------------------*/

void closeFileProc(FileWindowRec *fw)
{
 FileWindowRec *fwi;
 MenuItemRec *menu;
 int i;

 if (fw->timeoutId)
     XtRemoveTimeOut(fw->timeoutId);
 XtDestroyWidget(fw->shell);
 umountDev(fw->dev, False);
 if (fw == file_windows)  file_windows = fw->next;
 else
 {
     for (fwi=file_windows; (fwi); fwi=fwi->next)
     {
	 if (fwi->next == fw)
	 {
	     fwi->next = fw->next;
	     break;
	 }
     }
 }
 for (i=0; i < fw->selected.n_sel; i++)
     XTFREE(fw->selected.file[i].name);
 XTFREE(fw->selected.file);
 for (i=0; i < fw->n_files; i++)  XTFREE(fw->files[i]);
 XTFREE(fw->files);
 XTFREE(fw->getwd);
 if ((menu = fw->dirfieldPopupMenu))
 {
     while (menu->label)
     {
	 XTFREE(menu->label);
	 menu++;
     }
     XTFREE(fw->dirfieldPopupMenu);
 }
 XTFREE(fw);
}

/*---------------------------------------------------------------------------*/

void fileSelect(FileWindowRec *fw, int item)
{
 SelectionRec *sel = &fw->selected;

 if (!(sel->n_alloc))
     sel->file = (FileSpec *) XtMalloc((sel->n_alloc = 10) * sizeof(FileRec));
 if (sel->file)
 {
     unselectAll(fw);
     sel->n_sel = 1;
     sel->file[0].nr = item;
     sel->file[0].name = XtNewString(fw->files[item]->name);
     sel->n_bytes = fw->files[item]->stats.st_size;
     XtVaSetValues(fw->files[item]->form, XmNborderColor, resources.select_color, NULL);
 }
 else
 {
     sel->n_sel = 0;
     sel->n_bytes = 0;
 }
 updateStatus(fw);
}

/*---------------------------------------------------------------------------*/

void fileToggle(FileWindowRec *fw, int item)
{
 SelectionRec *sel = &fw->selected;
 int i, nr;

 if ((nr = getSelNr(fw, item)) != -1)
 {
     XTFREE(sel->file[nr].name);
     memcpy(&sel->file[nr], &sel->file[--(sel->n_sel)], sizeof(FileSpec));
     sel->n_bytes -= fw->files[item]->stats.st_size;
     XtVaSetValues(fw->files[item]->form, XmNborderColor, winInfo.background, NULL);
 }
 else
 {
     if ((i = sel->n_sel++) >= sel->n_alloc)
	 sel->file = (FileSpec *) XTREALLOC(sel->file, (sel->n_alloc += 10) * sizeof(FileSpec));
     sel->file[i].nr = item;
     sel->file[i].name = XtNewString(fw->files[item]->name);
     sel->n_bytes += fw->files[item]->stats.st_size;
     XtVaSetValues(fw->files[item]->form, XmNborderColor, resources.select_color, NULL);
 }
 updateStatus(fw);
}

/*---------------------------------------------------------------------------*/

void selectAll(FileWindowRec *fw)
{
 SelectionRec *sel = &fw->selected;
 FileRec *file;
 int i, nr = 0;

 if (fw->n_files > sel->n_alloc)
     sel->file = (FileSpec *) XTREALLOC(sel->file, (sel->n_alloc = fw->n_files) * sizeof(FileSpec));
 sel->n_bytes = 0;

 for (i=0; i < fw->n_files; i++)
 {
     file = fw->files[i];
     if (strcmp(file->name, ".."))
     {
	 sel->file[nr].name = XtNewString(file->name);
	 sel->file[nr++].nr = i;
	 sel->n_bytes += file->stats.st_size;
	 XtVaSetValues(file->form, XmNborderColor, resources.select_color, NULL);
     }
     else
	 XtVaSetValues(file->form, XmNborderColor, winInfo.background, NULL);
 }
 sel->n_sel = nr;
}

/*---------------------------------------------------------------------------*/

#ifdef DMALLOC

void freeTypesForDmalloc(void)
{
 int i;

 for (i=0; i<n_types; i++)
 {
     XTFREE(types[i].pattern);
     XTFREE(types[i].icon);
     XTFREE(types[i].push_action);
     XTFREE(types[i].drop_action);
 }
 if (n_types) XTFREE(types);

 for (i=0; i<n_mgctypes; i++)
 {
     XTFREE(mgctypes[i].pattern);
#ifdef MAGIC_HEADERS
     XTFREE(mgctypes[i].magic_type);
#endif
     XTFREE(mgctypes[i].icon);
     XTFREE(mgctypes[i].push_action);
     XTFREE(mgctypes[i].drop_action);
 }
 if (n_mgctypes) XTFREE(mgctypes);

 XTFREE((Atom *) drop_file_args[0].value);
}

#endif	/* DMALLOC */


