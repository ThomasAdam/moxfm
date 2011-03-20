/*---------------------------------------------------------------------------
  Module FmChmod

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996

  Functions & data for handling the chmod feature
---------------------------------------------------------------------------*/

#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleBG.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/SeparatoG.h>
#include <Xm/Xm.h>

#include "Fm.h"

#define OWNER 0
#define GROUP 1
#define OTHERS 2

#define READ 0
#define WRITE 1
#define EXECUTE 2

static void chmodProc(Widget w, XtPointer client_data, XtPointer call_data);

/*---------------------------------------------------------------------------
  TYPEDEFS
---------------------------------------------------------------------------*/

typedef struct {
  Widget w;
  int value;
} ChmodItem;

typedef struct
 {
    Widget dialog, w_owner, w_group;
    String file, directory;
    char owner[10], group[10];
    uid_t owner_id;
    gid_t group_id;
    ChmodItem items[3][3];
 } ChmodData;

/*---------------------------------------------------------------------------*/

void propsDialog(SelFileNamesRec *fnames)
{
 ChmodData *chmode;
 Widget form, rowcol, icon, label, subform, line;
 struct stat stats;
 struct passwd *pw;
 struct group *gp;
 char directory[MAXPATHLEN], link[MAXPATHLEN], size[11];
 XmString xmmax;
 String name = fnames->names[0];
 String type, labelstr, infostr;
 String lsc = "Last status change:";
 String classes[] = { "User:", "Group:", "Others:" };
 Arg args[2];
 int i, j, len;
 Dimension width;
 Boolean cont;
 Boolean may_change_owner =
#ifdef linux
(geteuid() == 0);
#else
True;
#endif

 if (!(chmode = (ChmodData *) XtMalloc(sizeof(ChmodData))))
 {
     freeSelFiles(fnames);
     return;
 }
 chmode->file = XtNewString(name);
 chmode->directory = XtNewString(fnames->directory);
 if (chdir(fnames->directory) || !getwd(directory) || lstat(name, &stats))
 {
     chdir(user.home);
     sysError(fnames->shell, "Error determining file properties:");
     freeSelFiles(fnames);
     XTFREE(chmode);
     return;
 }

 link[0] = 0;
 if (S_ISLNK(stats.st_mode))
 {
     type = "Symbolic link";
     len = readlink(name, link, MAXPATHLEN);
     link[len] = 0;
     if (stat(name, &stats))  lstat(name, &stats);
 }
 else if (S_ISDIR(stats.st_mode))
     type = "Directory";
 else if (S_ISCHR(stats.st_mode))
     type = "Character special file";
 else if(S_ISBLK(stats.st_mode))
     type = "Block special file";
 else if(S_ISSOCK(stats.st_mode))
     type = "Socket";
 else if(S_ISFIFO(stats.st_mode))
     type = "Pipe or FIFO special file";
 else  type = "Ordinary file";

 chdir(user.home);
 chmode->owner_id = stats.st_uid;
 chmode->group_id = stats.st_gid;
 sprintf(size, "%lu", (unsigned long) stats.st_size);
 if ((pw = getpwuid(stats.st_uid)))
     strcpy(chmode->owner, pw->pw_name);
 else  sprintf(chmode->owner, "%lu", (unsigned long) stats.st_uid);
 if ((gp = getgrgid(stats.st_gid)))
     strcpy(chmode->group, gp->gr_name);
 else  sprintf(chmode->group, "%lu", (unsigned long) stats.st_gid);

 chmode->items[OWNER][READ].value     = (stats.st_mode) & S_IRUSR;
 chmode->items[OWNER][WRITE].value    = (stats.st_mode) & S_IWUSR;
 chmode->items[OWNER][EXECUTE].value  = (stats.st_mode) & S_IXUSR;

 chmode->items[GROUP][READ].value     = (stats.st_mode) & S_IRGRP;
 chmode->items[GROUP][WRITE].value    = (stats.st_mode) & S_IWGRP;
 chmode->items[GROUP][EXECUTE].value  = (stats.st_mode) & S_IXGRP;

 chmode->items[OTHERS][READ].value    = (stats.st_mode) & S_IROTH;
 chmode->items[OTHERS][WRITE].value   = (stats.st_mode) & S_IWOTH;
 chmode->items[OTHERS][EXECUTE].value = (stats.st_mode) & S_IXOTH;

 i = 0;
 XtSetArg(args[i], XmNautoUnmanage, False);  i++;
 XtSetArg(args[i], XmNforeground, resources.label_color);  i++;
 chmode->dialog = XmCreatePromptDialog(fnames->shell, "Properties", args, i);
 XtUnmanageChild(XmSelectionBoxGetChild(chmode->dialog, XmDIALOG_HELP_BUTTON));
 XtUnmanageChild(XmSelectionBoxGetChild(chmode->dialog, XmDIALOG_SELECTION_LABEL));
 XtUnmanageChild(XmSelectionBoxGetChild(chmode->dialog, XmDIALOG_TEXT));
 XtAddCallback(chmode->dialog, XmNokCallback, chmodProc, (XtPointer) chmode);
 XtAddCallback(chmode->dialog, XmNcancelCallback, chmodProc, (XtPointer) chmode);

 xmmax = XmStringCreateLocalized(lsc);
 width = XmStringWidth(resources.bold_font, xmmax) + 3;
 XmStringFree(xmmax);
 form = XtVaCreateManagedWidget("form", xmFormWidgetClass, chmode->dialog, XmNforeground, resources.label_color, NULL);

 icon = XtVaCreateManagedWidget("Icon", xmLabelGadgetClass, form, XmNlabelType, XmPIXMAP, XmNlabelPixmap, fnames->icon.bm, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, NULL);

 for (i=0; i<7; i++)
 {
     cont = False;
     switch (i)
     {
	 case 0:	labelstr = "Name:"; infostr = name; break;
	 case 1:	labelstr = "Directory:"; infostr = directory; break;
	 case 2:	labelstr = "Size:"; infostr = size; break;
	 case 3:	labelstr = "Type:"; infostr = type; break;
	 case 4:	if (!link[0])  cont = True; else { labelstr = "Symbolic link to:"; infostr = link; } break;
	 case 5:	labelstr = "Last modification:"; infostr = ctime(&stats.st_mtime); break;
	 case 6:	labelstr = lsc; infostr = ctime(&stats.st_ctime);
     }
     if (cont)  continue;

     line = XtVaCreateManagedWidget("line", xmFormWidgetClass, form, XmNforeground, resources.label_color, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, (i)? line : icon, XmNrightAttachment, XmATTACH_FORM, XmNtopOffset, 5, NULL);

     label = XtVaCreateManagedWidget(labelstr, xmLabelGadgetClass, line, XmNfontList, resources.bold_font, XmNwidth, width, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_END, NULL);
     XtVaCreateManagedWidget(infostr, xmLabelGadgetClass, line, XmNfontList, (i)? resources.label_font : resources.bold_font, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
 }
 line = XtVaCreateManagedWidget("line", xmSeparatorGadgetClass, form, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, line, XmNrightAttachment, XmATTACH_FORM, XmNtopOffset, 15, NULL);
 subform = XtVaCreateManagedWidget("subform", xmFormWidgetClass, form, XmNforeground, resources.label_color, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, line, XmNrightAttachment, XmATTACH_FORM, XmNtopOffset, 15, NULL);
 label = XtVaCreateManagedWidget("Owner:", xmLabelGadgetClass, subform, XmNfontList, resources.bold_font, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 chmode->w_owner = XtVaCreateManagedWidget("owner", xmTextFieldWidgetClass, subform, XmNvalue, chmode->owner, XmNfontList, resources.tty_font, XmNeditable, may_change_owner, XmNcursorPositionVisible, may_change_owner, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 label = XtVaCreateManagedWidget("Group:", xmLabelGadgetClass, subform, XmNfontList, resources.bold_font, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, chmode->w_owner, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 chmode->w_group = XtVaCreateManagedWidget("group", xmTextFieldWidgetClass, subform, XmNvalue, chmode->group, XmNfontList, resources.tty_font, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);

 label = XtVaCreateManagedWidget("Permissions:", xmLabelGadgetClass, form, XmNfontList, resources.bold_font, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, subform, XmNalignment, XmALIGNMENT_BEGINNING, XmNtopOffset, 15, NULL);

 rowcol = XtVaCreateManagedWidget("rowcol", xmRowColumnWidgetClass, form, XmNorientation, XmHORIZONTAL, XmNnumColumns, 4, XmNpacking, XmPACK_COLUMN, XmNradioBehavior, False, XmNisHomogeneous, False, XmNisAligned, True, XmNforeground, resources.label_color, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 20, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, label, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
 xmmax = XmStringCreateLocalized("");
 XtVaCreateManagedWidget(" ", xmLabelGadgetClass, rowcol, XmNfontList, resources.bold_font, NULL);
 XtVaCreateManagedWidget("read", xmLabelGadgetClass, rowcol, XmNfontList, resources.bold_font, NULL);
 XtVaCreateManagedWidget("write", xmLabelGadgetClass, rowcol, XmNfontList, resources.bold_font, NULL);
 XtVaCreateManagedWidget("exec.", xmLabelGadgetClass, rowcol, XmNfontList, resources.bold_font, NULL);
 for (i=0; i<3; i++)
 {
     for (j=-1; j<3; j++)
     {
	 if (j < 0)
	     XtVaCreateManagedWidget(classes[i], xmLabelGadgetClass, rowcol, XmNfontList, resources.bold_font, XmNalignment, XmALIGNMENT_END, NULL);
	 else
	 {
	     chmode->items[i][j].w = label =
	       XtVaCreateManagedWidget("toggle", xmToggleButtonGadgetClass, rowcol, XmNlabelString, xmmax, NULL);
	     XmToggleButtonGadgetSetState(label, (chmode->items[i][j].value != 0), False);
	 }
     }
 }

 XmStringFree(xmmax);
 freeSelFiles(fnames);
 XtManageChild(chmode->dialog);
 XtPopup(XtParent(chmode->dialog), XtGrabNone);
}

/*---------------------------------------------------------------------------*/

void chmodProc(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
 ChmodData *chmode = (ChmodData *) client_data;
 struct passwd *pw;
 struct group *gp;
 uid_t owner_id = chmode->owner_id;
 gid_t group_id = chmode->group_id;
 struct stat stats;
 mode_t mode;
 String entry;
 Boolean changed = False;
 int i, j;

 if (cbs->reason == XmCR_OK)
 {
     if (chdir(chmode->directory))
     {
	 sysError(chmode->dialog, "Cannot change to directory:");
	 return;
     }
     entry = XmTextFieldGetString(chmode->w_owner);
     if (entry[0] && strcmp(entry, chmode->owner))
     {
	 if (!(pw = getpwnam(entry)))
	 {
	     error(chmode->dialog, "The specified owner does not exist", NULL);
	     XTFREE(entry);
	     return;
	 }
	 changed = True;
	 owner_id = pw->pw_uid;
     }
     XTFREE(entry);
     entry = XmTextFieldGetString(chmode->w_group);
     if (entry[0] && strcmp(entry, chmode->group))
     {
	 if (!(gp = getgrnam(entry)))
	 {
	     error(chmode->dialog, "The specified group does not exist", NULL);
	     XTFREE(entry);
	     return;
	 }
	 changed = True;
	 group_id = gp->gr_gid;
     }
     XTFREE(entry);
     if (changed)
     {
	 if (chown(chmode->file, owner_id, group_id))
	 {
	     sysError(chmode->dialog, "Could not change owner/group:");
	     return;
	 }
     }
     if (stat(chmode->file, &stats))
     {
	 sysError(chmode->dialog, "Error changing permissions:");
	 return;
     }
     mode = stats.st_mode &
       ~(S_IRUSR | S_IWUSR | S_IXUSR |
	 S_IRGRP | S_IWGRP | S_IXGRP |
	 S_IROTH | S_IWOTH | S_IXOTH);

     for (i=0; i<3; i++)
     {
	 for (j=0; j<3; j++)
	     chmode->items[i][j].value = XmToggleButtonGadgetGetState(chmode->items[i][j].w);
     }

     mode |= chmode->items[OWNER][READ].value     ? S_IRUSR : 0;
     mode |= chmode->items[OWNER][WRITE].value    ? S_IWUSR : 0;
     mode |= chmode->items[OWNER][EXECUTE].value  ? S_IXUSR : 0;

     mode |= chmode->items[GROUP][READ].value     ? S_IRGRP : 0;
     mode |= chmode->items[GROUP][WRITE].value    ? S_IWGRP : 0;
     mode |= chmode->items[GROUP][EXECUTE].value  ? S_IXGRP : 0;

     mode |= chmode->items[OTHERS][READ].value    ? S_IROTH : 0;
     mode |= chmode->items[OTHERS][WRITE].value   ? S_IWOTH : 0;
     mode |= chmode->items[OTHERS][EXECUTE].value ? S_IXOTH : 0;

     if (mode != stats.st_mode)
     {
	 if (chmod(chmode->file, mode))
	 {
	     sysError(chmode->dialog, "Could not change permissions:");
	     return;
	 }
	 changed = True;
     }
     if (changed)
     {
	 markForUpdate(chmode->directory, RESHOW);
	 intUpdate(CHECK_DIR);
     }
 }

 XtDestroyWidget(XtParent(chmode->dialog));
 XTFREE(chmode->file); XTFREE(chmode->directory);
 XTFREE(chmode);
}
