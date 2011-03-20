/*-----------------------------------------------------------------------------
  Module FmFwActions.c                                                             

  (c) Simon Marlow 1991
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996
                                                                           
  functions for file window actions                                                   
-----------------------------------------------------------------------------*/

#include <Xm/RowColumn.h>
#include <Xm/TextF.h>

#include "Am.h"
#include "Fm.h"

/*---------------------------------------------------------------------------*/

static String no_target = "Can't find target directory";

/*---------------------------------------------------------------------------*/

int findFileItem(FileWindowRec *fw, Widget button)
{
 int i;

 for (i=0; i < fw->n_files; i++)
 {
     if (fw->files[i]->form == button)  return i;
 }
 return -1;
}

/*---------------------------------------------------------------------------*/

FileWindowRec *findFileWidget(Widget w, int *item)
{
 FileWindowRec *fw;
 int i;

 for (fw = file_windows; (fw); fw = fw->next)
 {
     for (i=0; i < fw->n_files; i++)
     {
	 if (fw->files[i]->form == w)
	 {
	     *item = i;
	     return fw;
	 }
     }
 }
 return NULL;
}

/*---------------------------------------------------------------------------*/

FileWindowRec *findFileWidgetByForm(Widget form)
{
 FileWindowRec *fw;

 for (fw = file_windows; fw; fw = fw->next)
 {
     if (fw->form == form)  return fw;
 }
 return NULL;
}

/*---------------------------------------------------------------------------*/

void fileSelectCb(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 FileWindowRec *fw;
 int i;

 if ((fw = findFileWidget(w, &i)))  fileSelect(fw, i);
}

/*---------------------------------------------------------------------------*/

void fileToggleCb(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 FileWindowRec *fw;
 int i;

 if ((fw = findFileWidget(w, &i)))  fileToggle(fw, i);
}

/*---------------------------------------------------------------------------*/

void selectBlockCb(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 FileWindowRec *fw;
 SelectionRec *sel;
 int i, first, last, k;

 if ((fw = findFileWidget(w, &i)))
 {
     sel = &fw->selected;
     if (sel->n_sel == 1)
     {
	 if ((k = sel->file[0].nr) < i)
	 {
	     first = k + 1;  last = i;
	 }
	 else
	 {
	     first = i;  last = k - 1;
	 }
	 for (i = first; i <= last; i++)
	 {
	     if ((k = sel->n_sel++) >= sel->n_alloc)
		 sel->file = (FileSpec *) XTREALLOC(sel->file, (sel->n_alloc += 10) * sizeof(FileSpec));
	     sel->file[k].nr = i;
	     sel->file[k].name = XtNewString(fw->files[i]->name);
	     sel->n_bytes += fw->files[i]->stats.st_size;
	     XtVaSetValues(fw->files[i]->form, XmNborderColor, resources.select_color, NULL);
	 }
	 updateStatus(fw);
     }
 }
}

/*---------------------------------------------------------------------------*/

void fileOpenCb(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 FileWindowRec *fw;
 FileRec *file;
 int item;
 char path[MAXPATHLEN];
 String *argv, push_action;

 if (!(fw = findFileWidget(w, &item)))  return;
 zzz();
 file = fw->files[item];
 strcpy(path, fw->directory);
 if (path[strlen(path)-1] != '/')  strcat(path, "/");
 strcat(path, file->name);
 if (S_ISDIR(file->stats.st_mode))
 {
     contractPath(path);
     chFileDir(fw, path);
 }
 else if (file->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
 {
     String arguments = XtNewString(file->name);
     String action = (String) XtMalloc((strlen(path) + 6) * sizeof(char));

     strcat(strcpy(action, "exec "), path);
     argv = makeArgv(action, 1, &arguments);
     executeApplication(user.shell, fw->directory, argv);
     XTFREE(action);
     freeArgv(argv);
 }
 else if (file->type)
 {
     if (*(push_action = file->type->push_action))
     {
	 if (!(strcmp(push_action, "EDIT")))
	     doEdit(fw->directory, file->name);
	 else if (!(strcmp(push_action, "VIEW")))
	     doView(fw->directory, file->name);
	 else if (!(strcmp(push_action, "LOAD")))
	 {
	     zzz();
	     newApplicationWindow(path, NULL);
	     wakeUp();
	 }
	 else
	 {
	     argv = (String *) XtMalloc(sizeof(String));
	     argv[0] = XtNewString(file->name);
	     performAction(fw->shell, file->type->icon_pm.bm, push_action, fw->directory, 1, argv);
	     XTFREE(argv);
	 }
     }
 }
 else doEdit(fw->directory, file->name);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void filePopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 Window root, child, win;
 Widget button;
 FileWindowRec *fw;
 int x, y, x_win, y_win;
 unsigned int mask;
 int item;

 fw = findFileWidgetByForm(w);
 if (!fw || fw->scrolled == None)  return;
 win = XtWindow(fw->icon_box);
 XQueryPointer(dpy, win, &root, &child, &x, &y, &x_win, &y_win, &mask);
 if (child != None)
 {
     button = XtWindowToWidget(dpy, child);
     item = findFileItem(fw, button);
     XtVaSetValues(fw->form, XmNuserData, (XtPointer) item, NULL);
     fileSelect(fw, item);
     if (S_ISDIR(fw->files[item]->stats.st_mode))
     {
	 XmMenuPosition(fw->dirPopup, (XButtonPressedEvent *) event);
	 XtManageChild(fw->dirPopup);
     }
     else if (fw->files[item]->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
     {
	 XmMenuPosition(fw->execPopup, (XButtonPressedEvent *) event);
	 XtManageChild(fw->execPopup);
     }
     else
     {
	 XmMenuPosition(fw->filePopup, (XButtonPressedEvent *) event);
	 XtManageChild(fw->filePopup);
     }
 }
 else
 {
     XmMenuPosition(fw->formPopup, (XButtonPressedEvent *) event);
     XtManageChild(fw->formPopup);
 }
}

/*---------------------------------------------------------------------------*/

void parentsPopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 FileWindowRec *fw;

 for (fw = file_windows; fw; fw = fw->next)
 {
     if (w == fw->dirfield)  break;
 }
 if (fw)
 {
     XmMenuPosition(fw->dirfieldPopup, (XButtonPressedEvent *) event);
     XtManageChild(fw->dirfieldPopup);
 }
}

/*---------------------------------------------------------------------------*/

void newAppWinCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 zzz();
 newApplicationWindow(NULL, NULL);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void fileRenameCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SelFileNamesRec *fnames;
 TextFieldRec targetLine[2];

 fnames = getSelFiles(fw);
 if (!fnames)  return;
 fnames->op = MOVE;
 targetLine[0].label = "New name:";
 *(targetLine[0].value = &fnames->target) = XtNewString(fnames->names[0]);
 targetLine[1].label = NULL;
 textFieldDialog(fw->shell, targetLine, renameFileProc, (XtPointer) fnames, fnames->icon.bm);
}

/*---------------------------------------------------------------------------*/

void renameFileProc(XtPointer fsel, int conf)
{
 SelFileNamesRec *fnames = (SelFileNamesRec *) fsel;
 struct stat stats;
 char to[MAXPATHLEN];

 if (conf != YES)
 {
     freeSelFiles(fnames);
     return;
 }
 if (chdir(fnames->directory))
 {
     sysError(fnames->shell, "System error:");
     freeSelFiles(fnames);
     return;
 }
 chdir(user.home);
 if (fnames->target[0] != '/' && fnames->target[0] != '~' && fnames->target != 0)
 {
     strcpy(to, fnames->directory);
     if (to[strlen(to)-1] != '/')  strcat(to, "/");
 }
 else to[0] = 0;
 strcat(to, fnames->target);
 fnexpand(to);
 XTFREE(fnames->target);
 fnames->target = XtNewString(to);
 if (!(stat(to, &stats)) && S_ISDIR(stats.st_mode))
     renameDialog(fnames);
 else moveFilesProc(fsel, YES);
}

/*---------------------------------------------------------------------------*/

void fileCopyCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SelFileNamesRec *fnames;
 TextFieldRec targetLine[2];

 fnames = getSelFiles(fw);
 if (!fnames)  return;
 fnames->op = COPY;
 targetLine[0].label = "Copy to:";
 *(targetLine[0].value = &fnames->target) = NULL;
 targetLine[1].label = NULL;
 textFieldDialog(fw->shell, targetLine, moveFilesProc, (XtPointer) fnames, fnames->icon.bm);
}

/*---------------------------------------------------------------------------*/

void fileMoveCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SelFileNamesRec *fnames;
 TextFieldRec targetLine[2];

 fnames = getSelFiles(fw);
 if (!fnames)  return;
 fnames->op = MOVE;
 targetLine[0].label = "Move to:";
 *(targetLine[0].value = &fnames->target) = NULL;
 targetLine[1].label = NULL;
 textFieldDialog(fw->shell, targetLine, moveFilesProc, (XtPointer) fnames, fnames->icon.bm);
}

/*---------------------------------------------------------------------------*/

void fileLinkCb(Widget w, XtPointer client_data , XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 SelFileNamesRec *fnames;
 TextFieldRec targetLine[2];

 fnames = getSelFiles(fw);
 if (!fnames)  return;
 fnames->op = LINK;
 targetLine[0].label = "Link to:";
 *(targetLine[0].value = &fnames->target) = NULL;
 targetLine[1].label = NULL;
 textFieldDialog(fw->shell, targetLine, moveFilesProc, (XtPointer) fnames, fnames->icon.bm);
}

/*---------------------------------------------------------------------------*/

void moveFilesProc(XtPointer fsel, int conf)
{
 SelFileNamesRec *fnames = (SelFileNamesRec *) fsel;
 struct stat tostats, tolstats, frstats;
 char from[MAXPATHLEN], to[MAXPATHLEN];
 String name, cwd;
 String op_name, from_err, to_err;
 size_t toend, fromend;
 int devto, devfrom, deverr = -1, i, perm, res;
 Boolean is_dir, is_link;

 if (conf != YES || !fnames->op)
 {
     freeSelFiles(fnames);
     return;
 }
 if (chdir(fnames->directory))
 {
     sysError(fnames->shell, "System error:");
     freeSelFiles(fnames);
     return;
 }
 chdir(user.home);
 switch (fnames->op)
 {
     case COPY:
	 op_name = "Copy:";
	 from_err = "Error copying";
	 to_err = "Error copying to";
	 break;
     case MOVE:
	 op_name = "Move:";
	 from_err = "Error moving";
	 to_err = "Error moving to";
	 break;
     default:  /* i.e. LINK */
	 op_name = "Link:";
	 from_err = "Error creating symlink to";
	 to_err = "Error creating symlink in";
 }
 if (fnames->target[0] != '/' && fnames->target[0] != '~' && fnames->target != 0)
 {
     strcpy(to, fnames->directory);
     if (to[strlen(to)-1] != '/')  strcat(to, "/");
 }
 else to[0] = 0;
 strcat(to, fnames->target);
 fnexpand(to);
 if (!(cwd = absolutePath(to)))
 {
     error(fnames->shell, no_target, to);
     freeSelFiles(fnames);
     return;
 }
 strcpy(to, cwd);
 XTFREE(cwd);
 fromend = strlen(strcpy(from, fnames->directory));
 if (from[fromend-1] != '/')
 {
     from[fromend++] = '/';
     from[fromend] = 0;
 }
 devfrom = findDev(from);
 if (mountDev(devto = findDev(to), False))
     deverr = devto;
 else if (mountDev(devfrom, False))
     deverr = devfrom;
 if (deverr != -1)
 {
     error(fnames->shell, "Cannot mount device on", mntable.devices[deverr].def_mpoint);
     umountDev(devto, False);
     freeSelFiles(fnames);
     return;
 }
 if (!(stat(to, &tostats)) && S_ISDIR(tostats.st_mode))
 {
     if (chdir(to) || !(perm = permission(&tostats, P_WRITE)))
     {
	 chdir(user.home);
	 if (!perm)
	     error(fnames->shell, "You have no write permission for ", to);
	 else  sysError(fnames->shell, "System error:");
	 umountDev(devto, False);
	 umountDev(devfrom, False);
	 freeSelFiles(fnames);
	 return;
     }
     chdir(user.home);
     fnames->dirtarget = True;
     toend = strlen(to);
     if (to[toend-1] != '/')
     {
	 to[toend++] = '/';
	 to[toend] = 0;
     }
 }
 else if (fnames->n_sel == 1)  fnames->dirtarget = False;
 else
 {
     error(fnames->shell, op_name, "Target for multiple files must be a folder");
     umountDev(devto, False);
     umountDev(devfrom, False);
     freeSelFiles(fnames);
     return;
 }
 if (!fnames->first)  zzz();
 XTFREE(fnames->target);
 fnames->target = XtNewString(to);
 for (i = fnames->first; i < fnames->n_sel; i++)
 {
     name = fnames->names[i];
     if (fnames->op != LINK && (!strcmp(name, ".") || !strcmp(name, "..")))
     {
	 error(fnames->shell, "Cannot move or copy . or ..", NULL);
	 continue;
     }
     strcpy(from+fromend, name);
     if (fnames->dirtarget)
     {
	 if (fnames->op != LINK && prefix(from, to))
	 {
	     String err_str, format = "Cannot move or copy %s to";
	     err_str = (String) XtMalloc((strlen(format) + strlen(from)) * sizeof(char));
	     sprintf(err_str, format, from);
	     error(fnames->shell, err_str, to);
	     XTFREE(err_str);
	     umountDev(devto, False);
	     umountDev(devfrom, False);
	     freeSelFiles(fnames);
	     wakeUp();
	     return;
	 }
	 strcpy(to+toend, name);
     }
     if (!(lstat(to, &tolstats)))
     {
	 fnames->first = i;
	 is_dir = False;
	 if (!(stat(to, &tostats)))
	 {
	     if (S_ISDIR(tostats.st_mode))
		 is_dir = True;
	     if (!stat(from, &frstats) && tostats.st_ino == frstats.st_ino)
	     {
		 error(fnames->shell, op_name, "Source and destination are identical");
		 umountDev(devto, False);
		 umountDev(devfrom, False);
		 freeSelFiles(fnames);
		 wakeUp();
		 return;
	     }
	 }
	 if (S_ISLNK(tolstats.st_mode))
	     is_link = True;
	 else  is_link = False;

	 if (fnames->conf_ovwr || (is_dir && (!is_link || fnames->op == COPY) && resources.confirm_delete_folder))
	     overwriteDialog(fnames, to, (fnames->op == COPY && is_dir && (lstat(from, &frstats) || !S_ISDIR(frstats.st_mode)))?
			     "File copy:" : op_name,
			     is_dir && (!is_link || fnames->op == COPY));
	 else overwriteProc(fsel, YES);
	 umountDev(devto, False);
	 umountDev(devfrom, False);
	 return;
     }
     switch (fnames->op)
     {
	 case COPY:	rcopy(from, to, False); res = 0; break;
	 case MOVE:	res = movefile(from, to); break;
	 default:	res = makeLink(from, to);
     }
     if (res)
     {
	 if (opError(fnames->shell, from_err, name) != YES)
	     break;
     }
     else  fnames->update = True;
 }
 umountDev(devto, False);
 umountDev(devfrom, False);
 if (fnames->update)
 {
     if (fnames->op == COPY)
	 intUpdate(CHECK_DIR);		/* Check for new subdirectories */
     if (fnames->op == MOVE)
	 markForUpdate(fnames->directory, CHECK_FILES);
     markForUpdate(to, RESHOW);
     if (fnames->op != COPY)
	 intUpdate(CHECK_DIR);
 }
 freeSelFiles(fnames);
 wakeUp();
}

/*---------------------------------------------------------------------------*/

void overwriteProc(XtPointer fsel, int conf)
{
 SelFileNamesRec *fnames = (SelFileNamesRec *) fsel;
 char to[MAXPATHLEN], from[MAXPATHLEN];
 String errstr;
 struct stat tolstats;
 size_t len;
 int devto, devfrom, deverr = -1, res;

 switch (conf)
 {
     case CANCEL:	fnames->first = fnames->n_sel; break;
     case ALL:		fnames->conf_ovwr = False;
     case YES:
	 switch (fnames->op)
	 {
	     case COPY:	errstr = "Error copying"; break;
	     case MOVE: errstr = "Error moving"; break;
	     default: errstr = "Error creating symlink to";
	 }
	 strcpy(from, fnames->directory);
	 if (from[(len = strlen(from)) - 1] != '/')
	     strcat(from, "/");
	 strcat(from, fnames->names[fnames->first]);
	 strcpy(to, fnames->target);
	 devfrom = findDev(from);
	 if (mountDev(devto = findDev(to), False))
	     deverr = devto;
	 else if (mountDev(devfrom, False))
	     deverr = devfrom;
	 if (deverr != -1)
	 {
	     error(fnames->shell, "Cannot mount device on", mntable.devices[deverr].def_mpoint);
	     fnames->first = fnames->n_sel;
	     umountDev(devto, False);
	     break;
	 }
	 if (fnames->dirtarget)  strcat(to, fnames->names[fnames->first]);
	 if (!(lstat(to, &tolstats)))
	 {
	     if (fnames->op != COPY && S_ISDIR(tolstats.st_mode))
		 rdelete(to);
	     else if (fnames->op == LINK || (fnames->op == MOVE && S_ISLNK(tolstats.st_mode)))
	     {
		 unlink(to);
		 fnames->update = True;
	     }
	 }
	 switch (fnames->op)
	 {
	     case COPY:	rcopy(from, to, False); res = 0; break;
	     case MOVE:	res = movefile(from, to); break;
	     default:	res = makeLink(from, to);
	 }
	 if (res)
	 {
	     if (opError(fnames->shell, errstr, from) != YES)
	     {
		 fnames->first = fnames->n_sel;
		 break;
	     }
	 }
	 else  fnames->update = True;
	 umountDev(devto, False);
	 umountDev(devfrom, False);
     case NO:	fnames->first++;
 }
 moveFilesProc(fsel, YES);
}

/*---------------------------------------------------------------------------*/

void selectAdd(FileWindowRec *fw, String pattern, Boolean replace_sel)
{
 SelectionRec *sel = &fw->selected;
 FileRec *file;
 int i, j;
 Boolean changed = False;

 if (replace_sel && fw->selected.n_sel)
 {
     unselectAll(fw);
     changed = True;
 }

 for (i=0; i < fw->n_files; i++)
 {
     if ((replace_sel || getSelNr(fw, i) == -1) && fnmultimatch(pattern, (file = fw->files[i])->name))
     {
	 changed = True;
	 if ((j = sel->n_sel++) >= sel->n_alloc)
	     sel->file = (FileSpec *) XTREALLOC(sel->file, (sel->n_alloc += 10) * sizeof(FileSpec));
	 sel->file[j].nr = i;
	 sel->file[j].name = XtNewString(file->name);
	 sel->n_bytes += file->stats.st_size;
	 XtVaSetValues(file->form, XmNborderColor, resources.select_color, NULL);
     }
 }
 if (changed)  updateStatus(fw);
 XTFREE(pattern);
}

/*---------------------------------------------------------------------------*/

void selectRemove(FileWindowRec *fw, String pattern)
{
 SelectionRec *sel = &fw->selected;
 int i = 0, item;
 Boolean changed = False;

 while (i < sel->n_sel)
 {
     if (fnmultimatch(pattern, sel->file[i].name))
     {
	 changed = True;
	 XTFREE(sel->file[i].name);
	 item = sel->file[i].nr;
	 memcpy(&sel->file[i], &sel->file[--(sel->n_sel)], sizeof(FileSpec));
	 sel->n_bytes -= fw->files[item]->stats.st_size;
	 XtVaSetValues(fw->files[item]->form, XmNborderColor, winInfo.background, NULL);
     }
     else i++;
 }
 if (changed)  updateStatus(fw);
 XTFREE(pattern);
}

/*---------------------------------------------------------------------------*/

void filterFiles(FileWindowRec *fw, String pattern)
{
 strcpy(fw->dirFilter, pattern);
 if (pattern[0])  fw->do_filter = True;
 else  fw->do_filter = False;
 XTFREE(pattern);
 updateFileDisplay(fw, RESHOW);
}
