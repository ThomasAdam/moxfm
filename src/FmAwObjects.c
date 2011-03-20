/*-----------------------------------------------------------------------------
  Module FmAwObjects.c                                                             

  (c) Oliver Mai 1995, 1996
                                                                           
  functions user defined Application window
  objects                                                
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <Xm/Xm.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>

#include "Am.h"

#define LABELLEN 10

UserObjectList usrObjects;

extern char *tempnam(const char *dir, const char *pfx);
static String crtObjFileName(Widget shell, String atom_name);
static int parseObj(FILE *fp, char **name, char **push_action, char **label, char **icon, char **drag_icon);

/*---------------------------------------------------------------------------*/

String crtObjFileName(Widget shell, String atom_name)
{
 struct stat stats;
 time_t now;
 String name;

 if (stat(resources.obj_dir, &stats))
 {
     if (mkdir(resources.obj_dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
     {
	 error(shell, "Could not create directory:", resources.obj_dir);
	 return NULL;
     }
 }
 else if (!S_ISDIR(stats.st_mode))
 {
     error(shell, "Not a directory:", resources.obj_dir);
     return NULL;
 }
 now = time(NULL);
 name = (String) XtMalloc((strlen(atom_name) + 12) * sizeof(char));
 sprintf(name, "%s-%lu", atom_name, (unsigned long) now);
 chdir(resources.obj_dir);
 if (!(lstat(name, &stats)))
 {
     XTFREE(name);
     name = tempnam(resources.obj_dir, NULL);
 }
 return name;
}

/*-------------------------------------------------------------------------*/

int parseObj(FILE *fp, char **name, char **push_action, char **label, char **icon, char **drag_icon)
{
  static char s[MAXAPPSTRINGLEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXAPPSTRINGLEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*name = split(s, ':')))
    return -1;
  if (!(*push_action = split(NULL, ':')))
    return -1;
  if (!(*label = split(NULL, ':')))
    return -1;
  if (!(*icon = split(NULL, ':')))
    return -1;
  if (!(*drag_icon = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

void readUserObjects(void)
{
 FILE *fp;
 String atom_name, push_action, label, icon, drag_icon;
 char s[MAXAPPSTRINGLEN];
 int i, p;

 usrObjects.nr = 0;
 usrObjects.obj = NULL;

 if (!(fp = fopen(resources.usrobj_file, "r")))
 {
     if (exists(resources.usrobj_file))
	 error(topShell, "Cannot read object file", resources.usrobj_file);
     return;
 }

 for (i=0; (p = parseObj(fp, &atom_name, &push_action, &label, &icon, &drag_icon)) > 0; i++)
 {
     usrObjects.obj = (UserObjectRec *) XTREALLOC(usrObjects.obj, (i+1) * sizeof(UserObjectRec));
     usrObjects.obj[i].atom_name = XtNewString(strparse(s, atom_name, "\\:"));
     if ((usrObjects.obj[i].atom = XmInternAtom(dpy, usrObjects.obj[i].atom_name, False)) == None)
     {
	 XTFREE(usrObjects.obj[i].atom_name);
	 error(topShell, "Couldn't initialize user defined object", atom_name);
	 i--;
     }
     else
     {
	 usrObjects.obj[i].push_action = XtNewString(strparse(s, push_action, "\\:"));
	 usrObjects.obj[i].label = XtNewString(strparse(s, label, "\\:"));
	 usrObjects.obj[i].icon = XtNewString(strparse(s, icon, "\\:"));
	 usrObjects.obj[i].drag_icon = XtNewString(strparse(s, drag_icon, "\\:"));
     }
 }

 if (p == -1)
     error(topShell, "Error in objects file:", resources.usrobj_file);

 usrObjects.nr = i;

  
  if (fclose(fp))
      sysError(topShell, "Error reading objects file:");
}

/*---------------------------------------------------------------------------*/

int dupObject(Widget shell, AppRec *dest, AppRec *src)
{
 FILE *fin, *fout;
 char buffer[BUFSIZ];
 size_t nr;

 if (chdir(src->directory))
     return -1;
 if (!(fin = fopen(src->fname, "r")))
 {
     chdir(user.home);
     return -1;
 }
 chdir(user.home);
 if (!(dest->fname = crtObjFileName(shell, usrObjects.obj[src->objType].atom_name)))
 {
     fclose(fin);
     return -1;
 }
 if (!(fout = fopen(dest->fname, "w")))
 {
     XTFREE(dest->fname);
     fclose(fin);
     return -1;
 }
 do
 {
     nr = fread(buffer, 1, BUFSIZ, fin);
     if (nr != fwrite(buffer, 1, nr, fout))
     {
	 fclose(fin);
	 fclose(fout);
	 chdir(src->directory);
	 unlink(dest->fname);
	 chdir(user.home);
	 XTFREE(dest->fname);
	 return -1;
     }
 } while (nr);
 fclose(fin);
 fclose(fout);
 dest->name = XtNewString(src->name);
 dest->directory = XtNewString(src->directory);
 dest->icon = XtNewString(src->icon);
 dest->push_action = XtNewString(src->push_action);
 dest->drop_action = XtNewString(src->drop_action);
 dest->objType = src->objType;
 dest->remove_file = True;
 return 0;
}

/*---------------------------------------------------------------------------*/

int getObjectType(String push_action)
{
 int i, res = APPLICATION;

 for (i=0; i < usrObjects.nr; i++)
 {
     if (!(strcmp(push_action, usrObjects.obj[i].atom_name)))
     {
	 res = i;
	 break;
     }
 }

 return res;
}

/*---------------------------------------------------------------------------*/

String getPushAction(int objType)
{
 if (objType >= usrObjects.nr || objType < 0)
     return "";
 return usrObjects.obj[objType].push_action;
}

/*---------------------------------------------------------------------------*/

void makeObjectButton(AppSpecRec *awi, int objType, char *buffer, unsigned long len)
{
 AppList *applist;
 AppRec *app;
 UserObjectRec *obj = &usrObjects.obj[objType];
 FILE *fo;
 String filename;
 char label[LABELLEN + 3];
 int i, c;

 if (!len)  return;

 if (!(filename = crtObjFileName(awi->win.aw->shell, obj->atom_name)))
     return;

 if (!(fo = fopen(filename, "w")))
     error(awi->win.aw->shell, "Could not open file for writing:", filename);
 else
 {
     if (len != fwrite(buffer, 1, len, fo))
	 error(awi->win.aw->shell, "Error saving dropped data", NULL);
     else
     {
	 applist = (AppList *) XtMalloc(sizeof(AppList));
	 *applist = app = (AppRec *) XtMalloc(sizeof(AppRec));
	 if (strcmp(obj->label, "CONTENTS"))
	     sprintf(label, "<%s>", obj->label);
	 else if (len <= LABELLEN)
	     sprintf(label, "<%s>", buffer);
	 else
	 {
	     strcpy(label, "<");
	     strncpy(&label[1], buffer, LABELLEN - 2);
	     strcpy(&label[LABELLEN - 1], "..>");
	 }
	 i = 0;
	 while (( c = label[i]))
	 {
	     if (c == ':' || c == '\n' || c == '\\')
		 label[i] = '_';
	     i++;
	 }
	 app->name = XtNewString(label);
	 app->directory = XtNewString(resources.obj_dir);
	 app->fname = filename;
	 app->icon = XtNewString(obj->icon);
	 app->push_action = XtNewString(obj->atom_name);
	 app->drop_action = XtNewString("");
	 app->remove_file = True;
	 app->objType = objType;
	 insertNewApp(awi->win.aw, awi->item, applist, 1);
     }
 }
 if (fo)  fclose(fo);
 chdir(user.home);
}
