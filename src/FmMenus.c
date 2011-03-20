/*-----------------------------------------------------------------------------
  Module FmMenus.c                                                             

  (c) Oliver Mai 1995, 1996
                                                                           
  some functions for building and posting menus                                                   
-----------------------------------------------------------------------------*/
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/CascadeBG.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include "Am.h"

AppWindowRec *findAppWidgetByShell(Widget shell)
{
 AppWindowRec *aw;

 for (aw = app_windows; (aw); aw = aw->next)
 {
     if (aw->shell == shell)  return aw;
 }
 return NULL;
}

void buttonPopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
 Window root, child, win;
 Widget button;
 AppWindowRec *aw;
 int x, y, x_win, y_win;
 unsigned int mask;

 if (!(aw = findAppWidgetByForm(w)) || (aw->blocked))
     return;
 win = XtWindow(aw->icon_box);
 XQueryPointer(dpy, win, &root, &child, &x, &y, &x_win, &y_win, &mask);
 if (child != None)  XTranslateCoordinates(dpy, win, child, x_win, y_win, &x_win, &y_win, &child);
 if (child != None)
 {
     button = XtWindowToWidget(dpy, child);
     XmProcessTraversal(button, XmTRAVERSE_CURRENT);
     XtVaSetValues(aw->form, XmNuserData, (XtPointer) button, NULL);
     XmMenuPosition(aw->buttonPopup, (XButtonPressedEvent *) event);
     XtManageChild(aw->buttonPopup);
 }
 else
 {
     XmMenuPosition(aw->appboxPopup, (XButtonPressedEvent *)event);
     XtManageChild(aw->appboxPopup);
 }
}

Widget BuildMenu(Widget parent, int type, char *title, char mnemonic, Boolean tear_off, MenuItemList items)
{
 Widget menu, cascade, widget;
 Arg args[3];
 int i, nargs;
 XmString str;

 XtSetArg(args[0], XmNforeground, resources.label_color);
 if (type == XmMENU_PULLDOWN)
 {
     menu = XmCreatePulldownMenu(parent, "Pulldown", args, 1);
     str = XmStringCreateLocalized(title);
     cascade = XtVaCreateManagedWidget(title, xmCascadeButtonGadgetClass, parent, XmNsubMenuId, menu, XmNlabelString, str, XmNmnemonic, mnemonic, XmNforeground, resources.label_color, NULL);
     XmStringFree(str);
 }
 else  menu = XmCreatePopupMenu(parent,"Popup", args, 1);
 if (tear_off)  XtVaSetValues(menu, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);

 for (i=0; (items[i].label); i++)
 {
     if (items[i].subitems)  widget = BuildMenu (menu, XmMENU_PULLDOWN, items[i].label, items[i].mnemonic, tear_off, items[i].subitems);
     else
     {
	 nargs = 0;
	 if (items[i].accelerator)
	 {
	     XtSetArg(args[nargs], XmNaccelerator, items[i].accelerator);  nargs++;
	     str = XmStringCreateLocalized(items[i].accel_text);
	     XtSetArg(args[nargs], XmNacceleratorText, str);  nargs++;
	 }
	 if (items[i].mnemonic)
	 {
	     XtSetArg(args[nargs], XmNmnemonic, items[i].mnemonic);  nargs++;
	 }
	 widget = XtCreateManagedWidget(items[i].label, *items[i].class, menu, args, nargs);
     }
     if (items[i].callback)
     {
	 if (items[i].class == &xmToggleButtonWidgetClass ||
	    items[i].class == &xmToggleButtonGadgetClass)
	     XtAddCallback(widget, XmNvalueChangedCallback, items[i].callback, items[i].callback_data);
	 else
	     XtAddCallback(widget, XmNactivateCallback, items[i].callback, items[i].callback_data);
     }
     items[i].object = widget;
 }
 return ((type == XmMENU_POPUP)? menu : cascade);
}
