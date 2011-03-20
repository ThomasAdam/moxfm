/*-----------------------------------------------------------------------------
  Module FmDialogs.c                                                             

  (c) Oliver Mai 1995, 1996
                                                                           
  functions for dialog windows                                                   
-----------------------------------------------------------------------------*/

#include <errno.h>
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/MessageB.h>
#include <Xm/PushBG.h>
#include <Xm/Protocols.h>
#include <Xm/SelectioB.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>

#include "Am.h"
#include "Fm.h"

#define TRASHDIR "/.trash"	/* relative to home dir */

#ifndef linux
#ifndef __NetBSD__
extern char *sys_errlist[];
#endif
#endif

static char error_string[MAXPATHLEN+256];

typedef enum { Question, Information, Error, Warning } DialogType;

typedef struct
{
    void (*answerHandlerCB)(XtPointer client_data, int answer);
    Widget no_button;
    XtPointer client_data;
} dlg_cb_struct;

void askQuestion(Widget shell, String lines, Boolean addCancelButton, Boolean addAllButton, Boolean yesno, DialogType style, Pixmap icon, void (*answerHandlerCB)(XtPointer, int), XtPointer client_data);
void dlg_callback(Widget w, XtPointer client_data, XtPointer call_data);
void inst_callback(XtPointer data, int answer);
void readTextProc(Widget dialog, XtPointer client_data, XtPointer call_data);
void sel_callback(Widget dialog, XtPointer client_data, XtPointer call_data);
void fil_callback(Widget dialog, XtPointer client_data, XtPointer call_data);
void operr_callback(XtPointer data, int answer);

void askQuestion(Widget shell, String question, Boolean addCancelButton, Boolean addAllButton, Boolean yesno, DialogType style, Pixmap icon, void (*answerHandlerCB)(XtPointer, int), XtPointer client_data)
{
 Widget dialog, all_button;
 Arg args[5];
 dlg_cb_struct *data = NULL;
 XmString quest = XmStringCreateLtoR(question, XmFONTLIST_DEFAULT_TAG);
 XmString no, yes, all;
 int i = 0;

 if (yesno)
 {
     yes = XmStringCreateLocalized("Yes");
     no = XmStringCreateLocalized("No");
 }
 XtSetArg(args[i], XmNautoUnmanage, True); i++;
 XtSetArg(args[i], XmNmessageString, quest); i++;
 if (yesno)
 {
     XtSetArg(args[i], XmNokLabelString, yes); i++;
     if (!addCancelButton)
     {
	 XtSetArg(args[i], XmNcancelLabelString, no); i++;
     }
 }
 XtSetArg(args[i], XmNforeground, resources.label_color); i++;
 switch (style)
 {
     case Question:	dialog = XmCreateQuestionDialog(shell, "Confirm", args, i); break;
     case Information:	dialog = XmCreateInformationDialog(shell, "Info", args, i); XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON)); break;
     case Error:		dialog = XmCreateErrorDialog(shell, "Error", args, i); break;
     default:		dialog = XmCreateWarningDialog(shell, "Warning", args, i);
 }
 XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
 if (answerHandlerCB)
 {
     data = (dlg_cb_struct*) XtMalloc(sizeof(dlg_cb_struct));
     data->answerHandlerCB = answerHandlerCB;
     data->client_data = client_data;
     XtAddCallback(dialog, XmNokCallback, dlg_callback, (XtPointer) data);
     XtAddCallback(dialog, XmNcancelCallback, dlg_callback, (XtPointer) data);
 }
 if (icon != None)
     XtVaSetValues(XmMessageBoxGetChild(dialog, XmDIALOG_SYMBOL_LABEL), XmNlabelPixmap, icon, NULL);
 if (addCancelButton && data)
 {
     data->no_button = XtVaCreateManagedWidget("no", xmPushButtonGadgetClass, dialog, XmNlabelString, no, NULL);
     XtAddCallback(data->no_button, XmNactivateCallback, dlg_callback, (XtPointer) data);
 }
 if (addAllButton && data)
 {
     all = XmStringCreateLocalized("Overwrite all");
     all_button = XtVaCreateManagedWidget("all", xmPushButtonGadgetClass, dialog, XmNlabelString, all, NULL);
     XtAddCallback(all_button, XmNactivateCallback, dlg_callback, (XtPointer) data);
     XmStringFree(all);
 }
 XmStringFree(quest);
 if (yesno)
 {
     XmStringFree(yes); XmStringFree(no);
 }
 XtManageChild(dialog);
 XtPopup(XtParent(dialog), XtGrabNone);
}

void dlg_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
 dlg_cb_struct *data = (dlg_cb_struct *) client_data;
 void (*answerHandlerCB)(XtPointer, int) = data->answerHandlerCB;
 XtPointer arg = data->client_data;
 String callback_name;
 int answer;

 switch (cbs->reason)
 {
     case XmCR_OK:	answer = YES; callback_name = XmNokCallback; break; /* Yes-button pressed */
     case XmCR_CANCEL:	answer = CANCEL; callback_name = XmNcancelCallback; break; /*Cancel/ No-button pressed */
     default	:	callback_name = XmNactivateCallback;
	 if (w == data->no_button)  answer = NO;
                        else  answer = ALL;
 }
 XtRemoveCallback(w, callback_name, dlg_callback, client_data);
 XTFREE(client_data);
 answerHandlerCB(arg, answer);
}

void confirm(Widget shell, String question, void (*answerHandlerCB)(XtPointer, int), XtPointer client_data)
{
 askQuestion(shell, question, True, False, True, Question, None, answerHandlerCB, client_data);
}

void ask(Widget shell, String question, void (*answerHandlerCB)(XtPointer, int), XtPointer client_data)
{
 askQuestion(shell, question, False, False, True, Question, None, answerHandlerCB, client_data);
}

void exitDialog(Widget shell)
{
 if (!resources.confirm_quit)  exitProc(NULL,YES);
 else askQuestion(shell, "Do you want to exit Moxfm?", False, False, True, Question, None, exitProc, NULL);
}

void deleteFilesDialog(SelFileNamesRec *files)
{
 if (!resources.confirm_deletes)  deleteFilesProc(files, YES);
 else
 {
     sprintf(error_string, "Do you want to delete %d item%s from\n%s?", files->n_sel, files->n_sel > 1 ? "s" : "", files->directory);
     askQuestion(files->shell, error_string, False, False, True, Question, files->icon.bm, deleteFilesProc, (XtPointer) files);
 }
}

void renameDialog(SelFileNamesRec *files)
{
 sprintf(error_string, "%s is a directory.\nDo you want to move %s?", files->target, files->names[0]);
 askQuestion(files->shell, error_string, False, False, True, Question, files->icon.bm, moveFilesProc, (XtPointer) files);
}

void moveFilesDialog(SelFileNamesRec *files)
{
 Boolean confirm;
 String opname;

 switch (files->op)
 {
     case COPY:
	 confirm = resources.confirm_copies;
	 opname = "copy";
	 break;
     case MOVE:
	 confirm = resources.confirm_moves;
	 opname = "move";
	 break;
     default:	/* i.e. LINK */
	 confirm = resources.confirm_links;
	 opname = "link";
 }

 if (!confirm)  moveFilesProc(files, YES);
 else
 {
     sprintf(error_string, "Do you want to %s %d item%s\nfrom %s\nto %s?",
	     opname, files->n_sel, files->n_sel > 1 ? "s" : "", files->directory, files->target);
     askQuestion(files->shell, error_string, False, False, True, Question, files->icon.bm, moveFilesProc, (XtPointer) files);
 }
}

void deleteDirDialog(SelFileNamesRec *files, String dirname)
{
 if (!resources.confirm_delete_folder)  deleteDirProc(files, YES);
 else
 {
     sprintf(error_string, "Do you really wish to delete folder\n%s", files->directory);
     if (error_string[strlen(error_string)-1] != '/')  strcat(error_string, "/");
     strcat(error_string, dirname);
     strcat(error_string, "\nand all items it contains?");
     askQuestion(files->shell, error_string, True, False, True, Warning, None, deleteDirProc, (XtPointer) files);
 }
}

void overwriteDialog(SelFileNamesRec *files, String filename, String op, int dirflag)
{
 sprintf(error_string, "%s %s %s already exists.\n", op, (dirflag)? "Directory" : "File", filename);
 if (!dirflag || strcmp(op, "Copy:"))
 {
     strcat(error_string, "Do you want to overwrite it");
     if (dirflag)
	 strcat(error_string, " and all its contents?");
     else  strcat(error_string, "?");
 }
 else strcat(error_string, "Do you want to proceed and overwrite existing files in it?");
 askQuestion(files->shell, error_string, True, True, True, Warning, None, overwriteProc, (XtPointer) files);
}

void unmountDialog(UnmountProcRec *data)
{
 sprintf(error_string, "Could not unmount %s.\nKeep trying?", mntable.devices[data->device].special);
 askQuestion(getAnyShell(), error_string, False, False, True, Error, None, unmountDlgProc, data);
}

void copyError(String from, String to)
{
 sprintf(error_string, "Error copying %s\nto %s:\n%s", from, to, sys_errlist[errno]);
 askQuestion(getAnyShell(), error_string, False, False, False, Error, None, copyErrorProc, NULL);
}

void removeAppDialog(AppWindowRec *aw, int i)
{
 AppSpecRec *appl = (AppSpecRec *) XtMalloc(sizeof(AppSpecRec));

 appl->win.aw = aw;
 appl->item = i;
 if (!resources.confirm_deletes)  removeProc((XtPointer)appl, YES);
 else askQuestion(aw->shell, "Do you want to remove this application?", False, False, True, Question, aw->apps[i].icon_pm.bm, removeProc, (XtPointer) appl);
}

void overwriteAppsDialog(AppWindowRec *aw)
{
    sprintf(error_string,
	    "Application file %s\nhas changed on disk. Overwrite?",
	    aw->appfile);
    askQuestion(aw->shell, error_string, False, False, True, Warning,
		None, writeApplicationDataProc, (XtPointer) aw);
}

void overwriteIconsDialog(Boolean all)
{
    Boolean *allp = XtMalloc(sizeof(Boolean));
    *allp = all;
    sprintf(error_string,
	    "Application file %s\nhas changed on disk. Overwrite?",
	    resources.dticon_file);
    askQuestion(getAnyShell(), error_string, False, False, True, Warning,
		None, writeDTIconsProc, (XtPointer) allp);
}

void removeIconDialog(DTIconRec *dticon)
{
 if (!resources.confirm_deletes)
     removeIconProc((XtPointer) dticon, YES);
 else askQuestion(XtParent(dticon->app.form), "Do you want to remove this desktop icon?", False, False, True, Question, dticon->app.icon_pm.bm, removeIconProc, (XtPointer) dticon);
}

void aboutCb(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;
 String msg = "This is moxfm v. %s.\n\n"
   "Copyright 1990-96 by Simon Marlow, Albert Graef, Oliver Mai.\n"
   "Copying policy: GNU General Public License\n\n"
   "For documentation see the moxfm(1x) manual page.\n\n"
   "Information about new versions of moxfm can be found at:\n"
   "http://www.musikwissenschaft.uni-mainz.de/~ag/xfm\n";

 sprintf(error_string, msg, version);
 askQuestion(fw->shell, error_string, False, False, False, Information, None, NULL, NULL);
}

void installDialog(void)
{
 String msg = "Welcome!\nIt seems this is the first time\n"
   "you are running moxfm. To proceed,\n"
   "moxfm has to copy some files to\n%s";
 String quest = (String) XtMalloc((strlen(msg) + strlen(resources.appcfg_path_r) * sizeof(char)));
 String dest;
 int answ = -1;
 int i;

 sprintf(quest, msg, resources.appcfg_path_r);
 askQuestion(topShell, quest, False, False, False, Question, None, inst_callback, (XtPointer) &answ);
 while (answ == -1)
     XtAppProcessEvent(app_context, XtIMAll);
 if (answ == YES)
 {
     dest = XtNewString(resources.appcfg_path);
     for (i = strlen(dest)-1; (i > 0 && dest[i] == '/'); i--)
	 dest[i] = 0;
     rcopy(XFMLIBDIR, dest, False);
     XTFREE(dest);
     dest = (String) XtMalloc((strlen(user.home) + strlen(TRASHDIR) + 1) * sizeof(char));
     strcpy(dest, user.home);
     strcat(dest, TRASHDIR);
     while (cpy.last >= cpy.first)
	 XtAppProcessEvent(app_context, XtIMAll);
     mkdir(dest, S_IRUSR | S_IWUSR | S_IXUSR);
     XTFREE(dest);
 }
 else
 {
     XtDestroyApplicationContext(app_context);
     exit(0);
 }
 XTFREE(quest);
}

void inst_callback(XtPointer data, int answer)
{
 int *answ = data;

 *answ = answer;
}

int opError(Widget shell, String msg, String fname)
{
 int answ = -1;

 sprintf(error_string, "%s\n%s:\n%s", msg, fname, sys_errlist[errno]);
 askQuestion(shell, error_string, False, False, False, Error, None, operr_callback, &answ);
 while (answ == -1)
     XtAppProcessEvent(app_context, XtIMAll);
 return answ;
}

void operr_callback(XtPointer data, int answer)
{
 int *answ = data;

 *answ = answer;
}

void saveDialog(Widget shell, XtPointer client_data)
{
 askQuestion(shell, "Do you want to save Application changes?", True, False, True, Question, None, changeAppProc, client_data);
}

void lastSaveDialog(Widget shell)
{
 askQuestion(shell, "Do you want to save all Application changes?", True, False, True, Question, None, quit, NULL);
}

void closeApplicationCB(Widget w, XtPointer client_data, XtPointer call_data)
{
 AppWindowRec *aw = (AppWindowRec *) client_data;

 if (lastWindow())  exitDialog(aw->shell);
 else closeDialog(aw);
}

void closeFileCB(Widget w, XtPointer client_data, XtPointer call_data)
{
 FileWindowRec *fw = (FileWindowRec *) client_data;

 if (lastWindow())  exitDialog(fw->shell);
 else closeFileProc(fw);
}

void closeDialog(AppWindowRec * aw)
{
 if (!(aw->modified))  closeProc((XtPointer) aw, NO);
 else if (resources.auto_save)  closeProc((XtPointer) aw, YES);
 else askQuestion(aw->shell, "Do you want to save Application changes?", True, False, True, Question, None, closeProc, aw);
}

typedef struct
{
    Widget textwidget;
    String *value;
} TextWidgetRec;

typedef struct
{
    void (*callback)(XtPointer, int);
    XtPointer callback_data;
    Widget dialog;
    int nitems;
    TextWidgetRec *line;
} TextWidgetList;

void textFieldDialog(Widget shell, TextFieldList fields, void (*callback)(XtPointer, int), XtPointer callback_data, Pixmap icon)
{
 TextWidgetList *list = (TextWidgetList *) XtMalloc(sizeof(TextWidgetList));
 Widget rowcol, form, label;
 Arg args[2];
 XmString name;
 int i = 0, l, longest=0;

 list->callback = callback;
 list->callback_data = callback_data;
 for (list->nitems = 0; (fields[list->nitems].label); list->nitems++);
 list->line = (TextWidgetRec *) XtMalloc(list->nitems * sizeof(TextWidgetRec));
 XtSetArg(args[i], XmNforeground, resources.label_color);  i++;
 XtSetArg(args[i], XmNautoUnmanage, False);  i++;
 list->dialog = XmCreatePromptDialog(shell, "Edit dialog", args, i);
 XtUnmanageChild(XmSelectionBoxGetChild(list->dialog, XmDIALOG_HELP_BUTTON));
 XtUnmanageChild(XmSelectionBoxGetChild(list->dialog, XmDIALOG_SELECTION_LABEL));
 XtUnmanageChild(XmSelectionBoxGetChild(list->dialog, XmDIALOG_TEXT));
 XtVaSetValues(XtParent(list->dialog), XmNdeleteResponse, XmDO_NOTHING, NULL);

 rowcol = XtVaCreateManagedWidget("Editform", xmRowColumnWidgetClass, list->dialog, XmNorientation, XmVERTICAL, NULL);

 if (icon != None)  XtVaCreateManagedWidget("Icon", xmLabelGadgetClass, rowcol, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icon, NULL);

 for (i=0; i<list->nitems; i++)
 {
     name = XmStringCreateLocalized(fields[i].label);
     if ((l = XmStringWidth((XmFontList) resources.bold_font, name)) > longest)  longest = l;
     XmStringFree(name);
  }
 longest += 3;

 for (i=0; i<list->nitems; i++)
 {
     form = XtVaCreateManagedWidget("Line", xmFormWidgetClass, rowcol, XmNforeground, resources.label_color, NULL);
     label = XtVaCreateManagedWidget(fields[i].label, xmLabelGadgetClass, form, XmNfontList, resources.bold_font, XmNwidth, longest, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNalignment, XmALIGNMENT_END, NULL);
     list->line[i].textwidget = XtVaCreateManagedWidget ("Input", xmTextFieldWidgetClass, form, XmNfontList, resources.tty_font, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNrightAttachment, XmATTACH_FORM, NULL);
     list->line[i].value = fields[i].value;
     if (*fields[i].value)
	 XmTextFieldSetString(list->line[i].textwidget, *fields[i].value);
 }

 XtAddCallback(list->dialog, XmNokCallback, readTextProc, (XtPointer) list);
 XtAddCallback(list->dialog, XmNcancelCallback, readTextProc, (XtPointer) list);

 XtManageChild(list->dialog);
 XtPopup(XtParent(list->dialog), XtGrabNone);
}

void readTextProc(Widget dialog, XtPointer client_data, XtPointer call_data)
{
 XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
 TextWidgetList *list = (TextWidgetList *) client_data;
 String callback_name;
 int i;

 if (cbs->reason == XmCR_OK)
 {
     for (i=0; i<list->nitems; i++)
     {
	 if (*list->line[i].value)  XTFREE(*list->line[i].value);
	 *list->line[i].value = XmTextFieldGetString (list->line[i].textwidget);
     }
     list->callback(list->callback_data, YES);
     callback_name = XmNokCallback;
 }
 else
 {
     list->callback(list->callback_data, CANCEL);
     callback_name = XmNcancelCallback;
 }
 XtRemoveCallback(dialog, callback_name, readTextProc, client_data);
 XtDestroyWidget(XtParent(list->dialog));
 XTFREE(list->line); XTFREE(list);
}

/*---------------------------------------------------------------------------*/

typedef struct
{
    FileWindowRec *fw;
    Widget add_button, rem_button, pattern_field;
} SelectDlgRec;

void selectionDialog(FileWindowRec *fw)
{
 SelectDlgRec *data = (SelectDlgRec *) XtMalloc(sizeof(SelectDlgRec));
 Widget dialog, icon, form, label, text;
 XmString replace, quest;
 Arg args[3];
 int i = 0;

 data->fw = fw;
 replace = XmStringCreateLocalized("Replace");
 quest = XmStringCreateLocalized("Enter pattern for files to be selected");
 XtSetArg(args[i], XmNautoUnmanage, True); i++;
 XtSetArg(args[i], XmNokLabelString, replace); i++;
 XtSetArg(args[i], XmNforeground, resources.label_color); i++;
 dialog = XmCreatePromptDialog(fw->shell, "Selection", args, i);
 XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
 XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_SELECTION_LABEL));
 XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT));
 XtVaSetValues(XtParent(dialog), XmNdeleteResponse, XmDO_NOTHING, NULL);
 XtAddCallback(dialog, XmNokCallback, (XtPointer) sel_callback, (XtPointer) data);
 XtAddCallback(dialog, XmNcancelCallback, (XtPointer) sel_callback, (XtPointer) data);

 form = XtVaCreateManagedWidget("Line", xmFormWidgetClass, dialog, XmNforeground, resources.label_color, NULL);
 icon = XtVaCreateManagedWidget("Icon", xmLabelGadgetClass, form, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[FILES_BM].bm, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
 text = XtVaCreateManagedWidget("Text", xmLabelGadgetClass, form, XmNlabelString, quest, XmNfontList, resources.bold_font, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 20, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, icon, XmNrightAttachment, XmATTACH_FORM, NULL);
 label = XtVaCreateManagedWidget("Filename pattern:", xmLabelGadgetClass, form, XmNfontList, resources.bold_font, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, icon, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
 data->pattern_field = XtVaCreateManagedWidget ("Input", xmTextFieldWidgetClass, form, XmNfontList, resources.tty_font, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, icon, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNrightAttachment, XmATTACH_FORM, NULL);

 data->add_button = XtVaCreateManagedWidget("Add", xmPushButtonGadgetClass, dialog, NULL);
 XtAddCallback(data->add_button, XmNactivateCallback, (XtPointer) sel_callback, (XtPointer) data);
 data->rem_button = XtVaCreateManagedWidget("Remove", xmPushButtonGadgetClass, dialog, NULL);
 XtAddCallback(data->rem_button, XmNactivateCallback, (XtPointer) sel_callback, (XtPointer) data);

 XmStringFree(quest);
 XmStringFree(replace);
 XtManageChild(dialog);
 XtPopup(XtParent(dialog), XtGrabNone);
}

void sel_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
 SelectDlgRec *data = (SelectDlgRec *) client_data;
 String pattern;

 if (cbs->reason != XmCR_CANCEL)
 {
     pattern = XmTextFieldGetString(data->pattern_field);
     if (cbs->reason == XmCR_OK)
	 selectAdd(data->fw, pattern, True);
     else if (w == data->add_button)
	 selectAdd(data->fw, pattern, False);
     else if (w == data->rem_button)
	 selectRemove(data->fw, pattern);
 }
 XTFREE(data);
}

/*---------------------------------------------------------------------------*/

typedef struct
{
    FileWindowRec *fw;
    Widget dialog, pattern_field;
} FilterDlgRec;

void filterDialog(FileWindowRec *fw)
{
 FilterDlgRec *data = (FilterDlgRec *) XtMalloc(sizeof(FilterDlgRec));
 Widget form, icon, text, label, clear_button;
 XmString quest;
 Arg args[2];
 int i = 0;

 data->fw = fw;
 quest = XmStringCreateLocalized("Enter pattern for files to be shown");
 XtSetArg(args[i], XmNautoUnmanage, False);  i++;
 XtSetArg(args[i], XmNforeground, resources.label_color);  i++;
 data->dialog = XmCreatePromptDialog(fw->shell, "Filter", args, i);

 XtUnmanageChild(XmSelectionBoxGetChild(data->dialog, XmDIALOG_HELP_BUTTON));
 XtUnmanageChild(XmSelectionBoxGetChild(data->dialog, XmDIALOG_SELECTION_LABEL));
 XtUnmanageChild(XmSelectionBoxGetChild(data->dialog, XmDIALOG_TEXT));
 XtVaSetValues(XtParent(data->dialog), XmNdeleteResponse, XmDO_NOTHING, NULL);
 XtAddCallback(data->dialog, XmNokCallback, (XtPointer) fil_callback, (XtPointer) data);
 XtAddCallback(data->dialog, XmNhelpCallback, (XtPointer) fil_callback, (XtPointer) data);
 XtAddCallback(data->dialog, XmNcancelCallback, (XtPointer) fil_callback, (XtPointer) data);

 form = XtVaCreateManagedWidget("Line", xmFormWidgetClass, data->dialog, XmNforeground, resources.label_color, NULL);
 icon = XtVaCreateManagedWidget("Icon", xmLabelGadgetClass, form, XmNlabelType, XmPIXMAP, XmNlabelPixmap, icons[FILES_BM].bm, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
 text = XtVaCreateManagedWidget("Text", xmLabelGadgetClass, form, XmNlabelString, quest, XmNfontList, resources.bold_font, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 20, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, icon, XmNrightAttachment, XmATTACH_FORM, NULL);
 label = XtVaCreateManagedWidget("Filter:", xmLabelGadgetClass, form, XmNfontList, resources.bold_font, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, icon, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
 data->pattern_field = XtVaCreateManagedWidget ("Input", xmTextFieldWidgetClass, form, XmNfontList, resources.tty_font, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, icon, XmNbottomAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNrightAttachment, XmATTACH_FORM, NULL);
 if (fw->do_filter)
     XmTextFieldSetString(data->pattern_field, fw->dirFilter);

 clear_button = XtVaCreateManagedWidget("Clear", xmPushButtonGadgetClass, data->dialog, NULL);
 XtAddCallback(clear_button, XmNactivateCallback, (XtPointer) fil_callback, (XtPointer) data);

 XmStringFree(quest);
/* XmStringFree(clear);*/
 XtManageChild(data->dialog);
 XtPopup(XtParent(data->dialog), XtGrabNone);
}

void fil_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
 FilterDlgRec *data = (FilterDlgRec *) client_data;
 String callback_name;

 if (cbs->reason == XmCR_ACTIVATE)
     XmTextFieldSetString( data->pattern_field, "");
 else
 {
     if (cbs->reason == XmCR_OK)
     {
	 filterFiles(data->fw, XmTextFieldGetString(data->pattern_field));
	 callback_name = XmNokCallback;
     }
     else
	 callback_name = XmNcancelCallback;
     XtRemoveCallback(w, callback_name, fil_callback, client_data);
     XtDestroyWidget(XtParent(data->dialog));
     XTFREE(data);
 }
}

/*---------------------------------------------------------------------------*/

void createCopyDialog(void)
{
 Widget form, label;
 Arg args[5];
 XmString message, hide, number, from, to;
 Dimension width;
 int i=0;

 message = XmStringCreateLocalized("Copying files...");
 hide = XmStringCreateLocalized("Hide");
 XtSetArg(args[i], XmNmessageString, message); i++;
 XtSetArg(args[i], XmNokLabelString, hide); i++;
 XtSetArg(args[i], XmNautoUnmanage, False); i++;
 XtSetArg(args[i], XmNresizePolicy, XmRESIZE_NONE); i++;
 XtSetArg(args[i], XmNforeground, resources.label_color); i++;
 copyInfo.dialog = XmCreateWorkingDialog(topShell, "copyinfo", args, i);
 XtUnmanageChild(XmMessageBoxGetChild(copyInfo.dialog, XmDIALOG_HELP_BUTTON));
 XmStringFree(message); XmStringFree(hide);
 XtAddCallback(copyInfo.dialog, XmNokCallback, copyInfoProc, NULL);
 XtAddCallback(copyInfo.dialog, XmNcancelCallback, copyInfoProc, NULL);

 number = XmStringCreateLocalized("# Files:");
 from = XmStringCreateLocalized("From:");
 to = XmStringCreateLocalized("To:");
 width = XmStringWidth(resources.bold_font, number)+3;
 form = XtVaCreateManagedWidget("form", xmFormWidgetClass, copyInfo.dialog, XmNforeground, resources.label_color, NULL);
 label = XtVaCreateManagedWidget("number_label", xmLabelGadgetClass, form, XmNlabelString, number, XmNwidth, width, XmNfontList, resources.bold_font, XmNalignment, XmALIGNMENT_END, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, NULL);
 copyInfo.number = XtVaCreateManagedWidget("number", xmLabelGadgetClass, form, XmNfontList, resources.label_font, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, NULL);
 label = XtVaCreateManagedWidget("from_label", xmLabelGadgetClass, form, XmNlabelString, from, XmNwidth, width, XmNfontList, resources.bold_font, XmNalignment, XmALIGNMENT_END, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, label, NULL);
 copyInfo.from = XtVaCreateManagedWidget("from", xmLabelGadgetClass, form, XmNfontList, resources.label_font, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, copyInfo.number, XmNrightAttachment, XmATTACH_FORM, NULL);
 label = XtVaCreateManagedWidget("to_label", xmLabelGadgetClass, form, XmNlabelString, to, XmNwidth, width, XmNfontList, resources.bold_font, XmNalignment, XmALIGNMENT_END, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, label, NULL);
 copyInfo.to = XtVaCreateManagedWidget("from", xmLabelGadgetClass, form, XmNfontList, resources.label_font, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, copyInfo.from, XmNrightAttachment, XmATTACH_FORM, NULL);
 XmStringFree(number); XmStringFree(from); XmStringFree(to);
 updateCopyDialog();
 XtManageChild(copyInfo.dialog);
 XtPopup(XtParent(copyInfo.dialog), XtGrabNone);
}

/*---------------------------------------------------------------------------*/

void copyInfoProc(Widget w, XtPointer client_data, XtPointer call_data)
{
 XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *) call_data;
 CopyRec *file;
 int i;

 if (cbs->reason == XmCR_CANCEL)
 {
     XtRemoveWorkProc(cpy.wpID);
     if (cpy.src != -1)  close(cpy.src);
     if (cpy.dest != -1)
     {
	 close(cpy.dest);
	 unlink(cpy.copies[cpy.first].to);
     }
     cpy.src = cpy.dest = -1;
     for (i = cpy.first; i <= cpy.last; i++)
     {
	 XTFREE((file = &cpy.copies[i])->from);
	 XTFREE(file->to);
     }
     cpy.first = 0;
     cpy.last = -1;
     for (i=0; i < mntable.n_dev; i++)
	 umountDev(i, True);
 }
 XtDestroyWidget(w);
 copyInfo.dialog = NULL;
 intUpdate(CHECK_FILES);
}

/*---------------------------------------------------------------------------*/

void updateCopyDialog(void)
{
 XmString number, from, to;
 char nr[10];

 sprintf(nr, "%d", cpy.last - cpy.first + 1);
 number = XmStringCreateLocalized(nr);
 from = XmStringCreateLocalized(cpy.copies[cpy.first].from);
 to = XmStringCreateLocalized(cpy.copies[cpy.first].to);
 XtVaSetValues(copyInfo.number, XmNlabelString, number, NULL);
 XtVaSetValues(copyInfo.from, XmNlabelString, from, NULL);
 XtVaSetValues(copyInfo.to, XmNlabelString, to, NULL);
 XmStringFree(number); XmStringFree(from); XmStringFree(to);
}

/*---------------------------------------------------------------------------*/

void error(Widget shell, String line1, String line2)
{
 Widget dialog;
 Arg args[3];
 char *message;
 XmString xmmess;
 size_t len2;
 int i = 0;

 if (line2)  len2 = strlen(line2);
 else  len2 = 0;
 message = (char *) XtMalloc(strlen(line1) + len2 + 2);
 strcpy(message, line1);
 if (line2)
 {
     strcat(message, "\n");
     strcat(message, line2);
 }
 xmmess = XmStringCreateLtoR(message, XmFONTLIST_DEFAULT_TAG);
 XTFREE(message);
 XtSetArg(args[i], XmNautoUnmanage, True); i++;
 XtSetArg(args[i], XmNmessageString, xmmess); i++;
 XtSetArg(args[i], XmNforeground, resources.label_color); i++;
 dialog = XmCreateErrorDialog(shell, "Error", args, i);
 XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
 XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
 XtManageChild(dialog);
 XtPopup(XtParent(dialog), XtGrabNone);
 XmStringFree(xmmess);
}

/*---------------------------------------------------------------------------*/

void sysError(Widget shell, String line)
{
 error(shell, line, sys_errlist[errno]);
}
