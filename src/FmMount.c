/*-----------------------------------------------------------------------------
  Module FmMount.c                                                             

  (c) Simon Marlow 1991
  (c) Albert Graef 1994
  (c) Oliver Mai 1995, 1996
                                                                           
  functions & data for mounting devices                                                   
-----------------------------------------------------------------------------*/

#ifdef SVR4
#ifndef IRIX
#define SVR4_MNT
#endif	/* IRIX */
#endif	/* SVR4 */

#ifndef NO_GETMNTENT

#ifdef SVR4_MNT
#include <stdio.h>
#include <sys/mnttab.h>
#else
#include <mntent.h>
#endif	/* SVR4_MNT */

#endif	/* NO_GETMNTENT */

#include <sys/wait.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/Protocols.h>
#include <Xm/AtomMgr.h>

#include "Fm.h"

#define MAXCFGLINELEN 1024

static String findMountPoint(String special, int *errflag);
static void toggle_mount(Widget button, XtPointer client_data, XtPointer call_data);
static void rescan_mtab(Widget button, XtPointer client_data, XtPointer call_data);
static void closeMountTbl(Widget w, XtPointer client_data, XtPointer call_data);
static void mountIconifyHandler(Widget shell, XtPointer client_data, XEvent *event);

/*-----------------------------------------------------------------------------
  PUBLIC DATA                                       
-----------------------------------------------------------------------------*/

MountRec mntable;

/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static int parseDev(FILE *fp, char **label, char **special, char **mpoint, char **mount_action, char **umount_action, char **slow)
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
  if (!(*label = split(s, ':')))
    return -1;
  if (!(*special = split(NULL, ':')))
    return -1;
  if (!(*mpoint = split(NULL, ':')))
    return -1;
  if (!(*mount_action = split(NULL, ':')))
    return -1;
  if (!(*umount_action = split(NULL, ':')))
    return -1;
  if (!(*slow = split(NULL, ':')))
    return -1;
  return l;
}

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

int devAction(char *action)
{
  int pid, status, res;

  chdir(user.home);
  if ((pid = fork()) == -1)
  {
      sysError(topShell, "Can't fork:");
      return 0;
  } else if (!pid)
  {
      if (resources.echo_actions)
	  fprintf(stderr, "%s\n", action);
      freopen("/dev/null", "r", stdin);
      if (user.arg0flag)
	  execlp(user.shell, user.shell, "-c", action, user.shell, NULL);
      else  execlp(user.shell, user.shell, "-c", action, NULL);
      perror("Exec failed");
      exit(1);
  } else
  {
    res = waitpid(pid, &status, 0);
    if (res == -1 || !WIFEXITED(status) || WEXITSTATUS(status))
    	return 0;
    else
   	return 1;
  }
}

/*---------------------------------------------------------------------------*/

void readDevices(String path)
{
  DeviceRec *dev;
  FILE *fp;
  char *label, *special, *mpoint, *mount_action, *umount_action, *slow;
  char s[MAXCFGSTRINGLEN];
  char *msg, *err = "Error reading ";
  int i, p;
  
  mntable.n_dev = 0;
  mntable.devices = NULL;
  mntable.dialog = None;
  mntable.iconic = False;
  mntable.show = False;
  mntable.slow_dev = False;
  
  if (!(fp = fopen(path, "r"))) return;

  for (i=0; (p = parseDev(fp, &label, &special, &mpoint, &mount_action, &umount_action, &slow)) > 0; i++)
  {
    mntable.devices = (DeviceRec *) XTREALLOC(mntable.devices, (i+1) * sizeof(DeviceRec) );
    dev = &mntable.devices[i];  
    dev->label = XtNewString(strparse(s, label, "\\:"));
    dev->special = XtNewString(strparse(s, special, "\\:"));
    dev->def_mpoint = XtNewString(strparse(s, mpoint, "\\:"));
    dev->mount_act = XtNewString(strparse(s, mount_action, "\\:"));
    dev->umount_act = XtNewString(strparse(s, umount_action, "\\:"));
    if (!strcmp(strparse(s, slow, "\\:"), "SLOW"))
	dev->slow_dev = True;
    dev->mpoint = NULL;
    dev->cp_req = False;
    dev->n_req = 0;
    dev->manmounted = False;
  }

  if (p == -1)
    error(topShell, "Error in devices file", NULL);

  mntable.n_dev = i;
  
  if (fclose(fp))
    sysError(topShell, "Error reading devices file:");

  if (checkDevices() == -1)
  {
      msg = (char *) XtMalloc((strlen(err) + strlen(MTAB) +1) * sizeof(char));
      strcpy(msg, err);
      strcat(msg, MTAB);
      error(topShell, msg, "Mounting features disabled!");
      XTFREE(msg);
      for (i=0; i<mntable.n_dev; i++)
      {
	  XTFREE(mntable.devices[i].label);
	  XTFREE(mntable.devices[i].special);
	  XTFREE(mntable.devices[i].def_mpoint);
	  if (mntable.devices[i].mpoint)
	      XTFREE(mntable.devices[i].mpoint);
	  XTFREE(mntable.devices[i].mount_act);
	  XTFREE(mntable.devices[i].umount_act);
      }
      XTFREE(mntable.devices);
      mntable.n_dev = 0;
  }
}

/*---------------------------------------------------------------------------*/

String findMountPoint(String special, int *errflag)
{
 String mpoint = NULL;

#ifndef NO_GETMNTENT

#ifdef SVR4_MNT

 struct mnttab mp;
 struct mnttab mpref = { NULL, NULL, NULL, NULL, NULL };
 FILE *mtab;

 if (!(mtab = fopen(MTAB, "r")))
 {
     *errflag = -1;
     return NULL;
 }
 mpref.mnt_special = special;
 if (!(getmntany(mtab, &mp, &mpref)))
     mpoint = XtNewString(mp.mnt_mountp);
 fclose(mtab);

#else	/* SVR4_MNT undefined */

 struct mntent *entry;
 FILE *mtab;

 if (!(mtab = setmntent(MTAB, "r")))
 {
     *errflag = -1;
     return NULL;
 }
 while ((entry = getmntent(mtab)))
 {
     if (!(strcmp(entry->mnt_fsname, special)))
     {
	 mpoint = XtNewString(entry->mnt_dir);
	 break;
     }
 }
 endmntent(mtab);

#endif	/* SVR4_MNT */
#endif	/* NO_GETMNTENT */

 return mpoint;
}

/*---------------------------------------------------------------------------*/

int checkDevices(void)
{
 DeviceRec *dev;
 Boolean mounted, sensitive;
 String mpoint;
 int i, changed = 0, errflag = 0;

#ifdef NO_GETMNTENT
 if (!resources.suppress_warnings)
     fprintf(stderr, "moxfm has been compiled with -DNO_GETMNTENT.\nThe mount table window might not work as you expect.\n");
#endif

 for (i=0; i<mntable.n_dev; i++)
 {
     dev = &mntable.devices[i];
     mpoint = findMountPoint(dev->special, &errflag);
     if (errflag)
	 return errflag;
     if (mpoint)  mounted = True;
     else  mounted = False;
     if (mounted && strcmp(mpoint, dev->def_mpoint))
     {
	 String msg = "Device %s (%s) should be mounted on";
	 String err = (String) XtMalloc((strlen(msg) + strlen(dev->label) + strlen(dev->special)) * sizeof(char));
	 sprintf(err, msg, dev->special, dev->label);
	 error(getAnyShell(), err, dev->def_mpoint);
	 XTFREE(err);
	 if (!dev->mpoint || strcmp(dev->mpoint, mpoint))
	     changed = 1;
     }
     else if (mpoint)
     {
	 XTFREE(mpoint);
	 mpoint = NULL;
     }
     if (dev->mpoint)
	 XTFREE(dev->mpoint);
     dev->mpoint = mpoint;
     if (mounted != dev->mounted)
     {
	 changed = 1;
	 dev->mounted = mounted;
     }
     if (!mounted && dev->n_req)
	 changed = 2;
     if (!mounted || (!dev->n_req && !dev->cp_req))
	 sensitive = True;
     else  sensitive = False;
     if (!changed && sensitive != dev->sensitive)
	 changed = 1;
     if (!dev->mounted)
	 dev->manmounted = False;
     dev->sensitive = sensitive;
 }
 return changed;
}

/* find the device for a directory */
int findDev(String path)
{
 int d;

 for (d = 0; d < mntable.n_dev; d++)
     if (prefix(mntable.devices[d].def_mpoint, path))
	 return d;
 return -1;
}

/*---------------------------------------------------------------------------*/

/* mount a device */
int mountDev(int d, Boolean cpyflag)
{
 DeviceRec *dev = &mntable.devices[d];
 int res = 0;

 if (d == -1)
     ;
 else if (dev->mounted && !dev->mpoint)
 {
     if (cpyflag)
	 dev->cp_req = True;
     else
	 dev->n_req++;
     if (dev->sensitive)
     {
	 dev->sensitive = False;
	 if (mntable.dialog != None)
	     updateMountDisplay(dev);
     }
 }
 else if (dev->mpoint)
     res = -1;
 else if (devAction(dev->mount_act))
 {
     if (cpyflag)
	 dev->cp_req = True;
     else
	 dev->n_req = 1;
     dev->mounted = True;
     dev->sensitive = False;
     if (mntable.dialog != None)
	 updateMountDisplay(dev);
 }
 else  res = -1;
 return res;
}

/*---------------------------------------------------------------------------*/

/* unmount a device */
void umountDev(int d, Boolean cpyflag)
{
 Boolean unmount = False;
 UnmountProcRec *data;
 DeviceRec *dev = &mntable.devices[d];

 if (d == -1 || !dev->mounted)
     ;
 else if (cpyflag)
 {
     if (dev->cp_req && !mntable.devices[d].n_req)
	 unmount = True;
     dev->cp_req = False;
 }
 else if (dev->n_req > 1)
     dev->n_req--;
 else
 {
     dev->n_req = 0;
     if (!dev->cp_req)
	 unmount = True;
 }

 if (unmount)
 {
     dev->sensitive = True;
     if (!dev->manmounted)
     {
	 if (devAction(dev->umount_act))
	 {
	     dev->mounted = False;
	     dev->manmounted = False;
	     if (dev->mpoint)
		 XTFREE(dev->mpoint);
	     dev->mpoint = NULL;
	 }
	 else
	 {
	     data = (UnmountProcRec *) XtMalloc(sizeof(UnmountProcRec));
	     data->device = d;
	     data->trials = 0;
	     XtAppAddTimeOut(app_context, 5000, unmountProc, (XtPointer) data);
	 }
     }
     if (mntable.dialog != None)
	     updateMountDisplay(dev);
 }
}

/*---------------------------------------------------------------------------*/

void unmountProc(XtPointer client_data, XtIntervalId *id)
{
 UnmountProcRec *data = (UnmountProcRec *) client_data;
 DeviceRec *dev = &mntable.devices[data->device];
 Boolean remove = False;

 rescan_mtab(None, NULL, NULL);
 if (!dev->mounted || dev->n_req > 0 || dev->cp_req)
     remove = True;
 else if (devAction(dev->umount_act))
 {
     dev->mounted = False;
     dev->manmounted = False;
     dev->sensitive = True;
     if (dev->mpoint)
	 XTFREE(dev->mpoint);
     dev->mpoint = NULL;
     remove = True;
     if (mntable.dialog != None)
	 updateMountDisplay(dev);
 }

 if (remove)  XTFREE(data);
 else if (++(data->trials) == 1 || !(data->trials % 5))
     unmountDialog(data);
 else
     XtAppAddTimeOut(app_context, 5000, unmountProc, (XtPointer) data);
}

/*---------------------------------------------------------------------------*/

void unmountDlgProc(XtPointer client_data, int answer)
{
 UnmountProcRec *data = (UnmountProcRec *) client_data;

 if (answer == YES)
     XtAppAddTimeOut(app_context, 5000, unmountProc, (XtPointer) data);
 else  XTFREE(data);
}

/*---------------------------------------------------------------------------*/

void unmountAll(void)
{
 DeviceRec *dev;
 int i;

  for (i = 0; i < mntable.n_dev; i++)
  {
      dev = &mntable.devices[i];
      if (dev->mounted && !devAction(dev->umount_act))
	  fprintf(stderr, "moxfm: Could not unmount %s\n", dev->special);
  }
}

/*---------------------------------------------------------------------------*/

void displayMountTable(void)
{
 Widget form, line = None, label, update, close;
 XmString str, mstr, umstr, unmstr;
 Dimension labelwidth = 0, statuswidth, buttonwidth, width;
 DeviceRec *dev;
 Arg args[8];
 int d, j;

 mntable.dialog = XtVaAppCreateShell("mount table", "Moxfm", topLevelShellWidgetClass, dpy, XmNiconPixmap, icons[DISK_BM].bm, XmNiconMask, icons[DISK_BM].mask, XmNtitle, "Mount table", XmNdeleteResponse, XmDO_NOTHING, XmNiconic, mntable.iconic, NULL);
 form = XtVaCreateManagedWidget("form", xmFormWidgetClass, mntable.dialog, XmNforeground, resources.label_color, NULL);
 mstr = XmStringCreateLocalized("mount");
 umstr = XmStringCreateLocalized("unmount");
 unmstr = XmStringCreateLocalized("unmounted");
 statuswidth = buttonwidth = XmStringWidth(resources.menu_font, unmstr);
 for (d=0; d < mntable.n_dev; d++)
 {
     str = XmStringCreateLocalized(mntable.devices[d].label);
     width = XmStringWidth(resources.bold_font, str);
     if (width > labelwidth)  labelwidth = width;
     XmStringFree(str);
     str = XmStringCreateLocalized(mntable.devices[d].def_mpoint);
     width = XmStringWidth(resources.menu_font, str);
     if (width > statuswidth)  statuswidth = width;
     XmStringFree(str);
 }
 for (d=0; d < mntable.n_dev; d++)
 {
     j = 0;
     dev = &mntable.devices[d];
     XtSetArg(args[j], XmNleftAttachment, XmATTACH_FORM); j++;
     if (line)
     {
	 XtSetArg(args[j], XmNtopAttachment, XmATTACH_WIDGET); j++;
	 XtSetArg(args[j], XmNtopWidget, line); j++;
     }
     else
     {
	 XtSetArg(args[j], XmNtopAttachment, XmATTACH_FORM); j++;
     }
     XtSetArg(args[j], XmNrightAttachment, XmATTACH_FORM); j++;
     XtSetArg(args[j], XmNleftOffset, 5); j++;
     XtSetArg(args[j], XmNtopOffset, 3); j++;
     XtSetArg(args[j], XmNrightOffset, 5); j++;
     XtSetArg(args[j], XmNforeground, resources.label_color); j++;
     line = XtCreateManagedWidget("line", xmFormWidgetClass, form, args, j);
     str = XmStringCreateLocalized(dev->label);
     label = XtVaCreateManagedWidget("device label", xmLabelGadgetClass, line, XmNlabelString, str, XmNwidth, labelwidth, XmNfontList, resources.bold_font, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, NULL);
     XmStringFree(str);
     dev->button = XtVaCreateManagedWidget("mount button", xmPushButtonGadgetClass, line, XmNlabelString, dev->mounted ? umstr : mstr, XmNfontList, resources.menu_font, XmNsensitive, dev->sensitive, XmNleftAttachment, XmATTACH_OPPOSITE_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNleftOffset, -buttonwidth-5, NULL);
     if (dev->mounted)
     {
	 if (dev->mpoint)
	     str = XmStringCreateLocalized(dev->mpoint);
	 else
	     str = XmStringCreateLocalized(dev->def_mpoint);
     }
     dev->state = XtVaCreateManagedWidget("device state", xmLabelGadgetClass, line, XmNlabelString, dev->mounted ? str : unmstr, XmNfontList, resources.menu_font, XmNwidth, statuswidth + 1, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, label, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, dev->button, XmNbottomAttachment, XmATTACH_FORM, XmNleftOffset, 10, XmNrightOffset, 10, NULL);
     if (dev->mounted)  XmStringFree(str);
     XtAddCallback(dev->button, XmNactivateCallback, toggle_mount, dev);
 }
 XmStringFree(mstr);
 XmStringFree(umstr);
 XmStringFree(unmstr);

 str = XmStringCreateLocalized("Update display");
 update = XtVaCreateManagedWidget("update button", xmPushButtonGadgetClass, form, XmNlabelString, str, XmNfontList, resources.menu_font, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, line, XmNbottomAttachment, XmATTACH_FORM, XmNleftOffset,5, XmNtopOffset, 7, XmNbottomOffset, 3,
#ifdef NO_GETMNTENT
XmNsensitive, False,
#endif
				  NULL);

 XmStringFree(str);
 XtAddCallback(update, XmNactivateCallback, rescan_mtab, NULL);
 str = XmStringCreateLocalized("Close");
 close = XtVaCreateManagedWidget("close button", xmPushButtonGadgetClass, form, XmNlabelString, str, XmNfontList, resources.menu_font, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, line, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNleftOffset,5, XmNtopOffset, 7, XmNbottomOffset, 3, XmNrightOffset, 5, NULL);
 XmStringFree(str);
 XtAddCallback(close, XmNactivateCallback, closeMountTbl, NULL);


 XtAddEventHandler(mntable.dialog, StructureNotifyMask, True, (XtEventHandler) mountIconifyHandler, NULL);
 XmAddWMProtocolCallback(mntable.dialog, wm_delete_window, closeMountTbl, NULL);
 XtRealizeWidget(mntable.dialog);

 if (mntable.show)
     XtVaSetValues(mntable.dialog, XmNx, mntable.x, XmNy, mntable.y, NULL);
}

/*---------------------------------------------------------------------------*/

void updateMountDisplay(DeviceRec *dev)
{
 XmString statestr, butstr;

 if (dev->mounted)
 {
     if (dev->mpoint)
	 statestr = XmStringCreateLocalized(dev->mpoint);
     else
	 statestr = XmStringCreateLocalized(dev->def_mpoint);
     butstr = XmStringCreateLocalized("unmount");
 }
 else
 {
     statestr = XmStringCreateLocalized("unmounted");
     butstr = XmStringCreateLocalized("mount");
 }
 XtVaSetValues(dev->state, XmNlabelString, statestr, NULL);
 XmStringFree(statestr);
 XtVaSetValues(dev->button, XmNlabelString, butstr, XmNsensitive, dev->sensitive, NULL);
 XmStringFree(butstr);
}

/*---------------------------------------------------------------------------*/

void toggle_mount(Widget button, XtPointer client_data, XtPointer call_data)
{
 DeviceRec *dev = (DeviceRec *) client_data;

 if (dev->mounted)
 {
     if (devAction(dev->umount_act))
     {
	 dev->mounted = False;
	 dev->manmounted = False;
	 dev->sensitive = True;
	 if (dev->mpoint)
	     XTFREE(dev->mpoint);
	 dev->mpoint = NULL;
	 updateMountDisplay(dev);
     }
     else
     {
	 rescan_mtab(None, NULL, NULL);
	 if (dev->mounted)
	     error(mntable.dialog, "Cannot unmount", dev->special);
     }
 }
 else
 {
     if (devAction(dev->mount_act))
     {
	 dev->mounted = True;
	 dev->manmounted = True;
	 dev->sensitive = True;
	 updateMountDisplay(dev);
     }
     else
     {
	 rescan_mtab(None, NULL, NULL);
	 if (!dev->mounted)
	     error(mntable.dialog, "Cannot mount device on", dev->def_mpoint);
     }
 }
}

/*---------------------------------------------------------------------------*/

void rescan_mtab(Widget button, XtPointer client_data, XtPointer call_data)
{
#ifndef NO_GETMNTENT

 FileWindowRec *fw;
 DeviceRec *dev;
 int d, res;

 res = checkDevices();
 if (res)
 {
     for (d = 0; d < mntable.n_dev; d++)
     {
	 dev = &mntable.devices[d];
	 updateMountDisplay(dev);
	 if (res == 2 && !dev->mounted)
	 {
	     for (fw = file_windows; fw; fw = fw->next)
	     {
		 if (fw->dev == d)
		 {
		     if (lastWindow())
			 chFileDir(fw, user.home);
		     else
			 closeFileProc(fw);
		     error(getAnyShell(), "A file window had to be closed as there is no device mounted on", dev->def_mpoint);
		 }
	     }
	 }
     }
 }

#endif	/* NO_GETMNTENT */
}

/*---------------------------------------------------------------------------*/

void closeMountTbl(Widget w, XtPointer client_data, XtPointer call_data)
{
 if (mntable.dialog != None)
     XtDestroyWidget(mntable.dialog);
 mntable.dialog = None;
}

/*---------------------------------------------------------------------------*/

void saveMountWindow(FILE *fout)
{
 Dimension x,y;

 if (mntable.dialog)
 {
     XtVaGetValues(mntable.dialog, XmNx, &x, XmNy, &y, NULL);
     fwrite(&x, sizeof(Dimension), 1, fout);
     fwrite(&y, sizeof(Dimension), 1, fout);
     fwrite(&mntable.iconic, sizeof(Boolean), 1, fout);
 }
}

/*---------------------------------------------------------------------------*/

void mountIconifyHandler(Widget shell, XtPointer client_data, XEvent *event)
{
 if (event->type == MapNotify)
     mntable.iconic = False;
 else if (event->type == UnmapNotify)
     mntable.iconic = True;
}
