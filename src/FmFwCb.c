/*---------------------------------------------------------------------------
  Module FmFwCb

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

  Callback routines for widgets in a file window
---------------------------------------------------------------------------*/

#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include "Fm.h"

typedef struct
{
    String directory, name;
    Widget shell;
} NewFileRec;


typedef struct
{
    String directory, action, arguments;
    Widget shell;
    IconRec icon;
} ExecFileRec;

/*---------------------------------------------------------------------------*/

void fileEditCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 int item;

 XtVaGetValues(fw->form, XmNuserData, (XtPointer) &item, NULL);
 doEdit(fw->directory, fw->files[item]->name);
}

/*---------------------------------------------------------------------------*/

void fileExecCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 ExecFileRec *data = (ExecFileRec *) XtMalloc(sizeof(ExecFileRec));
 TextFieldRec argLine[2];
 int item;

 XtVaGetValues(fw->form, XmNuserData, (XtPointer) &item, NULL);
 data->directory = XtNewString(fw->directory);
 data->action = (String) XtMalloc((sizeof(fw->files[item]->name) + 8) * sizeof(char));
 strcpy(data->action, "exec ./");
 strcat(data->action, fw->files[item]->name);
 data->shell = fw->shell;
 data->icon = icons[EXEC_BM];
 argLine[0].label = "Arguments:";
 *(argLine[0].value = &data->arguments) = NULL;
 argLine[1].label = NULL;
 textFieldDialog(fw->shell, argLine, fileExecProc, (XtPointer) data, data->icon.bm);
}

/*---------------------------------------------------------------------------*/

void fileExecProc(XtPointer fnm, int conf)
{
 ExecFileRec *data = (ExecFileRec *) fnm;
 String action;

 if (conf == YES)
 {
     action = (String) XtMalloc((strlen(data->action) + strlen(data->arguments) + 2) * sizeof(char));
     sprintf(action, "%s %s", data->action, data->arguments);
     performAction(data->shell, data->icon.bm, action, data->directory, 0, NULL);
     XTFREE(action);
 }
 XTFREE(data->arguments);
 XTFREE(data->directory);
 XTFREE(data->action);
 XTFREE(data);
}

/*---------------------------------------------------------------------------*/

void commandCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 ExecFileRec *data = (ExecFileRec *) XtMalloc(sizeof(ExecFileRec));
 TextFieldRec actionLine[2];
 int item;

 XtVaGetValues(fw->form, XmNuserData, (XtPointer) &item, NULL);
 data->directory = XtNewString(fw->directory);
 data->arguments = XtNewString("");
 data->shell = fw->shell;
 data->icon = icons[DIR_BM];
 actionLine[0].label = "Shell command:";
 *(actionLine[0].value = &data->action) = NULL;
 actionLine[1].label = NULL;
 textFieldDialog(fw->shell, actionLine, fileExecProc, (XtPointer) data, data->icon.bm);
}

/*---------------------------------------------------------------------------*/

void fileViewCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 int item;

 XtVaGetValues(fw->form, XmNuserData, (XtPointer) &item, NULL);
 doView(fw->directory, fw->files[item]->name);
}

/*---------------------------------------------------------------------------*/

void fileActionCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 TextFieldRec actionLine[2];
 ExecFileRec *data = (ExecFileRec *) XtMalloc(sizeof(ExecFileRec));
 SelFileNamesRec *fnames;
 size_t size = 1;
 int i;

 fnames = getSelFiles(fw);
 if (!fnames)  return;
 data->directory = XtNewString(fw->directory);
 for (i=0; i<fnames->n_sel; i++)
     size += strlen(fnames->names[i]) + 1;
 data->arguments = (String) XtMalloc(size * sizeof(char));
 data->arguments[0] = 0;
 for (i=0; i<fnames->n_sel; i++)
 {
     strcat(data->arguments, fnames->names[i]);
     strcat(data->arguments, " ");
 }
 data->shell = fw->shell;
 data->icon = fnames->icon;
 freeSelFiles(fnames);
 actionLine[0].label = "Action:";
 *(actionLine[0].value = &data->action) = NULL;
 actionLine[1].label = NULL;
 textFieldDialog(fw->shell, actionLine, fileExecProc, (XtPointer) data, data->icon.bm);
}

/*---------------------------------------------------------------------------*/

void filePropsCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SelFileNamesRec *fnames;

 fnames = getSelFiles(fw);
 if (fnames) propsDialog(fnames);
}

/*---------------------------------------------------------------------------*/

void dirOpenCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 char path[MAXPATHLEN];
 int item;

 zzz();
 XtVaGetValues(fw->form, XmNuserData, (XtPointer) &item, NULL);
 strcpy(path, fw->directory);
 if (path[strlen(path)-1] != '/')  strcat(path, "/");
 strcat(path, fw->files[item]->name);
 fnexpand(path);
 contractPath(path);
 newFileWindow(path, fw, NULL);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void newFileCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 TextFieldRec nameLine[2];
 NewFileRec *nfr = (NewFileRec *) XtMalloc(sizeof(NewFileRec));

 nfr->directory = XtNewString(fw->directory);
 nfr->shell = fw->shell;
 nameLine[0].label = "File name:";
 *(nameLine[0].value = &nfr->name) = NULL;
 nameLine[1].label = NULL;
 textFieldDialog(fw->shell, nameLine, newFileProc, (XtPointer) nfr, icons[FILE_BM].bm);
}

/*---------------------------------------------------------------------------*/

void newFileProc(XtPointer fnm, int conf)
{
 NewFileRec *nfr = (NewFileRec *) fnm;
 int desc;

 if (conf == YES)
 {
     if (chdir(nfr->directory))
	 sysError(nfr->shell, "Can't change to directory:");
     else if ((desc = create(nfr->name, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
	 sysError(nfr->shell, "Can't create file:");
     else
     {
	 close(desc);
	 markForUpdate(nfr->directory, CHECK_FILES);
	 intUpdate(CHECK_DIR);
     }
     chdir(user.home);
     XTFREE(nfr->name);
 }
 XTFREE(nfr->directory);
 XTFREE(nfr);
}

/*---------------------------------------------------------------------------*/

void mkDirCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 TextFieldRec nameLine[2];
 NewFileRec *nfr = (NewFileRec *) XtMalloc(sizeof(NewFileRec));

 nfr->directory = XtNewString(fw->directory);
 nfr->shell = fw->shell;
 nameLine[0].label = "Directory name:";
 *(nameLine[0].value = &nfr->name) = NULL;
 nameLine[1].label = NULL;
 textFieldDialog(fw->shell, nameLine, mkDirProc, (XtPointer) nfr, icons[DIR_BM].bm);
}

/*---------------------------------------------------------------------------*/

void mkDirProc(XtPointer fnm, int conf)
{
 NewFileRec *nfr = (NewFileRec *) fnm;

 if (conf == YES)
 {
     if (chdir(nfr->directory))
	 sysError(nfr->shell, "Can't change to directory:");
     else if (mkdir(nfr->name, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
	 sysError(nfr->shell, "Can't make directory:");
     else
     {
	 markForUpdate(nfr->directory, CHECK_FILES);
	 intUpdate(CHECK_DIR);
     }
     XTFREE(nfr->name);
 }
 XTFREE(nfr->directory);
 XTFREE(nfr);
}

/*---------------------------------------------------------------------------*/

void deselectCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 unselectAll((FileWindowRec *) client_data);
 updateStatus((FileWindowRec *) client_data);
}

/*---------------------------------------------------------------------------*/

void editFileConfCb(Widget w, XtPointer client_data, XtPointer calldata)
{
 zzz();
 doEdit(resources.appcfg_path, resources.cfg_file);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void editFileMonCb(Widget w, XtPointer client_data, XtPointer calldata)
{
 zzz();
 doEdit(resources.appcfg_path, resources.mon_file);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void selectAllCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 selectAll((FileWindowRec *) client_data);
 updateStatus((FileWindowRec *) client_data);
}

/*---------------------------------------------------------------------------*/

void selectCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 selectionDialog((FileWindowRec *) client_data);
}

/*---------------------------------------------------------------------------*/

void fsInfoCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 fsInfo((FileWindowRec *) client_data);
}

/*---------------------------------------------------------------------------*/

void fileRefreshCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 updateFileDisplay(fw, RESHOW);
}

/*---------------------------------------------------------------------------*/

void goHomeCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 chFileDir((FileWindowRec *) client_data, user.home);
}

/*---------------------------------------------------------------------------*/

void newFileWinCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 zzz();
 newFileWindow(fw->directory, fw, NULL);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void mountTableCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 if (!mntable.dialog)
     displayMountTable();
 else
     XMapRaised(dpy, XtWindow(mntable.dialog));
}

/*---------------------------------------------------------------------------*/

void goUpCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 char path[MAXPATHLEN];

 if (strcmp(fw->directory, "/"))
 {
     strcpy(path, fw->directory);
     strcat(path, "/..");
     contractPath(path);
     chFileDir(fw, path);
 }
}

/*---------------------------------------------------------------------------*/

void xtermCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 String *argv;

 zzz();
 argv = (String *) XtMalloc(4 * sizeof(String));
 argv[0] = user.shell;
 argv[1] = "-c";
 argv[2] = resources.xterm;
 argv[3] = NULL;

 executeApplication(user.shell, fw->directory, argv);

 XTFREE(argv);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void clrDirFieldCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 XmTextFieldSetString(fw->dirfield, fw->directory);
}

/*---------------------------------------------------------------------------*/

void showHiddenCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 fw->show_hidden = ((XmToggleButtonCallbackStruct *) call_data)->set ? True: False;
 updateFileDisplay(fw, RESHOW);
}

/*---------------------------------------------------------------------------*/

void showDirsCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 fw->show_dirs = ((XmToggleButtonCallbackStruct *) call_data)->set ? True: False;
 XtVaSetValues(fw->dirs_first_button, XmNsensitive, fw->show_dirs, NULL);
 updateFileDisplay(fw, RESHOW);
}

/*---------------------------------------------------------------------------*/

void dirsFirstCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 fw->dirs_first = ((XmToggleButtonCallbackStruct *) call_data)->set ? True: False;
 updateFileDisplay(fw, RESHOW);
}

/*---------------------------------------------------------------------------*/

void viewTypeCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 DisplayType olddisp = fw->display_type;

 if (w == fw->icon_view_button)
     fw->display_type = Icons;
 else  fw->display_type = Text;
 XmToggleButtonGadgetSetState(fw->icon_view_button, (fw->display_type == Icons), False);
 XmToggleButtonGadgetSetState(fw->text_view_button, (fw->display_type == Text), False);
 if (fw->display_type != olddisp)
 {
     fw->n_rows = 0;	/* Set Scrollbar to top */
     updateFileDisplay(fw, RESHOW);
 }
}

/*---------------------------------------------------------------------------*/

void sortTypeCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SortType oldsort = fw->sort_type;

 if (w == fw->sort_name_button)
     fw->sort_type = SortByName;
 else if (w == fw->sort_size_button)
     fw->sort_type = SortBySize;
 else  fw->sort_type = SortByMTime;
 XmToggleButtonGadgetSetState(fw->sort_name_button, (fw->sort_type == SortByName), False);
 XmToggleButtonGadgetSetState(fw->sort_size_button, (fw->sort_type == SortBySize), False);
 XmToggleButtonGadgetSetState(fw->sort_date_button, (fw->sort_type == SortByMTime), False);
 if (fw->sort_type != oldsort)
     updateFileDisplay(fw, RESHOW);
}

/*---------------------------------------------------------------------------*/

#ifdef MAGIC_HEADERS
void mgcHeadersCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 fw->magic_headers = ((XmToggleButtonCallbackStruct *) call_data)->set ? True: False;
 if (fw->magic_headers)
 {
     fw->slow_device = False;
     updateFileDisplay(fw, RESHOW);
 }
}
#endif

/*---------------------------------------------------------------------------*/

void dirPopupCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 MenuItemRec *menu;

 if ((menu = fw->dirfieldPopupMenu))
 {
     while (menu->label && menu->object != w)
	 menu++;
     if (menu->label)
     {
	 if (resources.newwin_on_dirpopup)
	 {
	     zzz();
	     newFileWindow(menu->label, fw, NULL);
	     wakeUp();
	 }
	 else
	     chFileDir(fw, menu->label);
     }
 }
}

/*---------------------------------------------------------------------------*/

void timeoutCb(XtPointer data, XtIntervalId *id)
{
 FileWindowRec *fw = (FileWindowRec *) data;

 fw->timeoutId = 0;
 updateFileDisplay(fw, (resources.check_files)? CHECK_FILES : CHECK_DIR);
}

/*---------------------------------------------------------------------------*/

void changeDirCb(Widget w, XtPointer client_data, XtPointer calldata)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 char *newpath, dir[MAXPATHLEN];

 newpath = XmTextFieldGetString(w);
 if (newpath[0] != '/' && newpath[0] != '~' && newpath[0] != 0)
 {
     strcpy(dir, fw->directory);
     if (dir[strlen(dir)-1] != '/')  strcat(dir, "/");
 }
 else dir[0] = 0;
 strcat(dir, newpath);
 XTFREE(newpath);
 fnexpand(dir);
 contractPath(dir);
 if (!chFileDir(fw, dir))
     XmTextFieldSetString(fw->dirfield, fw->directory);
}

/*---------------------------------------------------------------------------*/

void resetDirFieldCb(Widget w, XtPointer client_data, XtPointer calldata)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 XmTextFieldSetString(fw->dirfield, fw->directory);
}

/*---------------------------------------------------------------------------*/

void fileExitCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 exitDialog(((FileWindowRec *) client_data)->shell);
}

/*---------------------------------------------------------------------------*/

void dirFilterCb(Widget w, XtPointer client_data, XtPointer calldata)
{
 filterDialog((FileWindowRec *) client_data);
}
