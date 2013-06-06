// @ZBS {
//		*SDK_DEPENDS pthread python
//		*MODULE_DEPENDS pmafile.cpp colorpalette.cpp zuifilepicker.cpp zuitexteditor.cpp zuivar.cpp zuicheck.cpp
//		*EXTRA_DEFINES ZMSG_MULTITHREAD
// }

// OPERATING SYSTEM specific includes:
#include "windows.h"

#include "zpython.h"

#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
#include "pthread.h"
// STDLIB includes:
#include "assert.h"
#include "stdio.h"
#include "float.h"
#include "stdlib.h"
#ifdef WIN32
	#include "malloc.h"
	#include "direct.h"	// mkdir
#endif
// MODULE includes:
#include "pmafile.h"
#include "colorpalette.h"
#include "math.h"
// ZBSLIB includes:
#include "zplugin.h"
#include "zgltools.h"
#include "zviewpoint.h"
#include "zvars.h"
#include "zui.h"
#include "zwildcard.h"
#include "ztmpstr.h"
#include "pmafile.h"
#include "zidget.h"
#include "zmathtools.h"
#include "colorpalette.h"
#include "zconfig.h"
#include "ztl.h"
#include "ztime.h"
#include "zfilespec.h"
#include "zgraphfile.h"
#include "mainutil.h"


extern int useDirtyRects;

ZPLUGIN_BEGIN( fretview );

// This is the number of regions that you can have - used to be two, and KD is upping it to three.
#define REGION_MAX	(5)
int regionCount = 3;	// hard coded for a two pane calibration; It is updated by checking the Fretview_regionCount and then updating...


ZVARB( int, Fretview_regionCount, regionCount, 1, REGION_MAX );
ZVAR( float, Fretview_scale0, 1.0 );
ZVAR( float, Fretview_scale1, 1e2 );
ZVAR( float, Fretview_peakMaskHatRadius, 3.f );
ZVAR( float, Fretview_peakMaskBrimRadius, 3.f );
ZVAR( float, Fretview_peakMaskAnnulusRadius, 3.f );
ZVAR( float, Fretview_peakContentionRadius, 8.f );
ZVAR( float, Fretview_peakMassThreshold, 1e-2 );
ZVAR( int, Fretview_calibRender, 0 );
ZVAR( int, Fretview_dumpAllTime, 0 );
ZVAR( double, Fretview_acX, 4 );
ZVAR( double, Fretview_acY, 0.2 );
ZVAR( float, Fretview_fretY, 2e3 );



char fretviewDir[256] = {0,};

#define NUM_LAGS (64)

#undef AUTO_CORR	// remove the auto-correlation feature

#ifdef AUTO_CORR
double *gAutoCorr = 0;
#endif

// Zidgets
//----------------------------------------------------------------------------------------------------------------------------
struct TimeTrace {
	float *data;
	int count;

	TimeTrace() {
		data = 0;
		count = 0;
	}
	void alloc(int _count) {
		if( data ) {
			delete data;
			count = 0;
			data = 0;
		}
		count = _count;
		data = new float[count];
		memset(data,0,sizeof(float)*count);
	}
};


struct ZidgetPlotFret : ZidgetPlot {
	TimeTrace timeTrace[REGION_MAX];

	void extractAreaTimeTrace( int regionIndex, int radius, IVec2 select );
};

ZidgetPlotFret plotTime;

FVec4 regionColor[REGION_MAX] = {
	FVec4( 0.5f, 1.0f, 0.5f, 0.7f ),
	FVec4( 1.0f, 0.7f, 0.7f, 0.7f ),
	FVec4( 0.7f, 0.7f, 1.0f, 0.7f ),
	FVec4( 1.f, 1.f, 0.f, 0.5f ),
	FVec4( 0.f, 1.f, 1.f, 0.5f )
};

struct Region
{
	int index;
	ZidgetImage imageRaw;
	ZidgetImage imageWork;
	ZidgetPixelCircle selectRaw;
	ZidgetPixelCircle selectWork;
	ZidgetHistogram histogramRaw;
	ZidgetHistogram histogramWork;
	int meaningfulHistCount;
	FVec4 getColor() { return regionColor[index]; }
	void init(int r) {
		index = r;
		meaningfulHistCount = 4096-1;
		imageRaw.setName(ZTmpStr("region[%d].imageRaw",r));
		imageWork.setName(ZTmpStr("region[%d].imageWork",r));
		selectRaw.setName(ZTmpStr("region[%d].selectRaw",r));
		selectWork.setName(ZTmpStr("region[%d].selectWork",r));
		histogramRaw.setName(ZTmpStr("region[%d].histogramRaw",r));
		histogramWork.setName(ZTmpStr("region[%d].histogramWork",r));
	}
};

Region region[REGION_MAX];
const int REGION_LEFT = 0;
const int REGION_SECOND = 1;

const float selectRadius = 3.0f;

static int gLastFrame = -1;
static FVec2 gLastSelect = FVec2(0.f,0.f);

int gPeaksVisible = 0;

ZidgetLine2 timeSelect;

ZidgetRectLBWH rgnSelect;
ZidgetPos2 rgnSelectStart;
ZidgetPos2 rgnSelectStop;

int gViewingWorkImage = 0;

// Load / Save
//----------------------------------------------------------------------------------------------------------------------------

VidFile *vidFile = 0;

struct Load {
	pthread_t threadID;
	int haltSignal;
	char *filespec;
	int frameStart;
	int frameCount;
	float statusMonitor;
	int status;
	char lastLoadedFilename[256];

	Load() {
		haltSignal = 0;
		filespec = 0;
		frameStart = 0;
		frameCount = -1;
		statusMonitor = 0.f;
		status = 0;
		memset( lastLoadedFilename, 0, sizeof(lastLoadedFilename) );
	}
	void clear() {
		haltSignal = 0;
		filespec = 0;
		frameStart = 0;
		frameCount = -1;
		statusMonitor = 0.f;
		status = 0;
		lastLoadedFilename[0]=0;
	}
	boolean inProgress() { return status == -1; }
	boolean neverStarted() { return status == 0; }
	boolean complete() { return status == 1; }
};

void updateCalibMessage();

Load load;

ZHashTable fretviewConfig;

void saveConfig() {
	fretviewConfig.putF( "zviewpointTrans.x", zviewpointTrans.x );
	fretviewConfig.putF( "zviewpointTrans.y", zviewpointTrans.y );
	fretviewConfig.putF( "zviewpointScale", zviewpointScale );

	for( int r=0 ; r<REGION_MAX ; ++r )
	{
		fretviewConfig.putF( ZTmpStr( "region[%d].histogramRaw.histScale0", r), region[r].histogramRaw.histScale0 );
		fretviewConfig.putF( ZTmpStr( "region[%d].histogramRaw.histScale1", r), region[r].histogramRaw.histScale1 );

		fretviewConfig.putF( ZTmpStr( "region[%d].histogramWork.histScale0", r), region[r].histogramWork.histScale0 );
		fretviewConfig.putF( ZTmpStr( "region[%d].histogramWork.histScale1", r), region[r].histogramWork.histScale1 );
	}

	fretviewConfig.putF( "rgnSelectStart.pos.x", rgnSelectStart.pos.x );
	fretviewConfig.putF( "rgnSelectStop.pos.x", rgnSelectStop.pos.x );

	fretviewConfig.putF( "Fretview_peakMaskHatRadius", Fretview_peakMaskHatRadius );
	fretviewConfig.putF( "Fretview_peakMaskBrimRadius", Fretview_peakMaskBrimRadius );
	fretviewConfig.putF( "Fretview_peakMaskAnnulusRadius", Fretview_peakMaskAnnulusRadius );
	fretviewConfig.putF( "Fretview_peakContentionRadius", Fretview_peakContentionRadius );
	fretviewConfig.putF( "Fretview_peakMassThreshold", Fretview_peakMassThreshold );

	zConfigSaveFile( pluginPath("fretview.cfg"), fretviewConfig );
}

void loadConfig(ZHashTable &fretviewConfig) {
	zConfigLoadFile( pluginPath("fretview.cfg"), fretviewConfig );

	if( fretviewConfig.getS("lastVidLoaded") ) {
		zMsgQueue( "type=Fretview_VidOpen filespec='%s'", fretviewConfig.getS("lastVidLoaded") );
	}
	else if( options.getS( "Fretview_loadFile" ) ) {
		zMsgQueue( "type=Fretview_VidOpen filespec='%s%s'", fretviewDir, options.getS( "Fretview_loadFile" ) );
	}

	if( fretviewConfig.getS("lastCalibLoaded") ) {
		zMsgQueue( "type=Fretview_CalibLoad filespec='%s'", fretviewConfig.getS("lastCalibLoaded") );
	}

	for( int r=0 ; r<REGION_MAX ; ++r )
	{
		region[r].histogramRaw.histScale0 = fretviewConfig.getF( ZTmpStr( "region[%d].histogramRaw.histScale0", r) );
		region[r].histogramRaw.histScale1 = fretviewConfig.getF( ZTmpStr( "region[%d].histogramRaw.histScale1", r) );

		region[r].histogramWork.histScale0 = fretviewConfig.getF( ZTmpStr( "region[%d].histogramWork.histScale0", r) );
		region[r].histogramWork.histScale1 = fretviewConfig.getF( ZTmpStr( "region[%d].histogramWork.histScale1", r) );
	}

	zviewpointTrans.x = fretviewConfig.getF( "zviewpointTrans.x", 0 );
	zviewpointTrans.y = fretviewConfig.getF( "zviewpointTrans.y", 0 );
	zviewpointScale = fretviewConfig.getF( "zviewpointScale", 1 );

	rgnSelectStart.pos.x = fretviewConfig.getF( "rgnSelectStart.pos.x" );
	rgnSelectStop.pos.x = fretviewConfig.getF( "rgnSelectStop.pos.x" );

	Fretview_peakMaskHatRadius = fretviewConfig.getF( "Fretview_peakMaskHatRadius", Fretview_peakMaskHatRadius );
	Fretview_peakMaskBrimRadius = fretviewConfig.getF( "Fretview_peakMaskBrimRadius", Fretview_peakMaskBrimRadius );
	Fretview_peakMaskAnnulusRadius = fretviewConfig.getF( "Fretview_peakMaskAnnulusRadius", Fretview_peakMaskAnnulusRadius );
	Fretview_peakContentionRadius = fretviewConfig.getF( "Fretview_peakContentionRadius", Fretview_peakContentionRadius );
	Fretview_peakMassThreshold = fretviewConfig.getF( "Fretview_peakMassThreshold", Fretview_peakMassThreshold );

	updateCalibMessage();
}

void *loadThreadMain( void *args ) {
	load.filespec = (char *)args;
	load.haltSignal = 0;
	load.status = -1;
	load.status = vidFile->openRead( load.filespec, load.frameStart, load.frameCount, &load.statusMonitor, &load.haltSignal );
	int noFrames = ( vidFile->numFrames <= 0 );
	if( load.haltSignal || noFrames ) {
		assert( load.status == 0 || noFrames );
		strcpy( load.lastLoadedFilename, "" );
		if( vidFile ) {
			delete vidFile;
			vidFile = 0;
		}
		if( noFrames )
			zMsgQueue( "type=ZUISet key=text val='No Frames Found' toZUI=loadStatus" );
	}
	else {
		zMsgQueue( "type=Fretview_Loaded filespec='%s'", escapeQuotes(load.filespec) );
	}
	zMsgQueue( "type=zuiDialogClose die=1 dialogName=LoadProgress" );	// close the dialog box
	free( load.filespec );
	load.filespec = 0;
	return 0;
}

ZMSG_HANDLER( Fretview_CancelLoad ) {
	load.haltSignal = 1;
	while( load.inProgress() ) {
		zTimeSleepMils( 100 );
	}
	zMsgQueue( "type=ZUISet key=text val='Canceled' toZUI=loadStatus" );
}


int safeItoF(double num) {
	if( num < selectRadius )
		num = selectRadius;
	if( num > max(vidFile->w,vidFile->h) - selectRadius )
		num = max(vidFile->w,vidFile->h) - selectRadius;
	return (int)num;
}


ZMSG_HANDLER( Fretview_VidOpen ) {
	if( load.inProgress() ) {
		zuiMessageBox( "File In Progress", "Please wait for the current file to finish loading.", 1 );
		return;
	}

	if( vidFile ) {
		delete vidFile;
		vidFile = 0;
	}

	// OPEN filespec if passed, else creates filepicker to select file
	if( zmsgHas(filespec) ) {
		char *filespec = zmsgS(filespec);

		int error = 0;
		if( strstr( filespec, ".pma" ) ) {
			vidFile = new PMAVidFile;

			if ( zWildcardFileExists( filespec ) ) {
				zuiProgressBox( "LoadProgress", "Loading...", "The pma file is being loaded. Please wait.", "type=Fretview_CancelLoad" );
				zMsgQueue( "type=ZUISet key=text val='The pma file is being loaded. Please wait.' toZUI=LoadProgress" );

				// LAUNCH a thread to open the file as it can take a long time.
				pthread_attr_t attr;
				pthread_attr_init( &attr );
				pthread_attr_setstacksize( &attr, 16*4096 );
				pthread_create( &load.threadID, &attr, &loadThreadMain, strdup(filespec) );
				pthread_attr_destroy( &attr );
			}
			else {
				error = 1;
			}
		}

		if( error ) {
			// FAILED.  Display notice to user & reset UI
			zuiMessageBox( "File Error", "The file you have selected does not exist, can not be opened or is not in a supported format.", 1 );
		}

	}
	else {
		// KD 2012-06-08 was: zuiChooseFile( "Open File", fretviewDir, "type=Fretview_Open", 0, true );
		zuiChooseFile( "Open File", fretviewDir, true, "type=Fretview_VidOpen", 0, true );
	}
}

struct SeqWriter {
	ZBits bits;
	ZTmpStr fileDir;
	ZTmpStr fileStem;
	int frameIndex;
	ZTmpStr dialogName;

	SeqWriter() { frameIndex = -1; }

	void cancel() {
		zMsgQueue( "type=ZUISet key=text val='Canceled' toZUI=fretviewSeqProgress" );
		frameIndex = -1;
	}

	void start() {
		if( frameIndex != -1 )
			cancel();

		bits.createCommon( vidFile->w, vidFile->h, ZBITS_COMMON_LUM_16 );
		bits.owner = 0;

		char dir[1024] = {0,};
		strcpy( dir, fretviewDir );
		if( dir[strlen(dir)-1] == '/' || dir[strlen(dir)-1] == '\\' ) {
			dir[strlen(dir)-1] = 0;
		}
/*
		char time[64] = {0,}, date[64] = {0,};
		_tzset();
		_strtime( time );
		_strdate( date );
		char *c;
		for( c=time; *c; c++ ) {
			if( *c == ':' ) *c = '-';
		}
		for( c=date; *c; c++ ) {
			if( *c == '/' ) *c = '-';
		}


		fileDir.set("%s/%s_%s",dir, date, time);
		fileStem.set("%s_%s", date, time );
*/
		ZFileSpec fileSpec(load.lastLoadedFilename);
		ZTmpStr strippedFileName( fileSpec.getFile(0) );
		fileDir.set( "%s/%s", dir, strippedFileName.s );
		fileStem.set( strippedFileName.s );

		if( !zWildcardFileExists( fileDir.s ) ) {
			#ifdef WIN32
				mkdir( fileDir.s );
			#endif
		}

		{
			FILE *f = fopen( ZTmpStr("%s/metadata.txt",fileDir.s).s, "wt" );
			if( f ) {
				fprintf( f, vidFile->getMetaData() );
				fclose( f );
			}
		}

		frameIndex = 0;

		zuiProgressBox( "SeqProgress", "Writing files...", "The sequence files are being written. Please wait.", "type=Fretview_CancelSeq" );
	}

	void update() {
		if( frameIndex == -1 )
			return;

		if( frameIndex >= vidFile->numFrames ) {
			zMsgQueue( "type=ZUISet key=text val='Wrote %d' toZUI=fretviewSeqProgress", vidFile->numFrames );
			zMsgQueue( "type=zuiDialogClose die=1 dialogName=SeqProgress" );	// close the dialog box
			frameIndex = -1;
			return;
		}

		if( frameIndex >= 0 ) {
			zMsgQueue( "type=ZUISet key=text val='File %d of %d' toZUI=fretviewSeqProgress", frameIndex, vidFile->numFrames );
			zMsgQueue( "type=ZUISet key=text val='File %d of %d' toZUI='SeqProgress/progressBoxText'", frameIndex, vidFile->numFrames );
			zMsgQueue( "type=ZUISet key=layout_forceW val='%d' toZUI='SeqProgress/progressBar'", frameIndex );
			ZTmpStr fileName("%s/%s_%05d.tif", fileDir.s, fileStem.s, frameIndex );
			bits.bits = (char *)vidFile->getFrameU2( frameIndex, 0 );
			int ok = zGraphFileSave( fileName, &bits );
			frameIndex += 1;
		}
	}
};

SeqWriter seqWriter;

ZMSG_HANDLER( Fretview_CancelSeq ) {
	seqWriter.cancel();
}


ZMSG_HANDLER( Fretview_SaveSeqFile ) {
	if( !vidFile )
		return;
	seqWriter.start();
}

// Calibration
//----------------------------------------------------------------------------------------------------------------------------

struct CalibCursor : ZidgetPos2 {
	public: CalibCursor() : ZidgetPos2() { };
};

CalibCursor calibCursor[REGION_MAX];

//ZidgetPos2 &calibLft = (calibCursor[0]);
//ZidgetPos2 &calibRgt = (calibCursor[1]);

struct CalibPts : public ZTLVec<FVec2> {
//	FVec2 get(int index) { return this->
};

CalibPts calibPts;

struct CalibMat : FMat3 {
	CalibMat() : FMat3() { }
};

CalibMat calibMat[REGION_MAX];	// Note that the 0th item is NEVER USED, because zero calibrated to itself is identity.

int calibMatIsLoaded = 0;
int gCalibDirty = 0;
int gCalibValid = 0;

struct CalibSelected {
private:
	int index;

public:
	CalibSelected() { index = -1; }
	void set(int _index)	{ index = _index; }
	boolean isValid()	{ return index >= 0 && index < calibPts.count; }
	int getRegion()		{ assert(index>=0); return index % regionCount; }
	int getRecordOffset(){ assert(index>=0); return index-getRegion(); }
};

CalibSelected calibSelected;


void imageInit( ZidgetImage &image, int wRegion, int h, int index ) {
	static const float spacing = 10.f;

	image.srcBits.createCommon( wRegion, h, ZBITS_COMMON_LUM_FLOAT );
	image.srcBits.fill( 0 );
	image.colorScaleMin = 0.f;
	image.colorScaleMax = 100.f;
	image.pos.x = (wRegion + spacing) * index;
	image.pos.y = 0.f;
}

void selectInit( ZidgetPixelCircle &select, ZidgetImage &image, int regionIndex, boolean disablePos ) {
	select.attach( &image );
	select.radius = selectRadius;
	if( disablePos )
		select.handles[ZidgetPixelCircle_pos].disabled = 1;
	select.handles[ZidgetPixelCircle_radius].disabled = 1;
	select.color = region[regionIndex].getColor();
}

void setRegionCount( int newCount ) {
	if( Fretview_regionCount != newCount )
		Fretview_regionCount = newCount;

	if( regionCount != newCount ) {
		int oldCount = regionCount;
		regionCount = newCount;

		zMsgQueue( "type=%s toZUI='fretviewMapCtr'", regionCount < 3 ? "ZUIDisable" : "ZUIEnable" );

		for( int r=0 ; r<REGION_MAX ; ++r ) {
			calibCursor[r].detach();

			region[r].histogramRaw.detach();
			region[r].histogramWork.detach();
		}

		int wRegion = vidFile->w / regionCount;
		int h = vidFile->h;

		for( int r=0 ; r<REGION_MAX ; ++r ) {
			if( r >= regionCount ) {
				region[r].imageRaw.detach();
				region[r].selectRaw.detach();
				region[r].imageWork.detach();
				region[r].selectWork.detach();
			}
			else {
				region[r].imageRaw.attach();
				region[r].selectRaw.attach();
				//region[r].imageWork.attach();
				//region[r].selectWork.attach();
/*
				imageInit( region[r].imageWork, wRegion, h, r );
				selectInit( region[r].selectWork, region[r].imageWork, r, r!=REGION_LEFT );
				imageInit( region[r].imageRaw, wRegion, h, r );
				selectInit( region[r].selectRaw, region[r].imageRaw, r, r!=REGION_LEFT );
*/
			}
		}

		calibPts.count = 0;

		gLastFrame = -1;	// forces image redraw
		gLastSelect = FVec2(0.f,0.f);	// re-handles selected area
		extern void render();
		render();
	}
}


int calibSolveMatrixForAllRegions() {
	int allInvertable = true;
	for( int r=0 ; r<regionCount ; ++r )
	{
		// Note that region zero is calibrated to itself, and should always be 'identity'
		float lxlx=0.f, lyly=0.f, lxly=0.f, lx=0.f, ly=0.f, lxrx=0.f, lyrx=0.f, rx=0.f, lxry=0.f, lyry=0.f, ry=0.f;
		for( int i=0; i<calibPts.count; i+=regionCount ) {
			lxlx += calibPts[i].x * calibPts[i].x;
			lyly += calibPts[i].y * calibPts[i].y;
			lxly += calibPts[i].x * calibPts[i].y;
			lx   += calibPts[i].x;
			ly   += calibPts[i].y;
			lxrx += calibPts[i].x * calibPts[i+r].x;
			lyrx += calibPts[i].y * calibPts[i+r].x;
			rx   += calibPts[i+r].x;
			lxry += calibPts[i].x * calibPts[i+r].y;
			lyry += calibPts[i].y * calibPts[i+r].y;
			ry   += calibPts[i+r].y;
		}

		FMat3 m;
		m.m[0][0] = lxlx;
		m.m[0][1] = lxly;
		m.m[0][2] = lx;

		m.m[1][0] = lxly;
		m.m[1][1] = lyly;
		m.m[1][2] = ly;

		m.m[2][0] = lx;
		m.m[2][1] = ly;
		m.m[2][2] = (float)calibPts.count / regionCount; //! KD was 2.f changed per zack

		FVec3 b;
		b.x = lxrx;
		b.y = lyrx;
		b.z = rx;

		int result = m.inverse();
		allInvertable = allInvertable && result;

		FVec3 x0 = m.mul( b );

		b.x = lxry;
		b.y = lyry;
		b.z = ry;
		FVec3 x1 = m.mul( b );

		calibMat[r].m[0][0] = x0.x;
		calibMat[r].m[1][0] = x0.y;
		calibMat[r].m[2][0] = x0.z;

		calibMat[r].m[0][1] = x1.x;
		calibMat[r].m[1][1] = x1.y;
		calibMat[r].m[2][1] = x1.z;

		calibMat[r].m[0][2] = 0.f;
		calibMat[r].m[1][2] = 0.f;
		calibMat[r].m[2][2] = 1.f;
	}

	gCalibValid = allInvertable;

	return allInvertable;
}

void setCalibRender( int newState ) {
	if( !vidFile ) {
		newState = 0;
	}
	Fretview_calibRender = newState;
	zMsgQueue( "type=%s toZUI='fretviewCalibView'", newState ? "ZUISelect" : "ZUIDeselect" );
}

void setCalibDirty(int newState) {
	gCalibDirty = newState;

	zMsgQueue( "type=%s toZUI='fretviewCalibSaveAs'", ( calibPts.count > 0 ) ? "ZUIEnable" : "ZUIDisable" );
	zMsgQueue( "type=%s toZUI='fretviewCalibSave'", gCalibDirty ? "ZUIEnable" : "ZUIDisable" );
}

void updateSpotInfo();

void updateCalibMessage() {
	int haveNoPoints = calibPts.count <= 0;
	int haveEnoughPoints = (calibPts.count / regionCount) >= 3;

	char *status =
		haveNoPoints ? "Uncalibrated" : 
		!haveEnoughPoints ? "Need more points" : 
		!gCalibValid ? "Points are invalid" :
		"VALID";

	zMsgQueue( "type=ZUISet key=text val='(%s)' toZUI=fretviewCalibStatus", status );
	updateSpotInfo();
}

void calibLoad( char *filespec ) {
	calibMatIsLoaded = 0;
	FILE *f = fopen( filespec, "rt" );
	if( f ) {
		calibMatIsLoaded = 1;
		calibPts.clear();

		int tempRegionCount = -1;
		fscanf( f, "%d\n", &tempRegionCount );
		
		setRegionCount(tempRegionCount);

		FVec2 pt;
		while( fscanf( f, "%f %f\n", &pt.x, &pt.y ) > 0 ) {
			calibPts.add( pt );
		}
		fclose( f );

		calibSolveMatrixForAllRegions();

		fretviewConfig.putS( "lastCalibLoaded", filespec );
		ZFileSpec spec(filespec);
		zMsgQueue( "type=ZUISet key=text val='\"%s\"' toZUI=fretviewCalibFilename", escapeQuotes(spec.getFile(1)) );
		updateCalibMessage();
		setCalibDirty(1);
		setCalibRender(1);
	}
}

void calibSave( char *filespec ) {
	FILE *f = fopen( filespec, "wt" );
	if( f ) {
		fprintf( f, "%d\n", regionCount );
		for( int i=0; i<calibPts.count; i++ ) {
			fprintf( f, "%f %f\n", calibPts[i].x, calibPts[i].y );
		}
		fclose( f );

		ZFileSpec spec(filespec);
		zMsgQueue( "type=ZUISet key=text val='\"%s\"' toZUI=fretviewCalibFilename", escapeQuotes(spec.getFile(1)) );
		fretviewConfig.putS( "lastCalibLoaded", filespec );

		setCalibDirty(0);
		setCalibRender(1);
	}
}

void getCalibFileSpec( ZTmpStr &str ) {
	char *file = fretviewConfig.getS( "lastCalibLoaded" );
	if( file && *file ) {
		str.set( "%s", file );
	}
	else {
		char *pmaFileSpec = fretviewConfig.getS( "lastVidLoaded" );
		ZFileSpec spec(pmaFileSpec);
		str.set( "%s%s.calib", fretviewDir, spec.getFile(0) );
	}
}

ZMSG_HANDLER( Fretview_CalibSaveAs ) {
	// SAVE filespec if passed, else creates filepicker to select file
	if( zmsgHas(filespec) ) {
		char *filespec = zmsgS(filespec);
		calibSave( filespec );
	}
	else {
		ZTmpStr file;
		getCalibFileSpec( file );

		// KD 2012-06-08 was: zuiChooseFile( "Save Calibration File", fretviewDir, "type=Fretview_CalibSaveAs", 0, false );
		zuiChooseFile( "Save Calibration File", file.s, false, "type=Fretview_CalibSaveAs", 0, false );
	}
}

ZMSG_HANDLER( Fretview_CalibSave ) {
	ZTmpStr file;
	getCalibFileSpec( file );
	if( file.getLen() > 0 ) {
		calibSave( file.s );
	}
	else {
		zuiMessageBox( "Unable to Save", "No file name specified. Please use Save As..." );
	}
}

ZMSG_HANDLER( Fretview_CalibLoad ) {
	// LOAD filespec if passed, else creates filepicker to select file
	if( zmsgHas(filespec) ) {
		char *filespec = zmsgS(filespec);
		calibLoad( filespec );
	}
	else {
		// KD 2012-06-08 was: zuiChooseFile( "Load Calibration File", fretviewDir, "type=Fretview_CalibLoad", 0, false );
		zuiChooseFile( "Load Calibration File", fretviewDir, false, "type=Fretview_CalibLoad", 0, false );
	}
}

void calibTransformLeftToWhichRegionAndBound( FVec2 &lft, FVec2 &rgt, int whichRegion, int w, int h, int r ) {
	lft.x = max( r, min( w-r, lft.x ) );
	lft.y = max( r, min( h-r, lft.y ) );

	FVec3 lft3( lft.x, lft.y + vidFile->getInfoI("y"), 1.f );
	FVec3 rgt3 = calibMat[whichRegion].mul( lft3 );

	rgt3.y -= vidFile->getInfoI("y");
	rgt3.x = max( r, min( w-r, rgt3.x ) );
	rgt3.y = max( r, min( h-r, rgt3.y ) );

	rgt.x = rgt3.x;
	rgt.y = rgt3.y;
}

void calibTransformWhichRegionToLeftAndBound( FVec2 &rgt, FVec2 &lft, int whichRegion, int w, int h, int r ) {
	rgt.x = max( r, min( w-r, rgt.x ) );
	rgt.y = max( r, min( h-r, rgt.y ) );

	FVec3 rgt3( rgt.x, rgt.y + vidFile->getInfoI("y"), 1.f );
	FMat3 calibInverse = calibMat[whichRegion];
	calibInverse.inverse();
	FVec3 lft3 = calibInverse.mul( rgt3 );

	lft3.y -= vidFile->getInfoI("y");
	lft3.x = max( r, min( w-r, lft3.x ) );
	lft3.y = max( r, min( h-r, lft3.y ) );

	lft.x = lft3.x;
	lft.y = lft3.y;
}

void calibTransformLeftToWhichRegion( FVec2 &lft, FVec2 &rgt, int whichRegion ) {
	FVec3 lft3( lft.x, lft.y + vidFile->getInfoI("y"), 1.f );
	FVec3 rgt3 = calibMat[whichRegion].mul( lft3 );
	rgt3.y -= vidFile->getInfoI("y");
	rgt.x = rgt3.x;
	rgt.y = rgt3.y;
}

void calibTransformWhichRegionToLeft( FVec2 &rgt, FVec2 &lft, int whichRegion ) {
	FVec3 rgt3( rgt.x, rgt.y + vidFile->getInfoI("y"), 1.f );
	FMat3 calibMatInverse = calibMat[whichRegion];
	calibMatInverse.inverse();
	FVec3 lft3 = calibMatInverse.mul( rgt3 );
	lft3.y -= vidFile->getInfoI("y");
	lft.x = lft3.x;
	lft.y = lft3.y;
}

ZMSG_HANDLER( Fretview_CalibView ) {
	if( zmsgI( state ) ) {
		setCalibRender(1);
	}
	else {
		setCalibRender(0);
	}
}

ZMSG_HANDLER( Fretview_CalibAdd ) {
	if( !vidFile )
		return;
	// This adds a new spot 10 to the right of the last spot.  it works like
	// copy/paste of a graphic so you don't see them overlap each other
	for( int r=0 ; r<regionCount ; ++r ) {
		calibCursor[r].pos.x += 10.f;
		calibPts.add( calibCursor[r].pos );
	}

	calibSelected.set(calibPts.count - regionCount); //! 2;

	updateCalibMessage();

	setCalibRender(1);
	setCalibDirty(1);
}

ZMSG_HANDLER( Fretview_CalibClear ) {
	calibPts.clear();
	updateCalibMessage();
	setCalibRender(1);
	setCalibDirty(0);
}


void calibRender() {
	// KD checking just to make sure the # of regions hasn't changed.  If it has, you need to
	// clear all the calibPts and start over.
	for( int r=0 ; r<REGION_MAX ; ++r ) {
		calibCursor[r].detach();
	}
	//!	calibLft.detach();
	//!	calibRgt.detach();

	if( Fretview_calibRender ) { 
		for( int i=0 ; i<regionCount ; ++i ) {
			if( gViewingWorkImage ) {
				calibCursor[i].attach( &region[i].imageWork );
				//calibLft.attach( &imageWorkLft );
				//calibRgt.attach( &imageWorkRgt );
			}
			else {
				calibCursor[i].attach( &region[i].imageRaw );
				//calibLft.attach( &imageRawLft );
				//calibRgt.attach( &imageRawRgt );
			}
		}

//		glBegin( GL_LINES );
//			FVec2 l = calibLft.localToWorld.mul( FVec3(calibLft.pos.x, calibLft.pos.y, 0.f) );
//			FVec2 r = calibRgt.localToWorld.mul( FVec3(calibRgt.pos.x, calibRgt.pos.y, 0.f) );
//			glVertex2fv( l );
//			glVertex2fv( r );
//		glEnd();

		if( calibSelected.isValid() ) {
			for( int r=0 ; r<regionCount ; ++r ) {
				calibPts[calibSelected.getRecordOffset()+r] = calibCursor[r].pos;
				//! calibPts[calibSelectedRecord+0] = calibLft.pos;
				//! calibPts[calibSelectedRecord+1] = calibRgt.pos;
			}
		}

		glColor3ub( 100, 150, 255 );
		glBegin( GL_LINES );
		for( int i=0; i<calibPts.count; i+=regionCount ) {
			for( int r=0 ; r<regionCount ; ++r ) {
				const float ptSIZE = 3.f;
				glVertex2f( calibPts[i+r].x + region[r].imageRaw.pos.x - ptSIZE, calibPts[i+r].y + region[r].imageRaw.pos.y - ptSIZE );
				glVertex2f( calibPts[i+r].x + region[r].imageRaw.pos.x + ptSIZE, calibPts[i+r].y + region[r].imageRaw.pos.y + ptSIZE );
				glVertex2f( calibPts[i+r].x + region[r].imageRaw.pos.x - ptSIZE, calibPts[i+r].y + region[r].imageRaw.pos.y + ptSIZE );
				glVertex2f( calibPts[i+r].x + region[r].imageRaw.pos.x + ptSIZE, calibPts[i+r].y + region[r].imageRaw.pos.y - ptSIZE );

//!				glVertex2f( calibPts[i+0].x + imageRawLft.pos.x - 3.f, calibPts[i+0].y + imageRawLft.pos.y - 3.f );
//!				glVertex2f( calibPts[i+0].x + imageRawLft.pos.x + 3.f, calibPts[i+0].y + imageRawLft.pos.y + 3.f );
//!				glVertex2f( calibPts[i+0].x + imageRawLft.pos.x - 3.f, calibPts[i+0].y + imageRawLft.pos.y + 3.f );
//!				glVertex2f( calibPts[i+0].x + imageRawLft.pos.x + 3.f, calibPts[i+0].y + imageRawLft.pos.y - 3.f );

//!				glVertex2f( calibPts[i+1].x + imageRawRgt.pos.x - 3.f, calibPts[i+1].y + imageRawRgt.pos.y - 3.f );
//!				glVertex2f( calibPts[i+1].x + imageRawRgt.pos.x + 3.f, calibPts[i+1].y + imageRawRgt.pos.y + 3.f );
//!				glVertex2f( calibPts[i+1].x + imageRawRgt.pos.x - 3.f, calibPts[i+1].y + imageRawRgt.pos.y + 3.f );
//!				glVertex2f( calibPts[i+1].x + imageRawRgt.pos.x + 3.f, calibPts[i+1].y + imageRawRgt.pos.y - 3.f );
			}
		}
		glEnd();

		calibSolveMatrixForAllRegions();
	}
}

void calibSelect( FVec2 mouse ) {
	float closestMag = 100000.f;
	CalibSelected closest;

	const int humanMustClickWithinThisManyPixels = 5;
	// MOVE the calib selector to the nearest
	for( int i=0; i<calibPts.count; i+=regionCount ) {
		FVec2 dist = mouse;
		for( int c=0 ; c<regionCount ; ++c ) {
			dist.sub( calibPts[i+c] );
			if( dist.mag() <= humanMustClickWithinThisManyPixels*humanMustClickWithinThisManyPixels && dist.mag() <= closestMag ) {
				closestMag = dist.mag();
				closest.set(i);
			}
		}
	}

	if( closest.isValid() ) {	// eg did we find one at all?
			for( int r=0 ; r<regionCount ; ++r )
			{
				calibCursor[r].pos = calibPts[closest.getRecordOffset()+r];
			}
			calibSelected = closest;
/*
//!
		if( !(closestI & 1) ) {
			// Left
			calibLft.pos = calibPts[closestI+0];
			calibRgt.pos = calibPts[closestI+1];
			calibSelectedRecord = closestI;
		}
		else {
			// Right
			calibLft.pos = calibPts[closestI-1];
			calibRgt.pos = calibPts[closestI+0];
			calibSelectedRecord = closestI-1;
		}
*/
	}
}

// Peaks
//----------------------------------------------------------------------------------------------------------------------------
#define PIXELS_PER_PEAK (1024)
struct PixelList {
	int count;
	IVec2 pixel[PIXELS_PER_PEAK];
};

struct PeakInfo {
	float mean;
	float median;
	float stddev;
	int count;
};

struct PeakTracker {
	PixelList hat;
	PixelList ann;

	PeakInfo hatInfo;
	PeakInfo annInfo;
};

struct Peak {
	int groupCount;
	float x;
	float y;
	int contention;
		// 0 = no contention
		// 1 = too close on same side
		// 2 = You're on the right side near where an exepcted mapping from left falls
	int whichRegion;
		// 0 = lft, 1 through n are the others

	PeakTracker peak[REGION_MAX];
		// KD: This appears to be a cache used to track and persist detected peaks

	void reset() {
		memset( this, 0, sizeof(*this) );
	}

	Peak() {
		reset();
	}
};

ZTLVec<Peak> peaks;

void peaksPixelList( ZBits *bits, FVec2 pos, float inner, float outer, IVec2 *pixels, int &pixelsCount, int binSize ) {
	int w = bits->w();
	int h = bits->h();
	
	// pos is in the full coord system but we want to do everything in bin coor system
	// Step 1: convert everything to bin system
	// Step 2: Define the circle in bin system (but we have to test bounds in full so locally convert back)
	// Step 3: Convert back to full system and sample the inner parts
	
	pos.x /= (float)binSize;
	pos.y /= (float)binSize;
	inner /= (float)binSize;
	outer /= (float)binSize;
	
	int px = (int)safeItoF( pos.x + 0.5f );
	int py = (int)safeItoF( pos.y + 0.5f );
	
	int o = (int)(outer+1);
	
	for( int y=-o; y<=o; y++ ) {
		for( int x=-o; x<=o; x++ ) {
		
			float dx = (float)x + 0.5f;
			float dy = (float)y + 0.5f;
			if( (dx*dx + dy*dy < outer*outer && dx*dx + dy*dy >= inner*inner) || (x==0 && y==0 && inner == 0.f) ) {
				int sx = (px + x)*binSize;
				int sy = (py + y)*binSize;
				for( int biny=0; biny<binSize; biny++ ) {
					for( int binx=0; binx<binSize; binx++ ) {
						if( sx+binx >= 0 && sx+binx < w && sy+biny >= 0 && sy+biny < h ) {
							IVec2 s( sx+binx, sy+biny );
							pixels[pixelsCount++] = s;
							assert( pixelsCount < PIXELS_PER_PEAK );
						}
					}
				}
			}
		}
	}
}

void peakMake(Peak &_peak, FVec2 pos, int regionIndex ) {
	Peak peak = _peak;
	peak.x = pos.x;
	peak.y = pos.y;
	peak.whichRegion = regionIndex;
	peaks.add( peak );
}

void peaksMapSideToSide( int sourceRegion ) {
	// sourceRegion
	// 0 = map left to all other regions
	// n = map sourceRegion to left ONLY
	//! KD: note that 'union' was not being used, and is thus no longer supported

	int i;

	// REMOVE all other sides
	for( i=0; i<peaks.count; i++ ) {
		if( sourceRegion != peaks[i].whichRegion ) {
			peaks.del( i );
			i--;
		}
	}

	// MAP 
	int origCount = peaks.count;
	for( i=0; i<origCount; i++ ) {
		FVec2 lft;
		if( sourceRegion != REGION_LEFT ) {
			// MAP sourceRegion to left
			calibTransformWhichRegionToLeft( FVec2( peaks[i].x, peaks[i].y ), lft, sourceRegion );
			peakMake( peaks[i], lft, REGION_LEFT );
		}

		for( int r=0 ; r<regionCount ; ++r ) {
			if( r != REGION_LEFT && r != sourceRegion ) {
				// MAP left to all other regions
				FVec2 pos;
				calibTransformLeftToWhichRegion( FVec2( peaks[i].x, peaks[i].y ), pos, r );
				peakMake( peaks[i], pos, r );
			}
		}
	}
}

void peaksSetupPixelLists( int binSize ) {
	// fill region[n].imageWork as needed

	for( int i=0; i<peaks.count; i++ ) {
		// KD: WARNING! this only sets up the region in which this peak exists, rather than ALL the regions,
		// yet there appear to be other regions getting processed at other times.  Be careful!

		//int r = peaks[i].whichRegion;
		for( int r=0 ; r<regionCount ; ++r ) {
			// The only reason this works is that peaks[i].(x,y) is in absolute coordinates, so we don't have
			// to know which region we're in.  It will setup ALL regions based on THIS peak's region.  Which is wonky.
			peaks[i].peak[r].hat.count = 0;
			peaksPixelList( &region[r].imageWork.srcBits, FVec2(peaks[i].x,peaks[i].y), 0.f, Fretview_peakMaskHatRadius, peaks[i].peak[r].hat.pixel, peaks[i].peak[r].hat.count, binSize );
			peaks[i].peak[r].ann.count = 0;
			peaksPixelList( &region[r].imageWork.srcBits, FVec2(peaks[i].x,peaks[i].y), Fretview_peakMaskHatRadius + Fretview_peakMaskBrimRadius, Fretview_peakMaskHatRadius + Fretview_peakMaskBrimRadius + Fretview_peakMaskAnnulusRadius, peaks[i].peak[r].ann.pixel, peaks[i].peak[r].ann.count, binSize );
		}
	}
}

Peak peaksAnalyzeAtPos( FVec2 p[REGION_MAX], int frame, int binSize ) {
	Peak peak;

	peak.x = p[REGION_LEFT].x;
	peak.y = p[REGION_LEFT].y;
	peak.whichRegion = REGION_LEFT;

	for( int r=0 ; r<regionCount ; ++r ) {
		float mean = -1.f, median = -1.f, stddev = -1.f;
		int i;
		ZBits *bits = &region[r].imageRaw.srcBits;

		FVec2 pt( (int)safeItoF(p[r].x+0.5f), (int)safeItoF(p[r].y+0.5f) );

		// hat
		peaksPixelList( bits, pt, 0.f, Fretview_peakMaskHatRadius, peak.peak[r].hat.pixel, peak.peak[r].hat.count, binSize );
		ZTLVec<float> pixelValsHat;
		for( i=0 ; i<peak.peak[r].hat.count ; i++ ) {
			pixelValsHat.add( bits->getFloat( peak.peak[r].hat.pixel[i].x, peak.peak[r].hat.pixel[i].y ) );
		}
		zmathStatsF( pixelValsHat, 0, pixelValsHat.count, &mean, &median, &stddev );
		peak.peak[r].hatInfo.mean = mean;
		peak.peak[r].hatInfo.median = median;
		peak.peak[r].hatInfo.stddev = stddev;
		peak.peak[r].hatInfo.count = pixelValsHat.count;

		// annulus
		peaksPixelList( bits, pt, Fretview_peakMaskHatRadius + Fretview_peakMaskBrimRadius, Fretview_peakMaskHatRadius + Fretview_peakMaskBrimRadius + Fretview_peakMaskAnnulusRadius, peak.peak[r].ann.pixel, peak.peak[r].ann.count, binSize );
		ZTLVec<float> pixelValsAnn;	
		for( i=0; i<peak.peak[r].ann.count; i++ ) {
			pixelValsAnn.add( bits->getFloat( peak.peak[r].ann.pixel[i].x, peak.peak[r].ann.pixel[i].y ) );
		}
		zmathStatsF( pixelValsAnn, 0, pixelValsAnn.count, &mean, &median, &stddev );
		peak.peak[r].annInfo.mean = mean;
		peak.peak[r].annInfo.median = median;
		peak.peak[r].annInfo.stddev = stddev;
		peak.peak[r].annInfo.count = pixelValsAnn.count;
	}

	return peak;
}

struct PeakAnalysis {
	FILE *file;
	int frameIndex;
	int startFrame;
	int stopFrame;

	PeakAnalysis() { file = 0; frameIndex = -1; startFrame = -1; stopFrame = -1; }

	void cancel() {
		if( file ) {
			zMsgQueue( "type=ZUISet key=text val='Canceled' toZUI=fretviewAnalysisInfo" );
			fclose( file );
			file = 0;
			frameIndex = -1;
		}
	}

	void start() {
		if( file )
			cancel();

		zuiProgressBox( "AnalysisProgress", "Writing analysis...", "The analysis is being written. Please wait.", "type=Fretview_CancelAnalysis" );

		int binSize = vidFile->getInfoI("bin");
		startFrame = (int)( rgnSelectStart.pos.x * (float)vidFile->numFrames );
		stopFrame  = (int)( rgnSelectStop .pos.x * (float)vidFile->numFrames );
		startFrame = max( 0, min( vidFile->numFrames-1, startFrame ) );
		stopFrame = max( startFrame+1, min( vidFile->numFrames-1, stopFrame ) );
		if( Fretview_dumpAllTime ) {
			startFrame = 0;
			stopFrame = vidFile->numFrames-1;
		}

		// OPEN the file same as the lodaded file with .fretview
		ZTmpStr filename( "%s.fretview", load.lastLoadedFilename );

		file = fopen( filename, "wt" );
		assert( file );
		
		// PRINT the header
		fprintf( file, "FILENAME= %s\n", load.lastLoadedFilename );
		fprintf( file, "PEAK_MASK_BRIM_RADIUS= %d\n", Fretview_peakMaskBrimRadius );
		fprintf( file, "PEAK_MASK_HAT_RADIUS= %d\n", Fretview_peakMaskHatRadius );
		fprintf( file, "PEAK_MASK_ANNULUS_RADIUS= %d\n", Fretview_peakMaskAnnulusRadius );

		fprintf( file, "REGION_COUNT= %d\n", regionCount );
		for( int r=0 ; r<regionCount ; ++r ) {
			fprintf( file, "REGION%d_PEAK_CUTOFF= %f\n", r, region[r].histogramWork.histScale0 );
		}

		fprintf( file, "ROIX= %d\n", vidFile->getInfoI( "x" ) );
		fprintf( file, "ROIY= %d\n", vidFile->getInfoI( "y" ) );
		fprintf( file, "ROIW= %d\n", vidFile->w );
		fprintf( file, "ROIH= %d\n", vidFile->h );

		for( int r=0 ; r<regionCount ; ++r ) {
			fprintf( file, "REGION%d_MAT_0_0= %f\n", r, calibMat[r].m[0][0] );
			fprintf( file, "REGION%d_MAT_1_0= %f\n", r, calibMat[r].m[1][0] );
			fprintf( file, "REGION%d_MAT_2_0= %f\n", r, calibMat[r].m[2][0] );
			fprintf( file, "REGION%d_MAT_0_1= %f\n", r, calibMat[r].m[0][1] );
			fprintf( file, "REGION%d_MAT_1_1= %f\n", r, calibMat[r].m[1][1] );
			fprintf( file, "REGION%d_MAT_2_1= %f\n", r, calibMat[r].m[2][1] );
		}

		// PRINT information about each spot
		int i;
		for( i=0; i<peaks.count; i++ ) {
			if( ! peaks[i].contention ) {
				// First transform to the left (unless this IS the left) and then transform from left to all others
				FVec2 pos[REGION_MAX];
				Peak &pk = peaks[i];

				if( pk.whichRegion != REGION_LEFT ) {
					pos[pk.whichRegion] = FVec2( pk.x, pk.y );
					calibTransformWhichRegionToLeft( pos[pk.whichRegion], pos[REGION_LEFT], pk.whichRegion );
				}
				else {
					pos[REGION_LEFT] = FVec2( pk.x, pk.y );
				}
				
				for( int r=0 ; r<regionCount ; ++r ) {
					if( r != REGION_LEFT && r != pk.whichRegion ) {
						calibTransformLeftToWhichRegion( pos[REGION_LEFT], pos[r], r );
					}
				}

				ZTmpStr result( "peak=%d region=%d", i, pk.whichRegion );
				for( int r=0 ; r<regionCount ; ++r ) {
					result.append( " r%dx=%d r%dy=%d", r, (int)(pos[r].x+0.5f), r, (int)(pos[r].y+0.5f) );
				}
				result.append( "\n" );

				fprintf( file, result );
			}
		}

		// PRINT header
		ZTmpStr header( "HEADER: peakNum" );
		for( int r=0 ; r<regionCount ; ++r ) {
			header.append( " cenCount%d cenMean%d cenMedian%d cenStdev%d bgCount%d bgMean%d bgMedian%d bgStdev%d", r, r, r, r, r, r, r, r );
		}
		header.append( "\n" );
		fprintf( file, header );

		frameIndex = startFrame;
	}

	void update() {
		if( !file )
			return;

		if( frameIndex > stopFrame ) {
			char *timeString = zTimeGetLocalTimeString();
			zMsgQueue( "type=ZUISet key=text val='At %s: Analysis dumped to %s.fretview' toZUI=fretviewAnalysisInfo", timeString, load.lastLoadedFilename );
			zMsgQueue( "type=zuiDialogClose die=1 dialogName=AnalysisProgress" );	// close the dialog box
			fclose( file );
			file = 0;
			frameIndex = -1;
			return;
		}

		zMsgQueue( "type=ZUISet key=text val='Writing %d of %d' toZUI=fretviewAnalysisInfo", frameIndex-startFrame, stopFrame-startFrame );
		zMsgQueue( "type=ZUISet key=text val='Writing frame %d of %d' toZUI='AnalysisProgress/progressBoxText'", frameIndex-startFrame, stopFrame-startFrame );

		const int framesToProcessPerUpdate = 5;
		int limitFrame = min(frameIndex+framesToProcessPerUpdate,stopFrame+1);
		int binSize = vidFile->getInfoI("bin");

		while( frameIndex >= startFrame && frameIndex < limitFrame ) {
			ZBits lftBits( &region[REGION_LEFT].imageRaw.srcBits );

			// Take the raw data from the movie and load it into the l & r images
			int x = 0, y = 0;
			int h = lftBits.h();
			int w2 = lftBits.w();
			for( y=0; y<h; y++ ) {
				unsigned short *s = 0;
				float *d = 0;

				for( int r=0 ; r<regionCount ; ++r ) {
					s = &(vidFile->getFrameU2(frameIndex,y)[w2*r]);
					d = region[r].imageRaw.srcBits.lineFloat(y);
					for( x=0; x<w2; x++ ) {
						*d++ = (float)*s++;
					}
				}
			}

			fprintf( file, "frame=%d time=%u\n", frameIndex, vidFile->getFrameTime(frameIndex) );
			for( int i=0; i<peaks.count; i++ ) {
				if( ! peaks[i].contention ) {

					Peak &pk = peaks[i];
					FVec2 pos[REGION_MAX];
					for( int r=0 ; r<REGION_MAX ; r++ ) {
						pos[r] = FVec2();
					}

					ZTmpStr result( "* %03d", i );
					for( int r=0 ; r<regionCount ; ++r ) {
						if( pk.whichRegion != REGION_LEFT ) {
							pos[r] = FVec2( pk.x, pk.y );
							calibTransformWhichRegionToLeft( pos[r], pos[REGION_LEFT], pk.whichRegion );
						}
						else {
							pos[REGION_LEFT] = FVec2( pk.x, pk.y );
						}

						if( r != REGION_LEFT && r != pk.whichRegion ) {
							calibTransformLeftToWhichRegion( pos[REGION_LEFT], pos[r], r );
						}
					}

					Peak analysis = peaksAnalyzeAtPos( pos, frameIndex, binSize );

					for( int r=0 ; r<regionCount ; ++r ) {
						const PeakInfo &h = analysis.peak[r].hatInfo;
						const PeakInfo &a = analysis.peak[r].annInfo;
						result.append( " %d %4.1f %4.1f %4.1f %d %4.1f %4.1f %4.1f", h.count, h.mean, h.median, h.stddev, a.count, a.mean, a.median, a.stddev );
					}
					result.append( "\n" );

					fprintf( file, result );
				}
			}
			fprintf( file, "\n" );
			frameIndex += 1;
		}
	}
};

PeakAnalysis peakAnalysis;

ZMSG_HANDLER( Fretview_CancelAnalysis ) {
	peakAnalysis.cancel();
}


void peaksOptimize() {
	int binSize = vidFile->getInfoI("bin");
	for( int i=0; i<peaks.count; i++ ) {
		// SEARCH around looking for best fit
		ZBits *bits = &region[peaks[i].whichRegion].imageWork.srcBits; //peaks[i].whichRegion == 0 ? &imageWorkLft.srcBits : &imageWorkRgt.srcBits;
		assert(bits);
		int w = bits->w();
		int h = bits->h();
		int hat = (int)( Fretview_peakMaskHatRadius );
		int hat2 = hat*hat;
		float bestAvg = -1.f;	// KD: used to be zero, but the result COULD have been zero, this never setting bestCen
		IVec2 bestCen( int(peaks[i].x+0.5f), int(peaks[i].y+0.5f) );
		for( int dy=-hat; dy<=hat; dy++ ) {
			for( int dx=-hat; dx<=hat; dx++ ) {
				IVec2 tryPos( int(peaks[i].x+0.5f) + dx, int(peaks[i].y+0.5f) + dy );
				IVec2 pixels[PIXELS_PER_PEAK];
				int pixelsCount = 0;
				
				// KD: Might be useful at this point to assert() that our (x,y) is within region peaks[i].whichRegion.
				peaksPixelList( bits, FVec2((int)tryPos.x,(int)tryPos.y), 0.f, Fretview_peakMaskHatRadius, pixels, pixelsCount, binSize );

				// COMPUTE sum inside hat at this dx,dy offset
				float sum = 0.f;
				for( int j=0; j<pixelsCount; j++ ) {
					// ACCUMULATE the spot (center)
					sum += bits->getFloat(pixels[j].x,pixels[j].y);
				}
				
				if( pixelsCount > 0 ) {
					float avg = sum / (float)pixelsCount;
					if( avg > bestAvg ) {
						bestAvg = avg;
						bestCen = tryPos;
					}
				}
			}
		}
		
		peaks[i].x = (float)bestCen.x;
		peaks[i].y = (float)bestCen.y;
	}
}

// Image processing
//----------------------------------------------------------------------------------------------------------------------------

void histogramImageSet( ZidgetHistogram &hist, ZidgetImage &image ) {
	int x, y;
	const int histDataBufferSize = 4096;
	float histogramData[histDataBufferSize] = { 0, };	// KD: DANGER: what does 4096 mean?  What if the # gets too big?
	int w = image.srcBits.w();
	int h = image.srcBits.h();

	for( y=0; y<h; y++ ) {
		float *srcL = image.srcBits.lineFloat( y );
		for( x=0; x<w; x++ ) {
			int histBucket = max( 0, min( histDataBufferSize-1, (int)*srcL ) );
			histogramData[histBucket]++;
			srcL++;
		}
	}
	hist.setHist( histogramData, histDataBufferSize );
}

void histogramSet( ZidgetHistogram &histogram, ZidgetImage &image, int index, int disabled ) {
	int wRegion = vidFile->w / regionCount;

	image.dirtyBits();
	histogram.attach( &image );
	histogram.pos.x = 0.0f; // Because it is attached to the image above it...
	histogram.pos.y = -30.f;
	histogram.logScale = 1;
	histogram.handles[ZidgetHistogram_adjustMax].disabled = disabled;
}


void histogramSetThreshold(Region &reg) {
	int i;
	float totalMass = 0.f;
	for( i=0; i<reg.histogramWork.histogramCount; i++ ) {
		totalMass += reg.histogramWork.histogram[i];
	}
	float mass = 0.f;
	for( i=0; i<reg.histogramWork.histogramCount; i++ ) {
		mass += reg.histogramWork.histogram[i];
		if( mass > (1.f-Fretview_peakMassThreshold)*totalMass ) {
			break;
		}
	}
	reg.histogramWork.histScale0 = (float)i;
}



#ifdef AUTO_CORR
// KD: For now, I'm just going to leave this as-is, processing only TWO regions.
// This fills the global variable 'autoCorr' with values.
void imagesAutocorrelation() {
	int x, y;
	int w = vidFile->w;
	int wRegion = w / regionCount;
	int h = vidFile->h;
	int r = (int)region[REGION_LEFT].selectRaw.radius;
	Region &rLeft = region[REGION_LEFT];

	int startFrame = (int)( rgnSelectStart.pos.x * (float)vidFile->numFrames );
	int stopFrame  = (int)( rgnSelectStop .pos.x * (float)vidFile->numFrames );
	startFrame = max( 0, min( vidFile->numFrames-2, startFrame ) );
	stopFrame = max( startFrame+1, min( vidFile->numFrames-2, stopFrame ) );
	int numFrames = stopFrame - startFrame + 1;

	imageInit( rLeft.imageWork, wRegion, h, REGION_LEFT );

	selectInit( rLeft.selectWork, rLeft.imageWork, REGION_LEFT, false );

	// For each pixel
	// For each lag
	
	const int corrBytes = wRegion * h * NUM_LAGS * sizeof(double);
	if( gAutoCorr )
		free( gAutoCorr );
	gAutoCorr = (double *)malloc( corrBytes );
	memset( gAutoCorr, 0, corrBytes );

	// ALLOCATE a single pixel's trace buffer
	double *timeTrace = (double *)alloca( numFrames * sizeof(double) );

	//KD: never used - double brightest = 0.0;
	for( y=0; y<h; y++ ) {
		printf( "%f%%\n", 100.f * (float)y / (float)h );
		for( x=0; x<wRegion; x++ ) {
		
			// EXTRACT out the pixel trace at this location lft and rgt channel, compute fret
			FVec2 lft, rgt;
			lft = FVec2( x, y );
			calibTransformLeftToWhichRegion( lft, rgt, REGION_SECOND );

			if( rgt.x >= 0 && rgt.x < wRegion && rgt.y >= 0 && rgt.y < h ) {

				double *dst = timeTrace;
				double lftSum = 0.0;
				double rgtSum = 0.0;
				for( int f=startFrame; f<=stopFrame; f++ ) {
					unsigned short *fLft = &vidFile->getFrameU2(f,(int)lft.y)[(int)lft.x];
					unsigned short *fRgt = &vidFile->getFrameU2(f,(int)rgt.y)[(int)rgt.x + wRegion];
					double lft = (double)*fLft;
					double rgt = (double)*fRgt;
					*dst++ = rgt / ( rgt + lft );
					lftSum += lft;
					rgtSum += rgt;
				}
				
				// PERFORM autocorrelation on this pixel's time trace
				double autoCorrSum = 0.0;
				for( int lag=0; lag<NUM_LAGS; lag++ ) {
					double *a = timeTrace;
					double *b = &timeTrace[lag];
					double accum = 0.0;
					int count = numFrames-lag;
					for( int t=0; t<count; t++ ) {
						accum += (*a) * (*b);
						a++;
						b++;
					}

					int autoCorrPixelOffset = (wRegion*y + x) * NUM_LAGS;
					double ac = accum / (double)count;
					gAutoCorr[ autoCorrPixelOffset + lag ] = ac;
					autoCorrSum += ac;
				}
				
				float *dstL = &rLeft.imageWork.srcBits.lineFloat( y )[ x ];
				*dstL = (float)autoCorrSum;
			}
		}
	}

	histogramSet( rLeft.histogramWork, rLeft.imageWork, REGION_LEFT, 0 );
	histogramImageSet( rLeft.histogramWork, rLeft.imageWork );
	histogramSetThreshold( rLeft );
}
#endif

void imagesFilter() {
	if( !vidFile )
		return;

	int x, y;
	int w = vidFile->w;
	int wRegion = w / regionCount;
	int h = vidFile->h;
	int radius = (int)region[REGION_LEFT].selectRaw.radius;

	int startFrame = (int)( rgnSelectStart.pos.x * (float)vidFile->numFrames );
	int stopFrame  = (int)( rgnSelectStop .pos.x * (float)vidFile->numFrames );
	startFrame = max( 0, min( vidFile->numFrames-2, startFrame ) );
	stopFrame = max( startFrame+1, min( vidFile->numFrames-2, stopFrame ) );

	for( int r=0 ; r<regionCount ; ++r ) {
		imageInit( region[r].imageWork, wRegion, h, r );
		selectInit( region[r].selectWork, region[r].imageWork, r, r!=REGION_LEFT );
	}

	// FILTER
	// Use an int map instead of float to make sure we preserve precision
	int *accum = new int[w * h];
	memset( accum, 0, sizeof(int)*w*h );

	int peakDiameter = radius * 2;
	int shiftRadius = peakDiameter * 2;

	for( int f=startFrame; f<=stopFrame; f++ ) {
		int shiftX = (int)( (float)shiftRadius * cos64F[ f % 64 ] );
		int shiftY = (int)( (float)shiftRadius * sin64F[ f % 64 ] );

		for( y=shiftRadius; y<h-shiftRadius; y++ ) {
			int *acc = &accum[y*w+shiftRadius];
			unsigned short *f0 = &vidFile->getFrameU2( f  , y        )[ shiftRadius        ];
			unsigned short *f1 = &vidFile->getFrameU2( f+1, y+shiftY )[ shiftRadius+shiftX ];

			for( x=0; x<w-2*shiftRadius; x++ ) {
				*acc += *f0 - *f1;
				f0++;
				f1++;
				acc++;
			}
		}
	}

	// CONVERT the int map to the float map in the work image
	int frameCount = stopFrame - startFrame;
	float frameCountF = (float)frameCount;
	for( y=0; y<h; y++ ) {
		for( int r=0 ; r<regionCount ; ++r ) {
			int *acc = &accum[y*w];
			acc += wRegion * r;
			float *dst = region[r].imageWork.srcBits.lineFloat( y );
			for( x=0; x<wRegion; x++ ) {
				*dst = max( 0.f, (float)*acc / frameCountF );
				dst++;
				acc++;
			}
		}
	}

	for( int r=0 ; r<REGION_MAX ; ++r ) {
		region[r].histogramWork.detach();
	}

	for( int r=0 ; r<regionCount ; ++r ) {
		histogramSet( region[r].histogramWork, region[r].imageWork, r, true );
		histogramImageSet( region[r].histogramWork, region[r].imageWork );
		histogramSetThreshold( region[r] );
	}

	delete accum;

	zMsgQueue( "type=ZUIEnable toZUI='fretviewPeakOpt'" );

	gPeaksVisible = 1;
}

void imagesPeakFind() {
	if( ! vidFile ) {
		return;
	}

	int x, y;
	int w = vidFile->w;
	int wRegion = w / regionCount;
	int h = vidFile->h;

	if( wRegion != region[REGION_LEFT].imageWork.srcBits.w() ) {
		assert(!"wRegion mismatches work image width");
		return;
	}

	peaks.clear();

	int startFrame = (int)( rgnSelectStart.pos.x * (float)vidFile->numFrames );
	int stopFrame  = (int)( rgnSelectStop .pos.x * (float)vidFile->numFrames );
	startFrame = max( 0, min( vidFile->numFrames-2, startFrame ) );
	stopFrame = max( startFrame+1, min( vidFile->numFrames-2, stopFrame ) );

	int stackTop = 0;
	int stackSize = w * h;
	int *stack = (int *)malloc( sizeof(int) * stackSize );

	#define PUSH(a,b) stack[stackTop] = ((a & 0xFFFF)<<16) | (b & 0xFFFF); stackTop++;
	#define POP(a,b) stackTop--; a=(stack[stackTop]&0xFFFF0000)>>16; b=(stack[stackTop]&0xFFFF); 

	// FLOODFILL
	for( int side=0; side<regionCount; side++ ) {

		ZBits *srcBits = &region[side].imageWork.srcBits;
		float threshold = region[side].histogramWork.histScale0;

		ZBits dstBits;
		dstBits.createCommon( srcBits->w(), srcBits->h(), ZBITS_COMMON_LUM_8 );
		dstBits.fill( 0 );

		int pCount = 1;

		for( y=0; y<h; y++ ) {
			float *src = srcBits->lineFloat( y );
			unsigned char *dst = dstBits.lineU1( y );
			for( x=0; x<wRegion; x++, dst++, src++ ) {
				if( !*dst && *src >= threshold ) {
					// Found a new point to begin filling

					// SETUP the peak
					Peak p;
					p.whichRegion = side;

					PUSH( x, y );
					while( stackTop > 0 ) {
						// POP and EVALUATE the predicate of the pixel on the stack
						int _x, _y;
						POP( _x, _y );
						if( 0 <= _x && _x < wRegion &&  0 <= _y && _y < h ) {
							float *_src = &srcBits->lineFloat( _y )[_x];
							unsigned char *_dst = &dstBits.lineU1( _y )[_x];

							// EVAL predicate. If it passes, update dest, push all of its neighbors on the stack
							if( !*_dst && *_src >= threshold ) {
								*_dst = pCount;
								p.groupCount++;
								p.x += (float)_x;
								p.y += (float)_y;

								// Orthogonals:
								PUSH( _x+1, _y );
								PUSH( _x-1, _y );
								PUSH( _x, _y+1 );
								PUSH( _x, _y-1 );

								if( stackTop >= stackSize-16 ) {
									goto done;
								}
							}
						}
					}
					
					if( p.groupCount > 1 ) {
						p.x /= (float)p.groupCount;
						p.y /= (float)p.groupCount;
						peaks.add( p );
						pCount++;
						assert( pCount < 0xFFFE );
					}
				}
			}
		}
		done:;
	}

	int binSize = vidFile->getInfoI("bin");
	for( int i=0; i<peaks.count; i++ ) {
		// FETCH pixel list for display
		FVec2 p[REGION_MAX];
		p[peaks[i].whichRegion] = FVec2( peaks[i].x, peaks[i].y );

		if( peaks[i].whichRegion != REGION_LEFT ) {
			calibTransformWhichRegionToLeft( p[peaks[i].whichRegion], p[REGION_LEFT], peaks[i].whichRegion );
		}
		
		for( int r=0 ; r<REGION_MAX ; ++r ) {
			if( r != REGION_LEFT && r != peaks[i].whichRegion ) {
				calibTransformLeftToWhichRegion( p[REGION_LEFT], p[r], r );
			}
		}

		for( int r=0 ; r<REGION_MAX ; ++r ) {
			peaksPixelList( &region[r].imageWork.srcBits, p[r], 0.f, Fretview_peakMaskHatRadius, peaks[i].peak[r].hat.pixel, peaks[i].peak[r].hat.count, binSize );
			peaksPixelList( &region[r].imageWork.srcBits, p[r], Fretview_peakMaskHatRadius + Fretview_peakMaskBrimRadius, Fretview_peakMaskHatRadius + Fretview_peakMaskBrimRadius + Fretview_peakMaskAnnulusRadius, peaks[i].peak[r].ann.pixel, peaks[i].peak[r].ann.count, binSize );
		}

		// SEARCH for contention
		boolean detectContention = true;
		if( detectContention ) {
			for( int j=0; j<peaks.count; j++ ) {
				if( i != j ) {
					if( peaks[i].whichRegion == peaks[j].whichRegion ) {
						FVec2 dist( peaks[i].x, peaks[i].y );
						dist.sub( FVec2( peaks[j].x, peaks[j].y ) );
						if( dist.mag() < Fretview_peakContentionRadius ) {
							peaks[i].contention = 1;
							peaks[j].contention = 1;
						}
					}
					else {
						// COMPARE peaks on opposite sides looking for collisions
						FVec2 dist = FVec2( peaks[j].x, peaks[j].y );
						dist.sub( p[peaks[j].whichRegion] );	// Remember p represents the 'i' index's data 
						if( dist.mag() <= Fretview_peakMaskHatRadius ) {
							peaks[j].contention = 2;
						}
					}
				}
			}
		}
	}

	free( stack );
}

void ignoreFloatingPointOverflow() {
	unsigned int fp_control_word;
    unsigned int new_fp_control_word;

    _controlfp_s(&fp_control_word, 0, 0);

    // Make the new fp env same as the old one,
    // except for the changes we're going to make
    new_fp_control_word = fp_control_word | _EM_INVALID | _EM_DENORMAL | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT;
    //Update the control word with our changes
    _controlfp_s(&fp_control_word, new_fp_control_word, _MCW_EM);
}

// View
//----------------------------------------------------------------------------------------------------------------------------

void viewPlotTimeTraceCallback( void *_plotZidget ) {
	if( !_plotZidget ) {
		return;
	}
	assert( sizeof(float *) == sizeof(int *) );	// KD: added
	assert( _plotZidget == &plotTime );

	glLineWidth( 1.f );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	static DVec4 plotColor[REGION_MAX];
	plotColor[0] = DVec4(20,150,20,110);
	plotColor[1] = DVec4(200,50,50,1100);
	plotColor[2] = DVec4(200,50,50,110);
	plotColor[3] = DVec4(50,200,200,110);
	plotColor[4] = DVec4(200,200,50,110);

	for( int r=0 ; r<regionCount ; ++r ) {
		float *timeLft = plotTime.timeTrace[r].data;
		int timeCount = plotTime.timeTrace[r].count;
		if( timeLft ) {
			glColor4ub( (int)plotColor[r].r, (int)plotColor[r].g, (int)plotColor[r].b, (int)plotColor[r].a );
			glBegin( GL_LINE_STRIP );
			for( int i=0; i<timeCount; i++ ) {
				glVertex2f( (float)i, (float)timeLft[i] );
			}
			glEnd();
		}
	}
/*

	timeLft = (float *)user[0];
	timeRgt = (float *)user[1];
	if( timeLft && timeRgt ) {
		int timeCountLft = (int)timeLft[0];
		int timeCountRgt = (int)timeRgt[0];
		timeLft++;
		timeRgt++;

FILE *file = fopen( "/transfer/trace.txt", "wb" );
if( !file ) {
	zuiMessageBox( "Missing Directory", "The directory called /transfer/ is missing. Please create it." );
}
else {
	for( int i=0; i<min(timeCountLft,timeCountRgt); i++ ) {
		fprintf( file, "%f\n", (float)timeRgt[i] / ((float)timeRgt[i] + (float)timeLft[i]) );
	}
	fclose( file );
}
		glColor4ub( 0, 0, 0, 110 );
		glBegin( GL_LINE_STRIP );
		for( int i=0; i<min(timeCountLft,timeCountRgt); i++ ) {
			glVertex2f( (float)i, Fretview_fretY * (float)timeRgt[i] / ((float)timeRgt[i] + (float)timeLft[i]) );
			
		}
		glEnd();
	}
*/

#ifdef AUTO_CORR

	double *acLft = (double *)user[2];
	if( acLft ) {
		glColor4ub( 20, 20, 200, 110 );
		glLineWidth( 3.f );
		glBegin( GL_LINE_STRIP );

		//double minAC = +1e10;
		//double maxAC = -1e10;
		//for( int i=0; i<NUM_LAGS; i++ ) {
		//	minAC = min( minAC, (double)acLft[i] );
		//	maxAC = max( maxAC, (double)acLft[i] );
		//}

		for( int i=0; i<NUM_LAGS; i++ ) {
			//glVertex2d( Fretview_acX * (double)i, Fretview_acY * (((double)acLft[i]-minAC)/(maxAC-minAC)) );
			try {
				glVertex2d( Fretview_acX * (double)i, Fretview_acY * (double)acLft[i] );
			} catch(...) {
				// Catches (and ignores) floating point overflow.
			}
		}
		glEnd();
		glLineWidth( 1.f );
	}
#endif
}

void viewSetFrame( int frame ) {
	int x, y;
	int w = vidFile->w;
	int h = vidFile->h;
	int wRegion = vidFile->w / regionCount;

	if( load.inProgress() || load.neverStarted() ) {
		return;
	}

	Region &rLeft = region[REGION_LEFT];

//static unsigned char _bNoMansLandFill = 0xFD;   /* fill no-man's land with this */
//static unsigned char _bAlignLandFill  = 0xED;   /* fill no-man's land for aligned routines */
//static unsigned char _bDeadLandFill   = 0xDD;   /* fill free objects with this */
//static unsigned char _bCleanLandFill  = 0xCD;   /* fill new objects with this */

//	{
//		ZTmpStr debugInfo( "r[0].iRaw.srcBit.bits=0x%p; %x", region[0].imageRaw.srcBits.bits, region[0].imageRaw.srcBits.bits ? *(unsigned int *)region[0].imageRaw.srcBits.bits : 0 );
//		zMsgQueue( "type=ZUISet key=text val='%s' toZUI=debugInfoOld", debugInfo.s );
//	}

	if( region[0].imageRaw.srcBits.bits && *(unsigned int*)region[0].imageRaw.srcBits.bits == 0xCDCDCDCD ) {
		assert(0);
	}

	for( int r=0 ; r<regionCount ; ++r ) {
		imageInit( region[r].imageRaw, wRegion, h, r );
	}

//	{
//		ZTmpStr debugInfo( "r[0].iRaw.srcBit.bits=0x%p; %x", region[0].imageRaw.srcBits.bits, region[0].imageRaw.srcBits.bits ? *(unsigned int *)region[0].imageRaw.srcBits.bits : 0 );
//		zMsgQueue( "type=ZUISet key=text val='%s' toZUI=debugInfoNew", debugInfo.s );
//	}

	for( int r=0 ; r<regionCount ; ++r ) {
		selectInit( region[r].selectRaw, region[r].imageRaw, r, r!=REGION_LEFT );
	}

	float histogramDataLft[4096] = { 0, };
	float histogramDataRgt[4096] = { 0, };

	// Take the raw data from the movie and load it into the l & r images
	for( y=0; y<h; y++ ) {
		unsigned short *s;
		float *d;

		for( int r=0 ; r<regionCount ; ++r ) {
			s = &vidFile->getFrameU2(frame,y)[wRegion*r];
			d = region[r].imageRaw.srcBits.lineFloat(y);
			for( x=0; x<wRegion; x++ ) {
				histogramDataLft[(*s)>>4]++;
				*d++ = (float)*s++;
			}
		}
	}

	for( int r=0 ; r<regionCount ; ++r ) {
		histogramSet( region[r].histogramRaw, region[r].imageRaw, r, region[r].histogramRaw.handles[ZidgetHistogram_adjustMax].disabled );
		histogramImageSet( region[r].histogramRaw, region[r].imageRaw );
	}
}

void ZidgetPlotFret::extractAreaTimeTrace( int regionIndex, int radius, IVec2 select ) {
	if( !vidFile ) return;
	int n = vidFile->numFrames;
	int w = vidFile->w;
	int h = vidFile->h;
	if( w == 0 || h == 0 ) return;

	// KD: This could happen very easily by clicking on an edge, so I made it auto-adjust instead.
	//assert( select.x >= radius && select.x < w-radius );
	//assert( select.y >= radius && select.y < h-radius );

	select.x = max(radius,min(w-radius-1,select.x));
	select.y = max(radius,min(h-radius-1,select.y));

	timeTrace[regionIndex].alloc( n );

	float *t = timeTrace[regionIndex].data;

	int r2 = radius * radius;
	for( int i=0; i<n; i++ ) {
		unsigned int val = 0;
		for( int y=-radius; y<=radius; y++ ) {
			for( int x=-radius; x<=radius; x++ ) {
				if( x*x + y*y - 1 <= r2 ) {
					val += vidFile->getFrameU2( i, select.y+y )[ select.x+x ];
				}
			}
		}
		*t++ = (float)val;
	}
}

#ifdef AUTO_CORR
double *extractAreaAutoCorr( IVec2 select ) {
	int w = vidFile->w;
	int w2 = w / 2;
	int h = vidFile->h;
	if( w == 0 || h == 0 ) return 0;

	// KD: this could easily happen if you click out on an edge, so I correct the values

	select.x = max(0,min(w-1,select.x));
	select.y = max(0,min(h-1,select.y));

	assert( select.x >= 0 && select.x < w );
	assert( select.y >= 0 && select.y < h );

	double *trace = new double[ NUM_LAGS ];
	for( int i=0 ; i<NUM_LAGS ; ++i )
		trace[i] = 0.0f;

	if( gAutoCorr ) {
		double *src = &autoCorr[ (select.y*w2 + select.x) * NUM_LAGS ];
		memcpy( trace, src, sizeof(double) * NUM_LAGS );
	}
	return trace;
}
#endif


void updatePlots() {
	Region &rLeft = region[REGION_LEFT];
	Region &rRight = region[REGION_SECOND];

	int w = rLeft.imageRaw.srcBits.w();
	int h = rLeft.imageRaw.srcBits.h();
	int radius = (int)rLeft.selectRaw.radius;

	float &lx = rLeft.selectRaw.pos.x;
	float &ly = rLeft.selectRaw.pos.y;
	lx = max(min(lx,w-radius),radius);
	ly = max(min(ly,h-radius),radius);

	IVec2 selectRawLftI( (int)rLeft.selectRaw.pos.x, (int)rLeft.selectRaw.pos.y );
	IVec2 selectRawRgtI( (int)rRight.selectRaw.pos.x, (int)rRight.selectRaw.pos.y );
	IVec2 selectRawRgtFullI( (int)rRight.selectRaw.pos.x + w, (int)rRight.selectRaw.pos.y );

	plotTime.extractAreaTimeTrace( 0, radius, selectRawLftI );
	plotTime.extractAreaTimeTrace( 1, radius, selectRawRgtFullI );

#ifdef AUTO_CORR
	assert( sizeof(double*) == sizeof(int) );
	plotTime.user[2] = (int)extractAreaAutoCorr( selectRawLftI );
#endif

#ifdef DO_SERIES_THING
	// ACCUMULATE statistic about the selection circle
	int r2 = radius * radius;
	int allocR = max( 1, radius );

	// KD: DANGER: The magic number '6' below I don't yet understand
	float *seriesLft = (float *)alloca( sizeof(float) * 6 * allocR * allocR );
	float *seriesRgt = (float *)alloca( sizeof(float) * 6 * allocR * allocR );

	int seriesCount = 0;
	for( int y=-radius; y<=radius; y++ ) {
		for( int x=-radius; x<=radius; x++ ) {
			if( x*x + y*y - 1 <= r2 ) {
				seriesLft[seriesCount] = rLeft.imageRaw.srcBits.getFloat( selectRawLftI.x+x, selectRawLftI.y+y );
				seriesRgt[seriesCount] = rRight.imageRaw.srcBits.getFloat( selectRawRgtFullI.x+x, selectRawRgtFullI.y+y );
				seriesCount++;
			}
		}
	}

	assert( seriesCount < 6 * allocR * allocR );
	float meanLft, medianLft, stddevLft;
	float meanRgt, medianRgt, stddevRgt;
	zmathStatsF( seriesLft, 0, seriesCount, &meanLft, &medianLft, &stddevLft );
	zmathStatsF( seriesRgt, 0, seriesCount, &meanRgt, &medianRgt, &stddevRgt );
#endif
}

void updateSpotInfo() {

	if( !gCalibValid ) {
		zMsgQueue( "type=ZUIHide toZUI=fretviewSpotInfoPanel");
		zMsgQueue( "type=ZUIShow toZUI=fretviewSpotInfoWarning");
		return;
	}

	zMsgQueue( "type=ZUIShow toZUI=fretviewSpotInfoPanel");
	zMsgQueue( "type=ZUIHide toZUI=fretviewSpotInfoWarning");

	FVec2 p[REGION_MAX];
	p[REGION_LEFT] = region[REGION_LEFT].selectRaw.pos;
	for( int r=REGION_SECOND ; r<regionCount ; ++r ) {
		calibTransformLeftToWhichRegion( p[REGION_LEFT], p[r], r );
	}

	int frame = (int)( timeSelect.head.x * (float)vidFile->numFrames );
	frame = max( 0, min( vidFile->numFrames-1, frame ) );

	int binSize = vidFile->getInfoI("bin");
	Peak peak = peaksAnalyzeAtPos( p, frame, binSize );

	ZTmpStr headings("(x,y)\ncenMean\ncenMedi\ncenStdev\ncenCount\nbgMean\nbgMedi\nbgStdev\nbgCount\n");
	zMsgQueue( "type=ZUISet key=text val='%s' toZUI=fretviewSpotInfoTitle", escapeQuotes(headings.s) );

	for( int r=0 ; r<3 ; ++r ) {
		PeakInfo &h = peak.peak[r].hatInfo;
		PeakInfo &a = peak.peak[r].annInfo;
		ZTmpStr info;
		if( r < regionCount ) {
			info.set(
				"(%d,%d)\n%-2.1f\n%-2.1f\n%-2.1f\n%d\n%-2.1f\n%-2.1f\n%-2.1f\n%d\n", 
				(int)safeItoF(p[r].x), (int)safeItoF(p[r].y),
				h.mean, h.median, h.stddev, h.count,
				a.mean, a.median, a.stddev, a.count
			);
		}

		zMsgQueue( "type=ZUISet key=text val='%s' toZUI=fretviewSpotInfo%d", escapeQuotes(info.s), r );
	}

}

void viewAutoscale() {
	int mode = 0;
	float maxCnt = 0.f;
	int maxBin = 0;

	for( int r=0 ; r<regionCount; r++ ) {
		int count = region[r].histogramRaw.histogramCount;
		float *hist = region[r].histogramRaw.histogram;

		float sum = 0.f;
		for( int i=0; i<count; i++ ) {
			if( hist[i] > maxCnt ) {
				maxCnt = hist[i];
				mode = i;
			}
			sum += hist[i];
		}

		// FIND the point where some percent of the mass is on the left side
		float s = 0.f;
		int j = 0;
		for( j=0; r<count && s < 0.9999f * sum; j++ ) {
			s += hist[j];
		}

		region[r].histogramRaw.histScale0 = (float)mode;
		region[r].histogramRaw.histScale1 = (float)j;
	}
}

void render() {
	int i, j;

	setRegionCount(Fretview_regionCount);

	peakAnalysis.update();

	seqWriter.update();

	glClearColor( 0.5f, 0.5f, 0.5f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT );
	zglPixelMatrixFirstQuadrant();

	zviewpointSetupView();

	// UPDATE the loading status
	if( load.inProgress() ) {
		assert( !load.haltSignal );
		// The loaded thread is running
		ZUI *z = ZUI::zuiFindByName( "loadStatus" );
		if( z ) {
			z->putS( "text", ZTmpStr( "Loading %2.1f%%", load.statusMonitor * 100.f ) );
		}
	}
	
	// UPDATE the selection spot
	Region &rLeft = region[REGION_LEFT];
	Region &rRight = region[REGION_SECOND];

	int w = rLeft.imageRaw.srcBits.w();
	int h = rLeft.imageRaw.srcBits.h();
	int radius = (int)rLeft.selectRaw.radius;

	if( w > 0 && h > 0 ) {
		if( ! rLeft.selectRaw.pos.equals( gLastSelect ) ) {
			for( int r=0 ; r<regionCount ; ++r ) {
				if( r != REGION_LEFT )
					calibTransformLeftToWhichRegionAndBound( rLeft.selectRaw.pos, region[r].selectRaw.pos, r, w, h, radius );
				region[r].selectWork = region[r].selectRaw;
			}
			updatePlots();
			updateSpotInfo();
			gLastSelect = rLeft.selectRaw.pos;
		}
		else if( ! rLeft.selectWork.pos.equals( gLastSelect ) ) {
			for( int r=0 ; r<regionCount ; ++r ) {
				if( r != REGION_LEFT )
					calibTransformLeftToWhichRegionAndBound( rLeft.selectWork.pos, region[r].selectWork.pos, r, w, h, radius );
				region[r].selectRaw = region[r].selectWork;
			}
			updatePlots();
			updateSpotInfo();
			gLastSelect = rLeft.selectWork.pos;
		}
	}

	// UPDATE the plot scale
	if( vidFile ) {
		plotTime.scaleX = 1.f / (vidFile->numFrames == 0 ? 1 : (float)vidFile->numFrames);
		plotTime.pos.y = h + 20.f;

		// BOUND the timeSelect
		timeSelect.head.x = max( 0.f, min( 1.f, timeSelect.head.x ) );
		timeSelect.head.y = -0.05f;
		timeSelect.tail.x = timeSelect.head.x;
		timeSelect.tail.y = 1.f;

		// BOUND the select region
		rgnSelectStart.pos.x = max( 0.f, rgnSelectStart.pos.x );
		rgnSelectStart.pos.y = 0.1f;
		rgnSelectStop.pos.x = max( rgnSelectStart.pos.x + plotTime.scaleX, min( 1.f, rgnSelectStop.pos.x ) );
		rgnSelectStop.pos.y = 0.1f;
		rgnSelect.bl.x = rgnSelectStart.pos.x;
		rgnSelect.w = rgnSelectStop.pos.x - rgnSelectStart.pos.x;
		rgnSelect.bl.y = 0.f;
		rgnSelect.h = 1.f;

		// UPDATE the view image at the selected time
		int frame = (int)( timeSelect.head.x * (float)vidFile->numFrames );
		frame = max( 0, min( vidFile->numFrames-1, frame ) );
		if( frame != gLastFrame ) {
			viewSetFrame( frame );
			gLastFrame = frame;
		}
	}

	// UPDATE the image scales
	int wRegion = !vidFile ? 100 : vidFile->w / regionCount;
	for( int r=0 ; r<regionCount ; ++r ) {
		Region &reg = region[r];

//		reg.histogramRaw.xScale = ((float)reg.histogramRaw.histogramCount / (float)wRegion) * 0.5f;
		reg.histogramRaw.xScale = 1.0f;
		if( reg.histogramRaw.histogramCount > 0 ) { 
			int &m = reg.meaningfulHistCount;
			int c = reg.histogramRaw.histogramCount;
			float *h = reg.histogramRaw.histogram;
			int arbitraryMin = 50;
			int arbitraryStep = 10;
			m = max(arbitraryMin,min(m,c));
			while( h[m] > 0 && m < c-arbitraryStep )
				m += arbitraryStep;
			while( h[m] == 0 && m > arbitraryMin )
				m -= arbitraryStep;
			reg.histogramRaw.xScale = (float)wRegion / (float)(m+arbitraryStep*2);
		}
		reg.histogramRaw.yScale = 4.00f;
		reg.histogramRaw.histScale0 = max( 0.f, reg.histogramRaw.histScale0 );
		reg.histogramRaw.histScale1 = max( reg.histogramRaw.histScale0+1.f, reg.histogramRaw.histScale1 );

		reg.imageRaw.colorScaleMin = reg.histogramRaw.histScale0;
		reg.imageRaw.colorScaleMax = reg.histogramRaw.histScale1;

		reg.histogramWork.xScale = 1.f;
		reg.histogramWork.yScale = 4.00f;
		reg.histogramWork.histScale0 = max( 0.f, reg.histogramWork.histScale0 );
		reg.histogramWork.histScale1 = max( reg.histogramWork.histScale0+1.f, reg.histogramWork.histScale1 );

		// ZBS temp changed this from a threshoilding operation to a normal op
		//imageWorkLft.colorScaleMin = histogramWorkLft.histScale0;
		//imageWorkLft.colorScaleMax = histogramWorkLft.histScale0+1.f;
		//imageWorkRgt.colorScaleMin = histogramWorkRgt.histScale0;
		//imageWorkRgt.colorScaleMax = histogramWorkRgt.histScale0+1.f;
		reg.imageWork.colorScaleMin = reg.histogramWork.histScale0;
		reg.imageWork.colorScaleMax = reg.histogramWork.histScale1;

		//imageWorkLft.colorScaleMin = 0.f;
		//imageWorkLft.colorScaleMax = Fretview_hackL;
		//imageWorkRgt.colorScaleMin = 0.f;
		//imageWorkRgt.colorScaleMax = Fretview_hackR;
	}

	// DRAW all zidgets
	glPushMatrix();
	Zidget::zidgetRenderAll();
	glPopMatrix();

	calibRender();

	// DRAW the peaks
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	for( i=0; i<peaks.count; i++ ) {
		glPushMatrix();
		glColor4ub( 128, 128, 255, 100 );
		if( peaks[i].contention == 1 ) {
			glColor4ub( 255, 200, 128, 70 );
		}
		else if( peaks[i].contention == 2 ) {
			glColor4ub( 128, 255, 255, 70 );
		}

		int r = peaks[i].whichRegion;
		Region &reg = region[r];

		glTranslatef( reg.imageWork.pos.x, reg.imageWork.pos.y, 0.f );

		glBegin( GL_QUADS );
		for( j=0; j<peaks[i].peak[r].hat.count; j++ ) {
			IVec2 pt = peaks[i].peak[r].hat.pixel[j];
			glVertex2i( pt.x+0, pt.y+0 );
			glVertex2i( pt.x+1, pt.y+0 );
			glVertex2i( pt.x+1, pt.y+1 );
			glVertex2i( pt.x+0, pt.y+1 );
		}

		for( j=0; j<peaks[i].peak[r].ann.count; j++ ) {
			IVec2 pt = peaks[i].peak[r].ann.pixel[j];
			glVertex2i( pt.x+0, pt.y+0 );
			glVertex2i( pt.x+1, pt.y+0 );
			glVertex2i( pt.x+1, pt.y+1 );
			glVertex2i( pt.x+0, pt.y+1 );
		}
		glEnd();

		glPopMatrix();
	}
}


ZMSG_HANDLER( Fretview_ViewFilter ) {
	gViewingWorkImage = zmsgI( state );
	if( gViewingWorkImage == 0 ) {
		for( int r=0 ; r<regionCount ; ++r ) {
			region[r].imageRaw.attach();
			region[r].imageWork.detach();
		}
	}
	else {
		for( int r=0 ; r<regionCount ; ++r ) {
			region[r].imageRaw.detach();
			region[r].imageWork.attach();
		}
	}
}

ZMSG_HANDLER( Fretview_autoscale ) {
	viewAutoscale();
}

ZMSG_HANDLER( Fretview_Filter ) {
	imagesFilter();
}

ZMSG_HANDLER( Fretview_PeakFind ) {
	imagesPeakFind();
}

ZMSG_HANDLER( Fretview_Autocorrelation ) {
#ifdef AUTO_CORR
	imagesAutocorrelation();
#endif
}

ZMSG_HANDLER( Fretview_PeakMap ) {
	if( !vidFile )	
		return;

	int which = zmsgI( which );
	if( which == -1 )
		if( regionCount < 3 )
			return;
		else
			which = 1;			// The center region

	if( which == -2 )
		which = regionCount-1; // always the right-most region


	peaksMapSideToSide( which );
	int binSize = vidFile->getInfoI("bin");
	peaksSetupPixelLists( binSize );
}

ZMSG_HANDLER( Fretview_PeakClear ) {
	peaks.clear();
	gPeaksVisible = 0;
	zMsgQueue( "type=ZUIDisable toZUI='fretviewPeakOpt'" );
}

ZMSG_HANDLER( Fretview_PeakOptimize ) {
	peaksOptimize();
	int binSize = vidFile->getInfoI("bin");
	peaksSetupPixelLists( binSize );
}

ZMSG_HANDLER( Fretview_DumpAnalysis ) {
	if( !vidFile )
		return;

	if( peaks.count > 0 ) {
		peakAnalysis.start();
	}
	else {
		zMsgQueue( "type=ZUISet key=text val='No peaks to analyze' toZUI=fretviewAnalysisInfo" );
	}
}

// Startup
//----------------------------------------------------------------------------------------------------------------------------

ZMSG_HANDLER( Fretview_Loaded ) {
	// This is a call back when the thread has finsihed loading a file

	assert( !load.haltSignal );

	char *filespec = zmsgS(filespec);
	fretviewConfig.putS( "lastVidLoaded", filespec );
	strcpy( load.lastLoadedFilename, filespec );
	ZFileSpec spec(filespec);

	zMsgQueue( "type=ZUISet key=text val='Loaded' toZUI=loadStatus" );

	char *s = spec.getFile();
	zMsgQueue( "type=ZUISet key=text val='\"%s\"' toZUI=fretviewFile", escapeQuotes(s) );
	zMsgQueue( "type=ZUISet key=text val='(%d x %d)' toZUI=fretviewDimensions", vidFile->w, vidFile->h );
	zMsgQueue( "type=ZUISet key=text val='#%d' toZUI=fretviewFrames", vidFile->numFrames );
	zMsgQueue( "type=ZUISet key=text val='#%d' toZUI=fretviewBin", vidFile->getInfoI("bin") );

	viewSetFrame( 0 );
}

static PyObject *PmaModule_SetMetaData(PyObject *self, PyObject *args) {
	char *metaData = 0;
	PyArg_ParseTuple(args,"s",&metaData);
	if( !vidFile )
		return Py_BuildValue("i",1);
	if( vidFile->isRecording() )
		return Py_BuildValue("i",2);

	vidFile->setMetaData(metaData);
	return Py_BuildValue("i",0);
}
PythonRegister register_PmaSetMetaData( "pma", "setMetaData", &PmaModule_SetMetaData);

static PyObject *PmaModule_GetMetaData(PyObject *self, PyObject *args) {
	if( !vidFile )
		return Py_BuildValue("i",1);
	return Py_BuildValue("s",vidFile->getMetaData());
}
PythonRegister register_PmaGetMetaData( "pma", "getMetaData", &PmaModule_GetMetaData);


void freeCallback(void * pUserData) {
//	if( pUserData && (pUserData == (void *)region[0].imageRaw.srcBits.bits || pUserData == (void *)region[0].imageRaw.srcBits.mallocBits) ) {
//		assert(0);
//	}
}

void stdoutHook( char *text ) {
	traceNoFormat(text);
}

void startup() {
//	useDirtyRects = 1;

	pythonInterpreter.startup(stdoutHook);

	for( int r=0 ; r<REGION_MAX ; ++r )
		region[r].init(r);

	colorPaletteInit();

	vidFile = 0;
	load.clear();
	peaks.clear();

	// SETUP the control panel
	ZUI::zuiExecuteFile( pluginPath( "_fretview.zui" ) );
	ZUI *z = ZUI::zuiFindByName( "fretviewZUI" );
	if( z ) {
		int viewport[4];
		glGetIntegerv( GL_VIEWPORT, viewport );
		z->putI( "scrollYHome", -10000+viewport[3] ); 
		z->scrollY = (float)( -10000+viewport[3] );
	}

	plotTime.attach();
	plotTime.pos.x = 0.f;
	plotTime.pos.y = 520.f;
	plotTime.w = 512.f;
	plotTime.h = 256.f;
	plotTime.unrangedCallback = viewPlotTimeTraceCallback;
	plotTime.panelColor = FVec4( 0.7f, 0.7f, 0.7f, 1.f );
	plotTime.handles[ZidgetPlot_trans].disabled = 1;
	plotTime.scaleY = 1.f / 10000.f;
	plotTime.setName("plotTime");

	timeSelect.attach( &plotTime );
	timeSelect.color = FVec4( 0.1f, 0.2f, 0.5f, 1.f );
	timeSelect.thickness = 3.f;
	timeSelect.head.x = 0.f;
	timeSelect.handles[ZidgetLine2_tail].disabled = 1;
	timeSelect.setName("timeSelect");

	rgnSelect.attach( &plotTime );
	rgnSelect.color = FVec4( 0.f, 0.f, 1.f, 0.1f );
	rgnSelect.handles[ZidgetLine2_bl].disabled = 1;
	rgnSelect.handles[ZidgetLine2_wh].disabled = 1;
	rgnSelect.setName("rgnSelect");
	
	rgnSelectStart.attach( &plotTime );
	rgnSelectStart.pos = FVec2( 0.f, 0.1f );
	rgnSelectStart.handles[0].zidgetHandleDrawCallback = zidgetHandleHorizArrows;
	rgnSelectStart.setName("rgnSelectStart");

	rgnSelectStop.attach( &plotTime );
	rgnSelectStop.pos = FVec2( 1.f, 0.1f );
	rgnSelectStop.handles[0].zidgetHandleDrawCallback = zidgetHandleHorizArrows;
	rgnSelectStop.setName("rgnSelectStop");

//	calibLft.handles[0].visibleAlways = 0;
//	calibLft.handles[0].zidgetHandleDrawCallback = zidgetHandleCrossHairs;
//	calibLft.setName("calibLft");

//	calibRgt.handles[0].visibleAlways = 0;
//	calibRgt.handles[0].zidgetHandleDrawCallback = zidgetHandleCrossHairs;
//	calibRgt.setName("calibRgt");

	strcpy( fretviewDir, options.getS("Fretview_dir","") );

	loadConfig(fretviewConfig);

	pythonConsoleStart(&fretviewConfig,options.getS("Fretview_pythonDir",""));

	if( !*fretviewDir ) {
		ZFileSpec filespec( fretviewConfig.getS("lastVidLoaded","") );
		strcpy( fretviewDir, filespec.getDrive()  );
		strcat( fretviewDir, filespec.getDir()  );
	}

	zMsgQueue( "type=ZUIDisable toZUI='fretviewPeakOpt'" );

	setCalibRender(0);
	setCalibDirty(0);

//debug
//zMsgQueue( "type=ZUISet key=text val='File a of b' toZUI=fretviewSeqProgress" );
}

void shutdown() {
	if( vidFile ) {
		delete vidFile;
		vidFile = 0;
	}

	load.clear();
	peaks.clear();

	plotTime.detach();

	for( int r=0 ; r<REGION_MAX ; ++r ) {
		Region &reg = region[r];
		reg.imageRaw.detach();
		reg.imageWork.detach();
		reg.selectRaw.detach();
		reg.selectWork.detach();
		reg.histogramRaw.detach();
		reg.histogramWork.detach();
	}

	timeSelect.detach();
	rgnSelect.detach();
	rgnSelectStart.detach();
	rgnSelectStop.detach();

	for( int r=0 ; r<REGION_MAX ; ++r ) {
		calibCursor[r].detach();
	}

	// SAVE the current options
	saveConfig();

	pythonInterpreter.shutdown();

	zMsgQueue( "type=ZUIDie toZUI=styleFretviewSectionHeader" );
	zMsgQueue( "type=ZUIDie toZUI=styleFretviewTwiddler" );
	zMsgQueue( "type=ZUIDie toZUI=fretviewZUI" );

	ZUI *z = ZUI::zuiFindByName( "pythonPanel" );
	if( z )
		z->die();
}

void handleMsg( ZMsg *msg ) {
	int i;

	zviewpointHandleMsg( msg );

	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Zidget_ZidgetHandleDrag) ) {
		if( zmsgIs(which,L) ) {
			updateCalibMessage();
		}
	}

	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
		FVec2 mouse = zviewpointProjMouseOnZ0Plane();
		ZidgetImage &image = region[REGION_LEFT].imageRaw;
		if(
			mouse.x >= image.pos.x && mouse.x < image.pos.x + image.srcBits.w() &&
			mouse.y >= image.pos.y && mouse.y < image.pos.y + image.srcBits.h()
		) {
			float closestMag = 100000.f;
			int closestI = -1;

			if( Fretview_calibRender ) {
				calibSelect( mouse );
			}
			else {
				// FIND the closest peak
				for( i=0; i<peaks.count; i++ ) {
					if( peaks[i].whichRegion == REGION_LEFT ) {
						FVec2 dist = mouse;
						dist.sub( FVec2( peaks[i].x, peaks[i].y ) );
						if( dist.mag() <= Fretview_peakMaskAnnulusRadius ) {
							closestMag = dist.mag();
							closestI = i;
						}
					}
				}
				if( closestI >= 0 ) {
					region[REGION_LEFT].selectRaw.pos = FVec2( peaks[closestI].x, peaks[closestI].y );
					printf( "region[0].selectRaw.pos = (%2.1f,%2.1f)", peaks[closestI].x, peaks[closestI].y );
				}
			}
		}
	}
	else {
//		trace("---\n");
//		for( ZHashWalk n( *msg ); n.next(); ) {
//			trace( "%s = %s\n", n.key, n.val );
//		}

	}


	Zidget::zidgetHandleMsg( msg );
}


ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;

/*
extern "C" _CRTIMP void __cdecl free( void * pUserData )
{
	fretview::freeCallback(pUserData);
	_CRTIMP void __cdecl _free_dbg( void * pUserData,int nBlockUse );
	_free_dbg(pUserData, 1 ); // 1 means _NORMAL_BLOCK_
}
*/
