/*-----------------------------------------------------------------------------
  Fm.h
  
  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996
-----------------------------------------------------------------------------*/

#ifndef FM_H
#define FM_H

#include "Common.h"

#ifdef linux
#define HAVE_REALPATH
#endif
#ifdef SVR4
#define HAVE_REALPATH
#endif

#ifdef HAVE_REALPATH
#define resolvepath realpath
#else
extern char *resolvepath(const char *path, char *resolved_path);
#endif	/* HAVE_REALPATH */

/*--FmDirs-------------------------------------------------------------------*/

#define MAXCFGSTRINGLEN MAXPATHLEN

#ifdef MAGIC_HEADERS
#ifndef MAGICTYPE_MAX
#define MAGICTYPE_MAX 128
#endif
#endif

#define NVARITEMS 10

void contractPath(String path);
String absolutePath(String path);
String relativePath(String path, String relative_to);
int dirComp(String dir1, String dir2);

/*--FmFw/FmDirs-------------------------------------------------------------------*/

/* structure representing user-defined file types */
typedef struct {
  char *pattern;
#ifdef MAGIC_HEADERS
  char *magic_type;
#endif
  char *icon;
  char *push_action;
  char *drop_action;
  int len, dir;
  IconRec icon_pm;
} TypeRec, *TypeList;

/* structure into which the directory information is read */
typedef struct {
	char name[FILENAME_MAX];
#ifdef MAGIC_HEADERS
	char magic_type[MAGICTYPE_MAX];
#endif
	struct stat stats;
	Widget form;
	IconRec icon;
	TypeRec *type;
	Boolean sym_link, drop_site;
} FileRec, **FileList;

typedef struct {
    int nr;
    String name;
} FileSpec;

typedef struct {
    int n_sel;
    size_t n_alloc;
    long n_bytes;
    FileSpec *file;
} SelectionRec;

typedef struct _FileWindowRec {
  struct _FileWindowRec *next;
  DisplayType display_type;
  Boolean show_dirs, dirs_first, show_hidden;
  int update;
  SortType sort_type;
  Widget shell, cont, menu_bar, form, dirfield, status, scrolled, icon_box, scrollbar;
  Widget var_file_items[NVARITEMS];
  Widget filePopup, dirPopup, execPopup, formPopup, dirfieldPopup;
  Widget dirs_first_button, icon_view_button, text_view_button, sort_name_button, sort_size_button, sort_date_button;
  MenuItemRec *dirfieldPopupMenu;
  SelectionRec selected;
  char directory[MAXPATHLEN];
  String getwd;
  int dev;
  struct stat stats;
  FileList files;
  long n_bytes;
  int n_files, n_rows;
  Boolean readable, iconic, drag_source;
  Boolean do_filter;             /* KMR */
  char dirFilter[MAXPATHLEN];    /* KMR */
  XtIntervalId timeoutId;
#ifdef MAGIC_HEADERS
  Boolean magic_headers, slow_device;
  Widget magic_button;
#endif
} FileWindowRec, *FileWindowList;

typedef struct
{
    String directory;
    int n_sel, first;
    String *names;
    String target;	/* used for copy and move */
    Boolean dirtarget, conf_ovwr, update;
    IconRec icon;
    Widget shell;
    int op;		/* operation to be performed */
    Boolean from_remote;
} SelFileNamesRec;

typedef struct
{
    Widget dialog;
    Widget number, from, to;
} CopyInfoRec;

typedef struct
{
    int device, trials;
} UnmountProcRec;

extern CopyInfoRec copyInfo;

#define P_READ 0x1
#define P_WRITE 0x2
#define P_EXECUTE 0x4

/* public functions */
Boolean readDirectory(FileWindowRec *fw);
void filterDirectory(FileWindowRec *fw, FilterType type);
void sortDirectory(FileList fl, int n, SortType type, Boolean dirs_first);
int permission(struct stat *stats, int perms);
void makePermissionsString(char *s, int perms, Boolean symlink);
void freeFileList(FileList files, int n_files);
#ifdef MAGIC_HEADERS
void getMagicTypes(FileWindowRec *fw);
void magic_parse_file(char *name);
void magic_get_type(char *name, char *buf);
#endif
void fsInfo(FileWindowRec *fw);

/*--FmFw---------------------------------------------------------------------*/

#define CHECK_DIR 0
#define CHECK_FILES 1
#define RESHOW 2

extern FileWindowList file_windows;
extern Boolean file_drag;

void initSelections(SelectionRec *sel);
int getSelNr(FileWindowRec *fw, int item);
SelFileNamesRec *getSelFiles(FileWindowRec *fw);
void freeSelFiles(SelFileNamesRec *fnames);
void unselectAll(FileWindowRec *fw);
void updateSelections(FileWindowRec *fw);
void initFileWindows();
FileWindowRec *createFileWindow(String path, FileWindowRec *parent, FILE *fin, GeomRec *geom);
int saveFileWindow(FileWindowRec *fw, FILE *fout);
int readFileWindow(FileWindowRec *fw, GeomRec *geom, FILE *fin);
void readFiles(FileWindowRec *fw);
#ifdef MAGIC_HEADERS
void getFileIcon(FileRec *file, Boolean magic);
#else
void getFileIcon(FileRec *file);
#endif
void createIconDisplay(FileWindowRec *fw);
void createTextDisplay(FileWindowRec *fw);
Widget newFileWindow(String path, FileWindowRec *parent, FILE *fin);
Boolean showFileDisplay(FileWindowRec *fw);
void resizeFile(Widget form, XtPointer client_data, XEvent *event);
void fileIconifyHandler(Widget shell, XtPointer client_data, XEvent *event);
void closeFileProc(FileWindowRec *fw);
void updateFileDisplay(FileWindowRec *fw, int reread);
Dimension setFileGeom(FileWindowRec *fw);
Boolean chFileDir(FileWindowRec *fw, String path);
void reSortFileDisplay(FileWindowRec *fw);
void reDisplayFileWindow(FileWindowRec *fw);
void fileSelect(FileWindowRec *fw, int i);
void fileToggle(FileWindowRec *fw, int i);
void selectAll(FileWindowRec *fw);

void markForUpdate(String path, int reread);
void intUpdate(int reread);
void reloadFileConfCb(Widget w, XtPointer client_data, XtPointer calldata);
void updateStatus(FileWindowRec *fw);

#ifdef DMALLOC
void freeTypesForDmalloc(void);
#endif

/*--FmFwCb-------------------------------------------------------------------*/

FmCallbackProc
  newFileCb, mkDirCb, selectCb, selectAllCb, deselectCb, editFileConfCb, editFileMonCb, fileIconsCb, fsInfoCb,
  fileTextCb, fileSortNameCb, fileSortSizeCb, fileSortMTimeCb, fileShowDirsCb,
  fileDirsFirstCb, fileCloseCb, goHomeCb, fileUpCb,
  fileShowHiddenCb, fileEditCb, fileExecCb, commandCb, fileViewCb, fileActionCb, filePropsCb, dirOpenCb, fileExitCb, dirFilterCb, mgcHeadersCb,
  viewTypeCb, sortTypeCb, showHiddenCb, showDirsCb, dirsFirstCb;

void timeoutCb(XtPointer data, XtIntervalId *id);
void changeDirCb(Widget w, XtPointer client_data, XtPointer calldata);
void resetDirFieldCb(Widget w, XtPointer client_data, XtPointer calldata);
void fileRefreshCb(Widget w, XtPointer client_data, XtPointer call_data);
void newFileWinCb(Widget w, XtPointer client_data, XtPointer call_data);
void goUpCb(Widget w, XtPointer client_data, XtPointer call_data);
void xtermCb(Widget w, XtPointer client_data, XtPointer call_data);
void clrDirFieldCb(Widget w, XtPointer client_data, XtPointer call_data);
void dirPopupCb(Widget w, XtPointer client_data, XtPointer call_data);
void newFileProc(XtPointer fnm, int conf);
void fileExecProc(XtPointer fnm, int conf);
void mkDirProc(XtPointer fnm, int conf);

/*---FmFwActions-------------------------------------------------------------*/

#define COPY 1
#define MOVE 2
#define LINK 3

typedef enum { SingleFile, MultipleFiles, Executable, Directory } FileType;

FmActionProc fileBeginDrag, treeOpenDir, fileExecAction, resetCursor, filePopup, parentsPopup, dirPopup;

int findFileItem(FileWindowRec *fw, Widget button);
FileWindowRec *findFileWidget(Widget w, int *item);
FileWindowRec *findFileWidgetByForm(Widget form);
FileWindowRec *findFileWidgetByField(Widget dirfield);
void fileSelectCb(Widget w, XEvent *event, String *params, Cardinal *num_params);
void fileToggleCb(Widget w, XEvent *event, String *params, Cardinal *num_params);
void selectBlockCb(Widget w, XEvent *event, String *params, Cardinal *num_params);
void fileOpenCb(Widget w, XEvent *event, String *params, Cardinal *num_params);
void buttonPopup(Widget w, XEvent *event, String *params, Cardinal *num_params);
void newAppWinCb(Widget w, XtPointer client_data, XtPointer call_data);
void fileRenameCb(Widget w, XtPointer client_data, XtPointer call_data);
void fileCopyCb(Widget w, XtPointer client_data, XtPointer call_data);
void fileMoveCb(Widget w, XtPointer client_data, XtPointer call_data);
void fileLinkCb(Widget w, XtPointer client_data, XtPointer call_data);
void renameFileProc(XtPointer fsel, int conf);
void moveFilesProc(XtPointer fsel, int conf);
void overwriteProc(XtPointer fsel, int conf);
void selectAdd(FileWindowRec *fw, String pattern, Boolean replace_sel);
void selectRemove(FileWindowRec *fw, String pattern);
void filterFiles(FileWindowRec *fw, String pattern);

/*--FmOps---------------------------------------------------------------------*/

typedef struct
{
    String from, to;
    Boolean move;
    String dirToBeRemoved;
} CopyRec;


typedef struct
{
    int n_alc, first, last;
    int src, dest;
    CopyRec *copies;
    XtWorkProcId wpID;
} CopyDataRec;

extern CopyDataRec cpy;

int makeLink(String from, String to);
int movefile(String from, String to);
void rcopy(char *oldpath, char *newpath, Boolean move_flag);
Boolean copyWorkProc(XtPointer);
void copyErrorProc(XtPointer ptr, int conf);

/*--FmDialogs----------------------------------------------------------------*/

void deleteFilesDialog(SelFileNamesRec *files);
void renameDialog(SelFileNamesRec *files);
void copyFilesDialog(SelFileNamesRec *files);
void moveFilesDialog(SelFileNamesRec *files);
void linkFilesDialog(SelFileNamesRec *files);
void deleteDirDialog(SelFileNamesRec *files, String dirname);
void overwriteDialog(SelFileNamesRec *files, String filename, String op, int dirflag);
void unmountDialog(UnmountProcRec *data);
void aboutCb(Widget w, XtPointer client_data, XtPointer call_data);
void installDialog(void);
int opError(Widget shell, String msg, String fname);
void selectionDialog(FileWindowRec *fw);
void filterDialog(FileWindowRec *fw);
void copyError(String from, String to);
void createCopyDialog(void);
void copyInfoProc(Widget w, XtPointer client_data, XtPointer call_data);
void updateCopyDialog(void);

/*--FmDragDrop-------------------------------------------------------------------*/

Boolean fileConvProc(Widget w, Atom *selection, Atom *target, Atom *type_return, XtPointer *value_return, unsigned long *length_return, int *format_return);
void fileDropFinish(Widget w, XtPointer client_data, XtPointer call_data);
void startFileDrag(Widget w, XEvent *event, String *params, Cardinal *num_params);
void handleFileWinDrop(Widget w, XtPointer client_data, XtPointer call_data);
void fileTransProc(Widget w, XtPointer client_data, Atom *seltype, Atom *type, XtPointer value, unsigned long *length, int format);
void handleFileDrop(Widget w, XtPointer client_data, XtPointer call_data);

/*--FmChmod------------------------------------------------------------------*/

void propsDialog(SelFileNamesRec *fnames);

/*--FmDelete-----------------------------------------------------------------*/

FmCallbackProc fileDeleteCb, emptyDir;
void deleteFilesProc(XtPointer fsel, int conf);
void deleteDirProc(XtPointer fsel, int conf);
void rdelete(char *path);
void rmDirs(char *path);

/* functions */

void initUtils();

Widget *createMenu(String menu_name, String menu_label, MenuItemList items, Cardinal n_items, Dimension left_margin, Widget parent, XtPointer client_data);

void fillIn(Widget w);
void grayOut(Widget w);
void tick(Widget w);
void noTick(Widget w);
void popupByCursor(Widget shell, XtGrabKind grab_kind);
Widget *createFloatingMenu(String menu_name, MenuItemRec *items, Cardinal n_items, Dimension left_margin, Widget parent, XtPointer client_data, Widget *menu_widget);
void execArgProc(XtPointer pcom, int conf);

/*--FmMount---------------------------------------------------------------------*/

int devAction(char *action);
void readDevices(String path);
int checkDevices(void);
int findDev(String path);
int mountDev(int d, Boolean cpyflag);	/* return 0 if OK */
void umountDev(int d, Boolean cpyflag);
void unmountProc(XtPointer client_data, XtIntervalId *id);
void unmountDlgProc(XtPointer client_data, int answer);
void unmountAll(void);
void displayMountTable(void);
void updateMountDisplay(DeviceRec *dev);
void saveMountWindow(FILE *fout);

/*--FmIcons--------------------------------------------------------------*/

void fileOnDeskCb(Widget w, XtPointer client_data, XtPointer call_data);

#endif
