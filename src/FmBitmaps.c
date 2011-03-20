/*---------------------------------------------------------------------------
  Module FmBitmaps

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  (c) Oliver Mai 1995
  
  Functions & data for handling the bitmaps.
---------------------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Drawing.h>
#include <X11/xpm.h>

#include "../lib/bitmaps/xfm_watch.xbm"
#include "../lib/bitmaps/xfm_watchmsk.xbm"

#include "../lib/pixmaps/file.xpm"
#include "../lib/pixmaps/folder.xpm"
#include "../lib/pixmaps/folder_up.xpm"
#include "../lib/pixmaps/app.xpm"
#include "../lib/pixmaps/files.xpm"
#include "../lib/pixmaps/file_link.xpm"
#include "../lib/pixmaps/folder_link.xpm"
#include "../lib/pixmaps/app_link.xpm"
#include "../lib/pixmaps/file_link_bad.xpm"

#include "../lib/pixmaps/stuff.xpm"
#include "../lib/pixmaps/floppy.xpm"

#include "../lib/pixmaps/recycle_button.xpm"
#include "../lib/pixmaps/home_button.xpm"
#include "../lib/pixmaps/folder_button.xpm"
#include "../lib/pixmaps/stuff_button.xpm"
#include "../lib/pixmaps/folder_up_button.xpm"
#include "../lib/pixmaps/backarr.xpm"

#include "Am.h"
#include "Fm.h"

#define CLOSENESS 80000

/*-----------------------------------------------------------------------------
  STATIC DATA
-----------------------------------------------------------------------------*/

typedef struct {
  char *bits;
  int width, height;
} BitmapRec;

#ifdef __STDC__
#define ICON(x) { x##_bits, x##_width, x##_height }
#else
#define ICON(x) { x/**/_bits, x/**/_width, x/**/_height }
#endif

static BitmapRec bitmaps[2] = {
    ICON(xfm_watch), ICON(xfm_watchmsk)
};

static char **pixmaps[] = {
  files_xpm, folder_xpm, folder_up_xpm, file_xpm,
  app_xpm, file_link_xpm, folder_link_xpm, app_link_xpm,
  file_link_bad_xpm, folder_xpm, stuff_xpm, stuff_xpm, floppy_xpm,
  recycle_button_xpm, home_button_xpm, folder_button_xpm, stuff_button_xpm, folder_up_button_xpm, backarr_xpm
};

/*-----------------------------------------------------------------------------
  PUBLIC DATA
-----------------------------------------------------------------------------*/

IconRec *icons;
Cursor cur;

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

void readBitmaps(void)
{
  int i, scrn;
  Colormap cmp;
  Window win;
  XColor black, white;
  XpmAttributes xpm_attr;
  static XpmColorSymbol none_color = { NULL, "None", (Pixel)0 };
  IconRec cursor;

  win = DefaultRootWindow(dpy);
  scrn = DefaultScreen(dpy);
  cmp = DefaultColormap(dpy, scrn);

  black.pixel = BlackPixel(dpy, scrn);
  XQueryColor(dpy, cmp, &black);
  white.pixel = WhitePixel(dpy, scrn);
  XQueryColor(dpy, cmp, &white);

  icons = (IconRec *) XtMalloc(END_BM * sizeof(IconRec));

  /* create the hardcoded cursor */

  cursor.bm = XCreateBitmapFromData(dpy, win, bitmaps[0].bits, bitmaps[0].width, bitmaps[0].height);
  cursor.mask = XCreateBitmapFromData(dpy, win, bitmaps[1].bits, bitmaps[1].width, bitmaps[1].height);
  cur = XCreatePixmapCursor(dpy, cursor.bm, cursor.mask, &black, &white, 16, 16);

  xpm_attr.valuemask = XpmReturnPixels | XpmColorSymbols | XpmCloseness;
  xpm_attr.colorsymbols = &none_color;
  xpm_attr.numsymbols = 1;
  xpm_attr.closeness = CLOSENESS;
  none_color.pixel = winInfo.background;
  for (i=0; i<END_BM; i++)
  {
      XpmCreatePixmapFromData(dpy, win, pixmaps[i], &icons[i].bm, &icons[i].mask, &xpm_attr);
      icons[i].width = xpm_attr.width;
      icons[i].height = xpm_attr.height;
  }
}

/*-----------------------------------------------------------------------------*/

IconRec readIcon(Widget shell, char *name)
{
  Window win = DefaultRootWindow(dpy);
  Screen *scrn = XtScreen(shell);
  IconRec icon;
  char fullname[MAXPATHLEN];
  int x, y;
  XpmAttributes attr;
  static XpmColorSymbol none_color = { NULL, "None", (Pixel) 0 };

  none_color.pixel = winInfo.background;

  /* first search for xpm icons: */

  attr.valuemask = XpmReturnPixels | XpmColorSymbols | XpmCloseness;
  attr.colorsymbols = &none_color;
  attr.numsymbols = 1;
  attr.closeness = CLOSENESS;

  if (XpmReadFileToPixmap(dpy, win, searchPath(fullname, resources.pixmap_path, name), &icon.bm, &icon.mask, &attr) == XpmSuccess)
  {
      icon.width = attr.width;
      icon.height = attr.height;
      return icon;
  }

  icon.mask = None;

  /* now search along *bitmapPath: */

  if (XReadBitmapFile(dpy, win, searchPath(fullname, resources.bitmap_path, name), &attr.width, &attr.height, &icon.bm, &x, &y) == BitmapSuccess)
  {
      icon.width = attr.width;
      icon.height = attr.height;
  }

  /* finally search bitmap in standard locations (*bitmapFilePath): */

  else
      icon.bm = XmuLocateBitmapFile(scrn, name, NULL, 0, (int *)&icon.width, (int *)&icon.height, &x, &y);
  return icon;
}

/*---------------------------------------------------------------------------*/

void freeIcon(IconRec icon)
{
 if (icon.bm != None)
 {
     freePixmap(icon.bm);
     if (icon.mask != None)  freePixmap(icon.mask);
 }
}

/*---------------------------------------------------------------------------*/

Boolean freePixmapProc(Pixmap icon_bm)
{
 XFreePixmap(dpy, icon_bm);
 return True;
}
