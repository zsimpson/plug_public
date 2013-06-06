// @ZBS {
//		*INTERFACE gui
//		*SDK_DEPENDS freeimage pcre pthread
//		*MODULE_DEPENDS zuiradiobutton.cpp zuicheck.cpp zuitexteditor.cpp zuifilepicker.cpp zuivar.cpp zuislider.cpp zuitabbutton.cpp zuitabpanel.cpp zglgfiletools.cpp 
//		*EXTRA_DEFINES ZMSG_MULTITHREAD
// }

//
// This template can be used to quickly create a zlab plugin that uses the _kin style left panel UI
// and standard file open/save/saveas etc... functionality, tutorial dialog, recent options, etc.
//
// The batch file makeplugin.bat in this folder will make a new folder and copy these files to
// new files with your new plugin name:
//
//	makeplugin thomas
//
//  created _thomas folder, with files _thomas.cpp and _thomas.zui
//
//  1.  Search and repace "templ2" with "yourpluginname" in the new files.
//	2.	You can remove the line *EXTRA_DEFINES ZMSG_MULTITHREAD, as well as the pthread
//      entry in SDK_DEPENDS at top if you're single threaded.
//  3.  run 'perl zlabbuild.pl' and select your new plugin, create the makefile.
//
// There are a lot more includes below than needed, esp. from ZBSLIB, remove at will.
// There are also many more items in MODULE_DEPENDS at top than you may need, but these
// provide a pretty complete UI (zui) impl to start with.
//


// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"

#ifdef WIN32
#undef APIENTRY
#undef WINGDIAPI
#endif

// SDK includes (from sdkpub):

// STDLIB includes (from OS):
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "ctype.h"
#ifndef __APPLE__
	#include "malloc.h"
#endif

// MODULE includes (stuff you write):

// ZBSLIB includes (from zbslib):
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zstr.h"
#include "zui.h"
#include "ztmpstr.h"
#include "zuivar.h"
#include "zfilespec.h"
#include "zmousemsg.h"
#include "zhashtable.h"
#include "zwildcard.h"
#include "ztime.h"
#include "zglfwtools.h"
#include "zconfig.h"
#include "zgltools.h"
#include "zglfont.h"
#include "zbitmapdesc.h"
#include "zgraphfile.h"

extern void trace( char *, ... );
extern char * getUserLocalFilespec( char *basename, int bMustExist );
extern ZHashTable options;
	// all imported from main.cpp

//==================================================================================================
// Local Types

struct templ2Model {
	ZHashTable properties;
	void clear() {
		properties.clear();
	}
	bool load( char *filename ) { return true; }
	bool save( char *filename ) { return true; }
};
	// the "model" is whatever this app considers "the world" -- it is the definitive model
	// in the MVC scheme, for example.  The model is what is loaded/saved by IO functions.


//==================================================================================================
// Forward declares (outside plugin namespace); utility functions; see end of this file for impl.

void updateDefaultPaths( char *filespec, char *key );
	// called when a user loads or saves a file to store the default location by type

char * getVersionString();
	// get the version string that is embedded in the TITLE macro

void glPrint( int x, int y, char *text );
	// handy print in screen coords can be called from within 3d rendering code
	// while preserving the matrices.

char tmpMessage[256];
int tmpMessageTime=0;
unsigned char tmpMessageColor[3];
void tempMessage( char *msg, char color='w', int time=100 );
	// for displaying temporary messages onscreen

void capturePluginImage( char *filename, bool notify=true, bool renderPanelOnly=false );
	// take a screen shot of the plugin window

//==================================================================================================
// ZVAR definitions: these are outside the scope of the plugin namespace for easier access
// in other modules.
//
// Keep these in alphabetical order, and use a prefix like "fit" etc... to group related items.
//
//ZVARB( float, Kin_errRelContribDeriv, 1.f, 0.f, 1.f );
//ZVAR( float, Kin_errToleranceEpsilon, 1e-18f );

//==================================================================================================
// Globals

int gMultiThreaded = 0;
	// are we running multithreaded?

int gIdling = 0;
	// is the program idle (no user input)

ZUI *gMainRenderPanel = 0;
	// the zui that contains the "view" on the model; this is
	// the ZUI called "pluginPanel"

#define MAINUI_PANEL_WIDTH (300)
ZUI *gMainUIPanel = 0;
	// the zui that contains the main ui panel, if any

templ2Model model;
	// the global model

//==================================================================================================
// Plugin

ZPLUGIN_BEGIN( templ2 );

void saveRecentOptions();
	// saves current options to "recent options" file unique per plugin
void loadRecentOptions();
	// load recent options

void startup() {
	printf( "startup...\n");
	loadRecentOptions();
		// load recent.opt file with last user settings

	if( options.getI("skipTutorial") ) {
		zMsgQueue( "type=ZUIHide toZUI=Tutorial" );
	}

	// START threads if configured
	gMultiThreaded = options.getI( "multiThread", 0 );
	if( gMultiThreaded ) {
		// computeThreadStart();
			// call your thread initialization routines here
	}

	// LOAD textures
	// chevronTex = zglGenTextureRGBAZBits( "_kin/chevron2.png", 0, 1 );

	// LOAD fonts
	// zglFontLoad( "verdana12", "verdana.ttf", 12 );

	// LOAD the MainUIPanel ui definition
	ZUI::zuiExecuteFile( ZTmpStr( "_%s/_%s.zui", thisPluginName, thisPluginName ) );

	// RESIZE the plugin panel & set it up for vertical scrolling
	gMainRenderPanel = ZUI::zuiFindByName( "pluginPanel" );
	gMainUIPanel = ZUI::zuiFindByName( ZTmpStr( "%sMainUIPanel", thisPluginName ) );
	gMainUIPanel->putI( "layoutManual_w", MAINUI_PANEL_WIDTH );
	if( options.getI( "hideMainUI", 0 ) ) {
		gMainUIPanel->putI( "hidden", 1 );		
	}
	else {
		zMsgQueue( "type=ZUISet key=layoutManual_x val='%d' toZUI=pluginPanel", MAINUI_PANEL_WIDTH );
		zMsgQueue( "type=ZUISet key=layoutManual_w val='W %d -' toZUI=pluginPanel; type=ZUILayout toZUI=pluginPanel", MAINUI_PANEL_WIDTH );
	}
	int viewport[4];
	glGetIntegerv( GL_VIEWPORT, viewport );
	if( gMainUIPanel ) {
		gMainUIPanel->putI( "scrollYHome", -10000+viewport[3] ); 
		gMainUIPanel->scrollY = (float)( -10000+viewport[3] );
	}
	zMsgQueue( "type=ZUILayout" );

	// LOAD last file loaded, if it exists.
	model.clear();
	char *filename = options.getS( "lastFileLoaded", 0 );
	if( filename ) {
		if( zWildcardFileExists( filename ) ) {
			zMsgQueue( ZTmpStr( "type=File_Open filespec='%s'",filename ) );
		}
	}
}

void shutdown() {
	// SHUTDOWN the other threads
	if( gMultiThreaded ) {
		//computeThreadStop();
			// stop your compute threads here
	}

	// RESIZE the plugin panel
	zMsgQueue( "type=ZUISet key=layoutManual_x val='0' toZUI=pluginPanel" );
	zMsgQueue( "type=ZUISet key=layoutManual_w val='W' toZUI=pluginPanel; type=ZUILayout toZUI=pluginPanel" );

	// DELETE textures, fonts

	// SAVE recent/default user settings
	saveRecentOptions();
}

void update() {
	if( gMultiThreaded ) {
		// Frame-limit render thread to 30fps
		static ZTime lastTime = 0;
		ZTime thisTime = zTimeNow();
		int sleepMils  =  (int) ( 33.333333  - (thisTime - lastTime)  * 1000.0 );
		if( sleepMils > 0 ) {
			zTimeSleepMils( sleepMils );
		}
		lastTime = thisTime;
	}

	// simple-minded yield on no user activity
	gIdling = false;
	#define TIMEOUT_SECS 3
	bool computationActive = false;
		// set the above based on whether your app is currently computing anything; if 
		// computation is not active, we'll do a lot of sleeping when the user is idle.
		// note that this sleep is for the current (render) thread.
	if( !computationActive && zTimeNow() - zglfwGetLastActivity() > TIMEOUT_SECS ) {
			zTimeSleepMils( 200 );
			gIdling=true;
	}
}

void render() {
	// Hack temp messages here:
	glColor3ub ( tmpMessageColor[0], tmpMessageColor[1], tmpMessageColor[2] );
	if( tmpMessageTime && *tmpMessage ) {
		tmpMessageTime--;
		glPrint( 5, (int)gMainRenderPanel->h - 15, tmpMessage );
			// could use zglFontPrint here; glPrint saves all matrices & restores,
			// so 2d printing can be done with pixel matrix during 3d renders.
	}
}

void syncUIToModel() {
}

// TEMPLATE (generic) Message Handlers
//----------------------------------------------------------------------------------

ZMSG_HANDLER( ZLAB_Shutdown ) {
	shutdown();
//	exit( 0 );
}

ZMSG_HANDLER( File_Open ) {
	// opens filespec if passed, else creates filepicker to select file
	if( zmsgHas(filespec) ) {
		char *filespec = zmsgS(filespec);
		if ( zWildcardFileExists( filespec ) ) {
			if( model.load( filespec ) ) {
				updateDefaultPaths( filespec, "loadPath" );
				options.putS( "lastFileLoaded", filespec );
				model.properties.putS( "filename", filespec );
				syncUIToModel();
				zMsgQueue( "type=ZUISet key=text val='%s' toZUI=modelFilename", filespec );
				zMsgQueue( "type=ZUILayout" );
					// because long filename may wordwrap & need new layout
			}
			else {
				// modelLoad failed.  Display notice to user & reset UI
				zuiMessageBox( "File Open Error", "The file you have selected does not appear to be in a supported format.", 1 );
			}
		}
		else {
			// bad filename was passed; last model remains open
			zuiMessageBox( "File Open Error", ZTmpStr( "The filename '%s' is invalid.", filespec ), 1 );
		}
	}
	else {
		zuiChooseFile( "Open File", options.getS( "loadPath" ), "type=File_Open", 0, true );
	}
}

char overwriteExistingFilename[256] = {0};
ZMSG_HANDLER( File_Save ) {
	char *filename=0;
	if( zmsgI( overwriteExisting ) ) {
		filename = overwriteExistingFilename;
			// this hack is to get around single/double quote problems in sequential message-passing
			// of filenames that contain spaces.
	}
	else {
		filename = zmsgHas(filespec) ? zmsgS(filespec) : model.properties.getS( "filename" );
	}
	if ( filename && filename[0] ) {
		// WARN user if existing filename was chosen from "SaveAs" operation.
		if( zmsgI( fromSaveAs ) ) {
			if( zWildcardFileExists( filename ) ) {
				char *msg   = "A file with that name already exists.  Press OK to overwrite the existing file.";
				char *okmsg = "type=File_Save overwriteExisting=1";
				strcpy( overwriteExistingFilename, filename );
				char *cancelmsg = "type=File_SaveAs";
				zuiMessageBox( "Overwrite File?", msg, 0, okmsg, cancelmsg );
				return;	
			}
		}
		options.putS( "lastFileLoaded", filename );
		updateDefaultPaths( filename, "loadPath" );
		model.save( filename );
		model.properties.putS( "filename", filename );
		filename = model.properties.getS( "filename" );	
		zMsgQueue( "type=ZUISet key=text val='%s' toZUI=modelFilename", filename );
		zMsgQueue( "type=ZUILayout" );
			// because long filename may wordwrap & need new layout
	}
	else {
		zMsgQueue( "type=File_SaveAs" );
	}
}

ZMSG_HANDLER( File_SaveAs ) {
	// CREATE and display a ZUIFilePicker to select the filename to save
	zuiChooseFile( "Save File", options.getS( "savePath" ), "type=File_Save fromSaveAs=1", NULL, true );
}

ZMSG_HANDLER( ZLAB_ToggleUI ) {
	// tfb: I've played with a couple options here.  The most obvious thing to do is hide the root UI 
	// node, which I did first.  The downside to this is that keyBindings for UI that is hidden are
	// not processed: so you can't control the program with the UI hidden!  So I tried moving the UI
	// offscreen.  This works, and key bindings work: interestingly, when a keyBinding is executed,
	// the UI comes back onscreen; surely this is because my hacked method of moving it offscreen 
	// is not quite right.  But the bigger downside to this approach is that either the visible
	// performance gain to be had in hiding the UI is not had when just moving it offscreen.  This makes
	// sense: it isn't the triangles that are costing us with the UI, but all the logic.
	//
	assert( gMainUIPanel );
	if( gMainUIPanel->getI( "hidden" ) ) {
		// MAKE Visible
		zMsgQueue( "type=ZUISet key=hidden val=0 toZUI=%s", gMainUIPanel->name );
		zMsgQueue( "type=ZUISet key=layoutManual_x val=%d toZUI=pluginPanel", MAINUI_PANEL_WIDTH );
		zMsgQueue( "type=ZUISet key=layoutManual_w val='W %d -' toZUI=pluginPanel; type=ZUILayout toZUI=pluginPanel", MAINUI_PANEL_WIDTH );
		syncUIToModel();
		tmpMessageTime = 0;
		options.putI( "hideMainUI", 0 );
	}
	else {
		// HIDE: 
		zMsgQueue( "type=ZUISet key=hidden val=1 toZUI=%s", gMainUIPanel->name );
		zMsgQueue( "type=ZUISet key=layoutManual_x val=0 toZUI=pluginPanel" );
		zMsgQueue( "type=ZUISet key=layoutManual_w val='W 0 -' toZUI=pluginPanel; type=ZUILayout toZUI=pluginPanel");
		tempMessage( "F8 to show UI" );
		options.putI( "hideMainUI", 1 );
	}
}

ZMSG_HANDLER( TutorialOK ) {
	ZUI *z = ZUI::zuiFindByName( "skipTutorial" );
	options.putI( "skipTutorial", z->getI("selected") );
	saveRecentOptions();
}

void handleMsg( ZMsg *msg ) {
	if( zmsgIs( type, KeyDown ) ) {
		if( zmsgIs( which, f8 ) )  {
			// toggle visibility of the main UI
			zMsgQueue( "type=ZLAB_ToggleUI" );
			zMsgUsed();
		}
		else if( zmsgIs( which, ctrl_p ) )  {
			// screen capture
			char filename[256];
			static int nextimg = 0;
			for( int i=nextimg; ; i++ ) {
				sprintf( filename, "zlab%04d.png", i );
				if( !zWildcardFileExists( filename ) ) {
					nextimg = i+1;
					break;
				}
			}
			capturePluginImage( filename );
			zMsgUsed();
		}
	}
}


//-----------------------------------------------------------------------------
// Some utility fns that might get factored out of this plugin later...

ZHashTable recentOpt;
void saveRecentOptions() {
	recentOpt.clear();
	recentOpt.putS( "version", getVersionString() ); 
	recentOpt.putS( "loadPath", options.getS( "loadPath" ) );
	recentOpt.putS( "savePath", options.getS( "savePath" ) );
	if( options.has( "lastFileLoaded" ) ) {
		recentOpt.putS( "lastFileLoaded", options.getS( "lastFileLoaded" ) );
	}
	recentOpt.putI( "skipTutorial", options.getI( "skipTutorial", 0 ) );
	recentOpt.putI( "hideMainUI", options.getI( "hideMainUI", 0 ) );
	zConfigSaveFile( getUserLocalFilespec( ZTmpStr( "%srecent.opt", templ2::thisPluginName ), 0 ), recentOpt );
}

void loadRecentOptions() {
	zConfigLoadFile( getUserLocalFilespec( ZTmpStr( "%srecent.opt", thisPluginName ), 1 ), recentOpt );
	char *optionsVersion = recentOpt.getS( "version" );
	if( !optionsVersion || strcmp( optionsVersion, getVersionString() ) ) {
		recentOpt.clear();
		zConfigLoadFile( ZTmpStr( "%srecent.opt", thisPluginName ), recentOpt );
			// force load of shipped options file if the version is new
	}
	options.putS( "loadPath", recentOpt.getS( "loadPath", "./*" ) );
	options.putS( "savePath", recentOpt.getS( "savePath", "./*" ) );
	if( recentOpt.has( "lastFileLoaded" ) ) {
		options.putS( "lastFileLoaded", recentOpt.getS( "lastFileLoaded" ) );
	}
	options.putI( "skipTutorial", recentOpt.getI( "skipTutorial", 0 ) );
	options.putI( "hideMainUI", recentOpt.getI( "hideMainUI", 0 ) );
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( update );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );


ZPLUGIN_END;

void updateDefaultPaths( char *filespec, char *key ) {
	// CREATE & STORE a path with * wildcard to save user preferred location of loading files.
	if( zWildcardFileExists( filespec ) ) {
		ZFileSpec fs ( filespec );
		char *drive = fs.getDrive();
		char *dir = fs.getDir();
		char *file = fs.getFile( 0 );
		char *ext = fs.getExt();
		ZFileSpec fs2 ( zFileSpecMake(   FS_DRIVE, drive, FS_DIR, dir, FS_FILE, "*", FS_EXT, ext, FS_END ) );
		options.putS( key, fs2.path );
	}
}

char * getVersionString() {
	// get the version string that is embedded in the TITLE macro
#ifdef TITLE
	static ZRegExp versionStr( "\\S+ Version [0-9]\\.[0-9]\\.[0-9]+" );
	versionStr.test( TITLE );
	return versionStr.get( 0 );
#else
	static char *unk = "Unknown";
	return unk;
#endif
}

void glPrint( int x, int y, char *text ) {
	glPushAttrib( GL_ALL_ATTRIB_BITS );
	glDisable( GL_LIGHTING );
	glDisable( GL_COLOR_MATERIAL );
	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_FOG );
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL );

	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	zglPixelMatrixFirstQuadrant();

	zglFontPrint( text, (float)x, (float)y, "controls" );

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glPopAttrib();
}

void tempMessage( char *msg, char color, int time ) {
	switch( color ) {
		case 'r':
			tmpMessageColor[0] = 255;
			tmpMessageColor[1] = 0;
			tmpMessageColor[2] = 0;
		break;

		default:
			tmpMessageColor[0] = tmpMessageColor[1] = tmpMessageColor[2] = 255;
	} 
	strcpy( tmpMessage, msg );
	tmpMessageTime = time;
}

void capturePluginImage( char *filename, bool notify, bool renderPanelOnly ) {
	#define BYTES_PER_PIXEL 4

	ZUI *captureZUI = renderPanelOnly ? gMainRenderPanel : ZUI::zuiFindByName( "root" );
	assert( captureZUI );

	int imageWidth = (int)captureZUI->w & ~1;
		// force even - required if image will be converted to mpeg movie
	int imageHeight = (int) captureZUI->h;

	static unsigned char * pixels = 0;
	static long pixelsize = 0;
	long sizeRequest = imageWidth * imageHeight * BYTES_PER_PIXEL;
	if ( sizeRequest > pixelsize ) {
		if( pixels ) {
			delete [] pixels;
		}
		pixels = new unsigned char[ sizeRequest ];
		pixelsize = sizeRequest;
	}

	glReadPixels( (int)captureZUI->x, (int)captureZUI->y, imageWidth, imageHeight, GL_RGBA,  GL_UNSIGNED_BYTE, pixels);

	// write pixels to file
	ZBitmapDesc bmd;
	bmd.init( imageWidth, imageHeight, BYTES_PER_PIXEL, (char *)pixels );
	zBitmapDescInvertLines( bmd );

	#ifdef __APPLE__
	zBitmapDescInvertBGR( bmd );
		// Flips the byte order of the bits in a bitmap of depth 3 or 4
	#endif
	int result = zGraphFileSave( filename, &bmd );
	if( notify ) {
		if( result ) {
			tempMessage( ZTmpStr( "Saved %s", filename ) );
		}
		else {
			tempMessage( ZTmpStr( "Error: Can't save %s", filename ), 'r' );
		}
	}
}
