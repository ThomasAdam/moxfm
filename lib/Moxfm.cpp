!***********************************************************************
! Application defaults for moxfm.
! Most of these are the defaults anyway, but they are listed here so you can
! change them if necessary.
!***********************************************************************

! Search paths for icon files
*bitmapPath: BITMAPDIR
*pixmapPath: PIXMAPDIR

! Default applications file to be loaded at startup
*mainApplicationFile:	~/.moxfm/Main

! Configuration file (where the file types are specified)
*configFile: ~/.moxfm/XFMRC
#ifdef MAGIC_HEADERS
! If you want to have the possibility *not* to use magic headers
! for *certain* files, you must here specify a file type configuration
! file which does not use magic headers
*noMagicConfigFile:	~/.moxfm/xfmrc.nomagic
#endif
! Startup file (determines, which windows are opened at startup
! if none of the options -ignorestart, -appmgr, -filemgr or -dir
! are specified)
*startupFile:		~/.moxfm/startup

! Device configuration file (floppies and such)
*devFile:		~/.moxfm/moxfmdev

! The monitor config file (specifies files to be monitored during automatic
! updates)
*monitorFile:		~/.moxfm/xfmmon

! Directory where application files are to be created
*applCfgPath:		~/.moxfm/

! File which contains user defined object types
*userObjectFile:	~/.moxfm/targets

! Directory where temporary objects are to be created
*tmpObjectDir:		~/.moxfm/tmpobj

! Magic headers configuration file (files you recognize by their header)
*magicFile:		~/.moxfm/magic

! Standard icon for new application groups
*applBoxIcon:		stuff.xpm

! (Don't) save application files and desktop icons
! automatically on changes:
*autoSave: 		true

! (Don't) save window positions and contents automatically
! on exit:
*saveOnExit: 		false

! Double click time in milliseconds
*doubleClickTime:		300

! Time interval in milliseconds for automatic folder and application window
! updates (set to zero to disable this feature)
*updateInterval:		10000

! Set this to True, if every single file (not only the directory) shall be
! checked for changes on automatic folder updates
*checkFiles:			true

! Set this to True if application windows should be updated automatically
! Works only if Moxfm*autoSave is set to true!
*checkApplicationFiles:		true

!***********************************************************************
! Preferences
!***********************************************************************

! Geometry for application and initial file manager windows
*initAppGeometry:		332x358-95+230
*initFileGeometry:		509x448+69+136
! width x height for subsequent file windows
*fileGeometry:			500x400
! width x height for subsequent application windows
*appGeometry:			300x350

! Position of the first desktop icon:
*firstIconPos:			+0+0
! Alignment of desktop icons (row/column with start
! at the position of the first icon). Values: Up, Down, Left, Right
*iconAlignment:			Down

! NOTE: The background color of icons is determined from the Moxfm.background
! resource. Make sure that this resource is always given a value appropriate
! for *both* application and file windows.

! The background color for file and application windows
*background:			#8F5BA8F5C7AD
*menubar*background:		#91EABFFFD998

! The background color for desktop icon labels:
*dticonColor:			#8F5BA8F5C7AD

*XmTextField*background:	white
!*menubar*foreground:		black

*menubar*tearOffModel:	TEAR_OFF_DISABLED

! This color is used when an icon button
! is highlighted or marked
*selectColor:		red

! Specify colors for display of filenames:
*filenameColor:		black
*linknameColor:		maroon
! This color applies only to text display style
*textDirColor:		lemon chiffon
! Highlight color for desktop icons
*dropColor:		lemon chiffon

*file window*form.highlightColor:	yellow
*file window*icon box*highlightColor:	yellow
#ifdef SGI_MODE
*sgiMode: true
*XmScrolledWindow.scrolledWindowMarginWidth:	3
*XmScrolledWindow.scrolledWindowMarginHeight:	3
#endif
! Use these only if you  have the 75dpi fonts. (recommended)
*boldFont:			-adobe-helvetica-bold-r-*-*-12-*
*iconFont:			-adobe-helvetica-medium-r-*-*-10-*
*buttonFont:			-adobe-helvetica-medium-r-*-*-12-*
*menuFont:			-adobe-helvetica-medium-r-*-*-12-*
*labelFont:			-adobe-helvetica-medium-r-*-*-12-*
*statusFont:			-adobe-helvetica-medium-r-*-*-12-*
*ttyFont:			-adobe-courier-medium-r-*-*-12-*

! This is for file names in text display mode:
*filenameFont:			-adobe-courier-medium-r-*-*-12-*

! This must be a fixed width font
*cellFont:			fixed

*XmMessageBox*fontList:		-adobe-helvetica-medium-r-*-*-12-*
*XmSelectionBox*fontList:	-adobe-helvetica-medium-r-*-*-12-*
*XmPushButtonGadget*fontList:	-adobe-helvetica-medium-r-*-*-12-*
*Popup*fontList:		-adobe-helvetica-medium-r-*-*-12-*
*menubar*fontList:		-adobe-helvetica-bold-r-*-*-14-*
*menubar*Pulldown*fontList:	-adobe-helvetica-bold-r-*-*-12-*

! Specify the sizes of the different icon toggles. The following values
! are appropriate for the icons supplied with xfm.
*appIconWidth:		48
*appIconHeight:		48
*fileIconWidth: 	48
*fileIconHeight:	40

*Editform*XmTextField*width:	 300

! Confirmation for various operations
*confirmDeletes: 		true
*confirmDeleteFolder: 		true
*confirmCopies: 		false
*confirmMoves: 			true
*confirmLinks:			false
*confirmOverwrite:		true
*confirmQuit:			true

! If you are disturbed by frequent Xt warnings (type "actions
! not found"), you might want to set this to true:
*suppressWarnings:		false

! Set this to true so you can see when moxfm is copying files
*showCopyInfo:			true

! Echo actions on stderr (useful for debugging purposes)
*echoActions:			false

! Directory display in Text type
*showOwner: 			true
*showDate: 			true
*showPermissions: 		true
*showLength: 			true

! Open a new window when a new directory is selected from
! the parent directory popup menu:
*newWinOnDirPopup:		true

! Show hidden files:
*showHidden:			false
! Show directories first (default):
*dirsFirst:			true

! The type of the first and subsequent file windows
! valid values are Icons and Text
*initialDisplayType:	 	Icons

! The type of sorting used by default
! valid values are SortBy{Name,Size,Date}
*defaultSortType: 		SortByName

! This defines the naming policy for files dropped on
! an application button. There are three valid values:
! FileName (the drop action is performed in the directory
! of the dropped files), RelativePath (path relative to the
! directory specified for the application button; default) and
! AbsolutePath.
!*passedNamePolicy:		RelativePath

! The default editor to use
*defaultEditor: 		exec emacs

! The default viewer to use
*defaultViewer:			exec xless

! The xterm to call
*xterm:				exec xterm -sb -sl 500 -j

*startFromXterm:		false
*keepXterm:			false

!***********************************************************************
! Miscellaneous settings (see also FmMain.c)
! Normally you won't have to change these.
!***********************************************************************

*borderWidth: 0
*DragReceiverProtocolStyle: DRAG_PREFER_PREREGISTER
