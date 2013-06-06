// @ZBS {
//		*MODULE_DEPENDS fitdata.cpp zhash2d.cpp
//		*SDK_DEPENDS gsl-1.8 clapack pthread
//		*EXTRA_DEFINES ZMSG_MULTITHREAD
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
#include "gsl/gsl_errno.h"
#include "gsl/gsl_odeiv.h"
#include "gsl/gsl_linalg.h"
#include "gsl/gsl_spline.h"
#include "gsl/gsl_interp.h"
#include "pthread.h"

// STDLIB includes:
#include "math.h"
#include "float.h"
#include "string.h"
#include "stdlib.h"
#ifdef WIN32
#include "malloc.h"
#endif
// MODULE includes:
// ZBSLIB includes:
#include "zmathtools.h"
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zviewpoint.h"
#include "zrand.h"
#include "zgltools.h"
#include "ztime.h"
#include "zglfont.h"
#include "ztmpstr.h"
#include "plot.h"
#include "zui.h"
#include "kineticbase.h"
#include "kineticfitter.h"
#include "zconsole.h"

extern void trace( char *fmt, ... );

// This is a prototype for the anlystic ODE system that will go into kin 2.0

//ZVAR( double, Aode_noise, 1e-4 );
ZVAR( double, Aode_noise, 0 );
ZVAR( int, Aode_plotOGWRT, 1 );
ZVAR( int, Aode_autoscale, 0 );
ZVARB( int, Aode_plotDebugTraces, 1, 0, 1 );

//int Kin_fitPositiveFactors = 1;
//int Kin_fitPositiveRates = 1;
//int Kin_fitspaceCalcLBoundDetail;
	// @TODO This and the fitdata.cpp zhash2d.cpp are here only while TFB is merging


int plotNew;
int plotLogX;
int plotLogY;


ZPLUGIN_BEGIN( aode );


// Plotting
//------------------------------------------------------------------------------------------

static float colors[][3] = {
	{1.f, 0, 0}, {0, 1.f, 0}, {0, 0, 1.f},
	{1.f, 1.f, 0}, {0, 1.f, 1.f}, {1.f, 0, 1.f},
	{0, 0, 0}, {1.f, 1.f, 1.f},

	{0.5f, 0, 0}, {0, 0.5f, 0}, {0, 0, 0.5f},
	{0.5f, 0.5f, 0}, {0, 0.5f, 0.5f}, {0.5f, 0, 0.5f},
	{0, 0, 0}, {0.5f, 0.5f, 0.5f},

};

struct TracePlot {
	KineticTrace *trace;
	int row;
	int colorIndex;
	int flags;
		#define KTP_STYLE_POINTS_COLOR_MARKS   (1)
		#define KTP_STYLE_POINTS_BLACK_DOTS    (2)
		#define KTP_STYLE_LINE_SOLID           (4)
		#define KTP_STYLE_LINE_DASHED          (8)
		#define KTP_STYLE_LINE_DOTTED          (16)
		#define KTP_STYLE_LINE_DOTDASHED       (32)
		#define KTP_STYLE_LINE_HALO            (64)
		#define KTP_STYLE_LINE_BOLD            (128)
		#define KTP_STYLE_SLERP                (256)
	char name[64];

	TracePlot() { trace = 0; row = 0; colorIndex = 0; flags = 0; name[0] = 0; }
	TracePlot( KineticTrace *_trace, int _row, int _colorIndex,  int _flags, char *_name ) {
		// I have to fork a copy of the trace because by the time I render it the other threads
		// could have terminted and delted it.
		trace = new KineticTrace();
		trace->copy( *_trace );
		row = _row;
		colorIndex = _colorIndex;
		flags = _flags;
		strncpy( name, _name, 64 );
	}
	~TracePlot() {
		if( trace ) {
			delete trace;
		}
	}
};

int kineticTracePlotCollisionDetect( TracePlot **plots, int plotCount, FVec2 collisionDetect ) {
	int p, i;

	FVec3 coll( (float)collisionDetect.x, (float)collisionDetect.y, 0.f );

	float closestDist = 1e10f;
	FVec2 closestDelta;
	int closestP = -1;

	for( p=0; p<plotCount; p++ ) {
		KineticTrace *trace = plots[p]->trace;
		int row = plots[p]->row;
		int cols = trace->cols;
		if( trace->data ) {
			FVec3 last( (float)trace->getTime(0), (float)trace->getData(0,row), 0.f );
			for( i=1; i<cols; i++ ) {
				FVec3 curr( (float)trace->getTime(i), (float)trace->getData(i,row), 0.f );
				FVec3 delta = curr;
				delta.sub( last );

				if( delta.mag2() > 0.f ) {
					FVec3 dist = zmathPointToLineSegment( coll, delta, curr );
					float mag2 = dist.mag2();
					if( mag2 < closestDist ) {
						if( mag2 < closestDist ) {
							closestDelta = FVec2( dist.x, dist.y );
							closestDist = mag2;
							closestP = p;
						}
					}
				}
				last = curr;
			}
		}
	}

	float onePixelX, onePixelY;
	zglPixelInWorldCoords( onePixelX, onePixelY );
	if( closestDelta.x > onePixelX*4.f || closestDelta.y > onePixelY*4.f ) {
		closestP = -1;
	}
	return closestP;
}

void kineticTracePlotArray( TracePlot **plots, int plotCount ) {
	// Given a list of traces plot each one.  
	// Also optionally given a collision point scan for the closest approach

	// COMPUTE world size of 1 pixel
	float onePixelX, onePixelY;
	zglPixelInWorldCoords( onePixelX, onePixelY );
	float tickSizeX = onePixelX * 2.f;
	float tickSizeY = onePixelY * 2.f;

	int finalPass = 0;
	for( int p=0; p<plotCount; p++ ) {
		KineticTrace *trace = plots[p]->trace;
		int row = plots[p]->row;
		int colorIndex = plots[p]->colorIndex;
		int flags = plots[p]->flags;

		int i;

		colorIndex = colorIndex % 16;
		int cols = trace->cols;

		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		float offset = 0.f;
		if( flags & KTP_STYLE_LINE_BOLD ) {
			glLineWidth( 3.f );
		}
		else {
			glLineWidth( 1.f );
		}

		if( flags & KTP_STYLE_POINTS_COLOR_MARKS ) {
			glColor4f( colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2], 0.5f );
			glBegin( GL_LINES );
				for( i=0; i<cols; i++ ) {
					double x = trace->getTime(i);
					double y = trace->getData(i,row);
					y += offset;
					glVertex2d( x-tickSizeX, y+tickSizeY );
					glVertex2d( x+tickSizeX, y-tickSizeY );
					glVertex2d( x-tickSizeX, y-tickSizeY );
					glVertex2d( x+tickSizeX, y+tickSizeY );
				}
			glEnd();
		}

		if( flags & KTP_STYLE_LINE_HALO ) {
			glPushAttrib( GL_LINE_BIT );
			glColor4f( 0.f, 0.f, 0.f, 0.2f );
			glLineWidth( 8.f );
			glBegin( GL_LINE_STRIP );

			if( flags & KTP_STYLE_SLERP ) {
				double endTime = trace->getTime(cols-1);
				for( double t=0.0; t<=endTime; t+=endTime/1000.0 ) {
					double y = trace->getElemSLERP(t,row);
					y += offset;
					glVertex2d( t, y );
				}
			}
			else {
				for( i=0; i<cols; i++ ) {
					double x = trace->getTime(i);
					double y = trace->getData(i,row);
					y += offset;
					glVertex2d( x, y );
				}
			}

			glEnd();
			glPopAttrib();
		}

		if( (flags & KTP_STYLE_LINE_SOLID) || (flags & KTP_STYLE_LINE_DASHED) || (flags & KTP_STYLE_LINE_DOTTED) || (flags & KTP_STYLE_LINE_DOTDASHED) ) {
			glColor4f( colors[colorIndex][0], colors[colorIndex][1], colors[colorIndex][2], 1.f );
			if( (flags & KTP_STYLE_LINE_SOLID) ) {
				glDisable( GL_LINE_STIPPLE );
			}
			else if( (flags & KTP_STYLE_LINE_DASHED) ) {
				glLineStipple( 1, 0xFF80 );
				glEnable( GL_LINE_STIPPLE );
			}
			else if( (flags & KTP_STYLE_LINE_DOTTED) ) {
				glLineStipple( 1, 0x3030 );
				glEnable( GL_LINE_STIPPLE );
			}
			else if( (flags & KTP_STYLE_LINE_DOTDASHED) ) {
				glLineStipple( 1, 0x10FF );
				glEnable( GL_LINE_STIPPLE );
			}
			glBegin( GL_LINE_STRIP );
				if( flags & KTP_STYLE_SLERP ) {
					double endTime = trace->getTime(cols-1);
					for( double t=0.0; endTime > 0.0 && t<=endTime; t+=endTime/1000.0 ) {
						double y = trace->getElemSLERP(t,row);
						y += offset;
						glVertex2d( t, y );
					}
				}
				else {
					for( i=0; i<cols; i++ ) {
						double x = trace->getTime(i);
						double y = trace->getData(i,row);
						y += offset;
						glVertex2d( x, y );
					}
				}
			glEnd();
			glDisable( GL_LINE_STIPPLE );
		}

		if( flags & KTP_STYLE_POINTS_BLACK_DOTS ) {
			glColor4f( 0.f, 0.f, 0.f, 1.f );
			glBegin( GL_LINES );
				for( i=0; i<cols; i++ ) {
					double x = trace->getTime(i);
					double y = trace->getData(i,row);
					y += offset;
					glVertex2d( x-tickSizeX, y+tickSizeY );
					glVertex2d( x+tickSizeX, y-tickSizeY );
					glVertex2d( x-tickSizeX, y-tickSizeY );
					glVertex2d( x+tickSizeX, y+tickSizeY );
				}
			glEnd();
		}

		if( finalPass ) {
			break;
		}
	}
}

// Fitter Status ZUI
//------------------------------------------------------------------------------------------

struct ZUIFitterStatusText : ZUIText {
	ZUI_DEFINITION(ZUIFitterStatusText,ZUIText);

	virtual void render() {
		int r;
		char buffer[1024] = {0,};

		KineticFitter *fitter = (KineticFitter *)getI( "fitter" );
		if( fitter ) {
			static char *fitStatusText[] = { "Unstarted", "Running", "Converged", "Trapped", "Cancelled", "Single Step", "Paused" };
			sprintf(
				buffer, "Status: %s\npropCount: %d\norigCount: %d\nError: %.2lg\nbackoff: %d\ncountSinceOrig: %d\npropEvalTime: %.2lg\norigNewTime: %.2lg\n",
				fitStatusText[ fitter->fitThreadStatus ], 
				fitter->propCount,
				fitter->origCount,
				fitter->propE,
				fitter->propBackoffStepsOnLastEigenValue,
				fitter->propCountSinceOrig,
				fitter->propEvalAccum,
				fitter->origNewAccum
			);

			strcat( buffer, "Reaction rates:\n" );

			for( r=0; r<fitter->reactionCount(); r++ ) {
				strcat( buffer, ZTmpStr( "  %d: %-.2lf\n", r, fitter->paramGetReactionRate(r) ) );
			}
		}
		else {
			strcpy( buffer, "Status: Unstarted\n" );
		}

		putS( "text", buffer );
		ZUIText::render();
	}
};

ZUI_IMPLEMENT(ZUIFitterStatusText,ZUIText);


struct ZUIFitterSVDBarPlot : ZUIPanel {
	ZUI_DEFINITION(ZUIFitterSVDBarPlot,ZUIPanel);

	#define colSpace (4.f)

	virtual void factoryInit() {
		putI( "dragging", -1 );
	}

	virtual void render() {
		ZUIPanel::render();
		KineticFitter *fitter = (KineticFitter *)getI( "fitter" );
		if( fitter ) {
			glColor3ub( 0, 0, 0 );

			float myW, myH;
			getWH( myW, myH );

			float mouseX, mouseY;
			getMouseLocalXY( mouseX, mouseY );

			glPushMatrix();
			glTranslatef( colSpace, colSpace, 0.f );
			mouseX -= colSpace;
			mouseY -= colSpace;

			int qCount = fitter->qBeingFitCount;

			const float sigmaInPixels = 20.f;

			int isDragging = getI("dragging") >= 0;
			int whichDragging = getI("dragging");

			float colWidth = qCount > 0 ? (myW-2.f*colSpace) / qCount - colSpace : 0.f;

			if( ! isDragging ) {
				// DRAW log axis
				glBegin( GL_LINES );
				for( int j=0; j<10; j++ ) {
					glColor4ub( 0, 0, 0, 150 );
					float y = 10.f * logf( powf(10.f,(float)j) );
					glVertex2f( 0.f, y );
					glVertex2f( myW-colSpace*2.f, y );
				}
				glEnd();

				glColor3ub( 0, 0, 100 );
				glPushMatrix();
				glRotatef( 90.f, 0.f, 0.f, 1.f );
				zglFontPrint( "Significant Digits (log 1+S/N)", 0.f, +10.f, "controls" );
				glPopMatrix();

				zglFontPrint( "Singular component (red if information is 0)", 0.f, -20.f, "controls" );
				zglFontPrint( "Drag yellow handle to visualize movement", 0.f, -35.f, "controls" );

			}

			if( fitter->origSignal.rows > 0 ) {
				for( int q=0; q<qCount; q++ ) {
					float x = (float)q * (colWidth + colSpace);
					float y = 0.f;

					float snr = (float)fitter->getSNRForRender( q );
					y = 10.f * logf( snr + 1.0f );
					y = max( 1.f, y );

					if( isDragging ) {
						glColor4f( 0.f, 0.f, 0.f, 0.1f );
					}
					else {
						if( snr > 0.f ) {
							glColor4f( 1.f, 1.f, 1.f, 0.5f );
						}
						else {
							glColor4f( 1.f, 0, 0, 0.5f );
						}
					}

					glBegin( GL_QUADS );
						glVertex2f( x, 0.f );
						glVertex2f( x+colWidth, 0.f );
						glVertex2f( x+colWidth, y );
						glVertex2f( x, y );
					glEnd();

					if( q == whichDragging ) {
						if( getF( "lastTwiddle" ) != mouseY ) {
							float delta = mouseY / sigmaInPixels;
							float s = (float)fitter->origSignal.getD(q,0);
							zMsgQueue( "type=FitterEigenTwiddle fitter=0x%x param=%d value=%f deltaValue=%f", (int)fitter, q, s, delta );
						}
						putF( "lastTwiddle", mouseY );
					}

					// DRAW a manipulation handles
					if( !isDragging || q == whichDragging ) {
						FVec2 dist( x + colWidth/2.f, 0.f );
						dist.sub( FVec2( mouseX, mouseY ) );
						float d = dist.mag2();
						d = max( 0.01f, d );
						float alpha = 100.f / d;
						if( q == whichDragging ) {
							alpha = 1.f;
						}

						float y = q == whichDragging ? mouseY : 0.f;

						glColor4f( 1.f, 1.f, 0.f, alpha );
						glBegin( GL_LINE_LOOP );
						for( int i=0; i<16; i++ ) {
							glVertex2f( x + colWidth/2.f + 5.f*cos64F[i<<2], y + 5.f*sin64F[i<<2] );
						}
						glEnd();

						if( q == whichDragging ) {
							glColor4ub( 200, 200, 0, 80 );
							glBegin( GL_QUADS );
								glVertex2f( x+colWidth/2.f-1.f, 0.f );
								glVertex2f( x+colWidth/2.f+1.f, 0.f );
								glVertex2f( x+colWidth/2.f+1.f, y );
								glVertex2f( x+colWidth/2.f-1.f, y );
							glEnd();
						}
					}
					else {
						// DRAW the sigma scale if dragging
						glLineWidth( 3.f );
						glBegin( GL_LINES );
							glColor4ub( 255, 255, 0, 255 );
							glVertex2f( 0.f, +0.f*sigmaInPixels );
							glVertex2f( myW-2.f*colSpace, +0.f*sigmaInPixels );
						glEnd();

						glLineWidth( 1.f );
						glBegin( GL_LINES );
							glColor4ub( 200, 200, 0, 80 );
							glVertex2f( 0.f, +1.f*sigmaInPixels );
							glVertex2f( myW-2.f*colSpace, +1.f*sigmaInPixels );
							glVertex2f( 0.f, +2.f*sigmaInPixels );
							glVertex2f( myW-2.f*colSpace, +2.f*sigmaInPixels );
							glVertex2f( 0.f, -1.f*sigmaInPixels );
							glVertex2f( myW-2.f*colSpace, -1.f*sigmaInPixels );
							glVertex2f( 0.f, -2.f*sigmaInPixels );
							glVertex2f( myW-2.f*colSpace, -2.f*sigmaInPixels );
						glEnd();

						glPushMatrix();
						glRotatef( 90.f, 0.f, 0.f, 1.f );
						zglFontPrint( "Movement in", -40.f, +20.f, "controls" );
						zglFontPrint( "standard deviations", -60.f, +7.f, "controls" );
						glPopMatrix();

					}
				}
			}

			glPopMatrix();
		}
	}

	void handleMsg( ZMsg *msg ) {
		if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
			zMsgUsed();
			requestExclusiveMouse( 0, 1 );

			KineticFitter *fitter = (KineticFitter *)getI( "fitter" );
			if( fitter ) {
				int qCount = fitter->qBeingFitCount;
				float myW, myH;
				getWH( myW, myH );
				float colWidth = qCount > 0 ? (myW-2.f*colSpace) / qCount - colSpace : 0.f;
				float which = zmsgF(localX) / (colWidth + colSpace);
				int whichI = (int)which;
				if( whichI >= 0 && which < qCount ) {
					putI( "dragging", whichI );
				}
			}
			return;
		}

		else if( zmsgIs(type,ZUIMouseReleaseDrag) ) {
			zMsgUsed();
			putI( "dragging", -1 );
			requestExclusiveMouse( 0, 0 );
			zMsgQueue( "type=FitterEigenTwiddleStop" );
		}

		ZUI::handleMsg( msg );
	}
};

ZUI_IMPLEMENT(ZUIFitterSVDBarPlot,ZUIPanel);


struct ZUIFitterParameterAdjustPlot : ZUIPanel {
	ZUI_DEFINITION(ZUIFitterParameterAdjustPlot,ZUIPanel);

	ZMat deltaParamVec;

	virtual void render() {
		int r;

		ZUIPanel::render();
		int twiddling = getI( "twiddling" );
		KineticFitter *fitter = (KineticFitter *)getI( "fitter" );
		if( fitter ) {
			float myW, myH;
			getWH( myW, myH );

			glPushMatrix();
			glTranslatef( myW / 2.f - 20.f, 0.f, 0.f );

			int reactionCount = fitter->reactionCount();
			float y;
			for( r=0; r<reactionCount; r+=2 ) {
				y = myH - (float)(r/2+1) * 60.f;

				if( twiddling ) {
					// DRAW the delta bars
					float scale = 100.f;
					float x0 = (float)deltaParamVec.getD( r+0, 0 );
					float x1 = (float)deltaParamVec.getD( r+1, 0 );

					glColor3ub( 0, 200, 0 );
					zglFontPrint( ZTmpStr("%4.1lf (%+4.1lf)", fitter->paramGetReactionRate(r+0)+x0, x0 ), x, y+18.f, "controls" );
					zglArrow( 25.f, y+12.f, 25.f+scale*x0, y+12.f, 0.f, 0.f, 2.f );

					if( r+1 < reactionCount ) {
						glColor3ub( 200, 0, 0 );
						zglFontPrint( ZTmpStr("%4.1lf (%+4.1lf)", fitter->paramGetReactionRate(r+1)+x1, x1 ), x, y-14.f, "controls" );
						zglArrow( 25.f, y+6.f, 25.f+scale*x1, y+6.f, 0.f, 0.f, 2.f );
					}
				}
				else {
					// DRAW the static arrows indicating reaction direction
					float x = 10.f;
					glColor3ub( 0, 200, 0 );
					zglFontPrint( ZTmpStr("%4.1lf", fitter->paramGetReactionRate(r+0)), x, y+18.f, "controls" );
					zglArrow( x+5.f, y+12.f, x+30.f, y+12.f, 0.f, 2.f, 2.f );

					if( r+1 < reactionCount ) {
						glColor3ub( 200, 0, 0 );
						zglFontPrint( ZTmpStr("%4.1lf", fitter->paramGetReactionRate(r+1)), x, y-14.f, "controls" );
						zglArrow( x+25.f, y+6.f, x+0.f, y+6.f, 0.f, 2.f, 2.f );
					}
				}

				// PRINT the reagent names
				for( int side=0; side<2; side++ ) {
					ZTmpStr str( "%s%s%s",
						fitter->reactionGet(r,side,0) == -1 ? "" : fitter->reagentGetName( fitter->reactionGet(r,side,0) ),
						fitter->reactionGet(r,side,0) != -1 && fitter->reactionGet(r,side,1) != -1 ? " + " : "",
						fitter->reactionGet(r,side,1) == -1 ? "" : fitter->reagentGetName( fitter->reactionGet(r,side,1) )
					);
					float strW, strH, strD;
					zglFontGetDim( str, strW, strH, strD, "controls" );

					glColor3ub( 0, 0, 0 );
					zglFontPrint( str, side==0 ? -strW : 50.f, y, "controls" );
				}
			}

			int eCount = fitter->eCount();
			int reagentCount = fitter->reagentCount();
			y -= 40.f;
			zglFontPrint( "Initial conditions", -20.f, y, "controls" );
			y -= 14.f;
			for( int e=0; e<eCount; e++ ) {
				for( r=0; r<reagentCount; r++ ) {
					glColor3ub( 0, 0, 0 );
					zglFontPrint( ZTmpStr("Ex %d: %s %4.1lf", e, fitter->reagentGetName(r), fitter->paramGetIC(e,r)), 0.f, y, "controls" );
					y -= 14.f;
				}
			}

			int obsConstCount = 0;
			KineticParameterInfo *pInfo = fitter->paramGet( PI_OBS_CONST, &obsConstCount );
			y -= 40.f;
			zglFontPrint( "Observable constants", -20.f, y, "controls" );
			y -= 14.f;
			for( int o=0; o<obsConstCount; o++ ) {
				glColor3ub( 0, 0, 0 );
				zglFontPrint( ZTmpStr("%d: %s %5.3lf", o, pInfo[o].name, pInfo[o].value), 0.f, y, "controls" );
				y -= 14.f;
			}

			glPopMatrix();
		}
	}

	void handleMsg( ZMsg *msg ) {
		if( zmsgIs(type,FitterEigenTwiddle) ) {
			KineticFitter *fitter = (KineticFitter *)zmsgI(fitter);

			ZMat deltaSignal;
			deltaSignal.alloc( fitter->origSignal.rows, fitter->origSignal.cols, zmatF64 );
//			double sign = zmathSignDD( fitter->fitSignal.getD( zmsgI(param), 0 ) );
			deltaSignal.putD( zmsgI(param), 0, (double)zmsgF(deltaValue) );

			zmatMul( fitter->origVtt, deltaSignal, deltaParamVec );

			putI( "twiddling", 1 );
		}

		if( zmsgIs(type,FitterEigenTwiddleStop) ) {
			putI( "twiddling", 0 );
		}
	}

};

ZUI_IMPLEMENT(ZUIFitterParameterAdjustPlot,ZUIPanel);


struct ZUIFitterFFTNoisePlot : ZUIPanel {
	ZUI_DEFINITION(ZUIFitterFFTNoisePlot,ZUIPanel);

	virtual void render() {
		ZUIPanel::render();
		KineticFitter *fitter = (KineticFitter *)getI( "fitter" );
		if( fitter ) {
			float myW, myH;
			getWH( myW, myH );

			int eo = 0;
			for( int e=0; e<fitter->eCount(); e++ ) {
				for( int o=0; o<fitter->experiments[e]->observableResidualFFTMat.rows; o++, eo++ ) {
					glColor3fv( colors[eo] );
					glBegin( GL_LINE_STRIP );
					int cols = fitter->experiments[e]->observableResidualFFTMat.cols;
					for( int i=0; i<cols; i++ ) {
						float x = (float)i * myW / (float)cols;
						float y = (float)fitter->experiments[e]->observableResidualFFTMat.getD( o, i );
						glVertex2f( x, 30.f * logf( y+1.f ) + 2.f );
					}
					glEnd();
				}
			}			
		}
	}
};

ZUI_IMPLEMENT(ZUIFitterFFTNoisePlot,ZUIPanel);


// Test code
//------------------------------------------------------------------------------------------

KineticSystem testSystem;
KineticSystem startSystem;
KineticSystem twiddleSystem;

KineticFitter *testFitter = 0;

void regressionTestStateCheck();


void render() {

// @TODO: puysh pop these
	#ifdef WIN32
		_clearfp();
		#ifdef _DEBUG
			_controlfp( _MCW_EM, _MCW_EM );
				// These calls set the floating point unit to exception on errors
		#endif
	#endif


	int e, o, i;//, j;

	regressionTestStateCheck();

	zviewpointSetupView();

	static int lastClosestPlot = -1;

	// BUILD the array of plots
	TracePlot *plots[ 1024 ] = {0,};
	int plotCount = 0;
	
	for( e=0; e<testSystem.eCount(); e++ ) {
		for( o=0; o<testSystem[e]->observablesCount(); o++ ) {
			// PLOT observable trace

			plots[plotCount++] = new TracePlot( &testSystem[e]->traceOC, o, o,  KTP_STYLE_SLERP | KTP_STYLE_LINE_SOLID | KTP_STYLE_POINTS_BLACK_DOTS, ZTmpStr("Ex %d: Obs: %d",e,o) );

			// PLOT measured data associated with this observable if any
			if( o < testSystem[e]->measured.count ) {
				plots[plotCount++] = new TracePlot( testSystem[e]->measured[o], o, o,  KTP_STYLE_SLERP | KTP_STYLE_POINTS_COLOR_MARKS, ZTmpStr("Meas Ex %d: Obs: %d",e,o) );
			}
		}
	}

	if( testFitter ) {
		testFitter->renderVariablesLock();
		
		for( e=0; e<testFitter->eCount(); e++ ) {
			for( o=0; o<testFitter->experiments[e]->observablesCount(); o++ ) {
				plots[plotCount++] = new TracePlot( &testFitter->experiments[e]->render_traceOC, o, o,  KTP_STYLE_SLERP | KTP_STYLE_LINE_SOLID | KTP_STYLE_POINTS_BLACK_DOTS | KTP_STYLE_LINE_HALO, ZTmpStr("Fit Ex %d: Obs: %d",e,o) );
			}

			for( i=0; i<testFitter->experiments[e]->render_traceC.rows; i++ ) {
				plots[plotCount++] = new TracePlot( &testFitter->experiments[e]->render_traceC, i, i,  KTP_STYLE_SLERP | KTP_STYLE_LINE_DASHED | KTP_STYLE_POINTS_BLACK_DOTS, ZTmpStr("Fit Ex %d: traceC[%d]",e,i) );
			}

//			if( Aode_plotOGWRT >= 0 && Aode_plotOGWRT < testFitter->experiments[e]->render_traceG.count ) {
//				int gCount = testFitter->experiments[e]->render_traceOG.count;
//				if( gCount >= 0 ) {
//					for( j=0; j<testFitter->experiments[e]->render_traceOG[Aode_plotOGWRT % gCount]->rows; j++ ) {
//						plots[plotCount++] = TracePlot( testFitter->experiments[e]->render_traceOG[Aode_plotOGWRT % gCount], j, j,  KTP_STYLE_LINE_DOTTED | KTP_STYLE_POINTS_BLACK_DOTS, ZTmpStr("Fit Ex %d: (traceG wrt %d)[%d]",e,Aode_plotOGWRT,j) );
//					}
//				}
//			}

			if( Aode_plotDebugTraces ) {
				for( i=0; i<testFitter->experiments[e]->render_traceDebug.rows; i++ ) {
					plots[plotCount++] = new TracePlot( &testFitter->experiments[e]->render_traceDebug, i, i,  KTP_STYLE_SLERP | KTP_STYLE_LINE_DOTDASHED, ZTmpStr("Fit Ex %d: debug[%d]",e,i) );
				}
			}
		}

		testFitter->renderVariablesUnlock();
	}

	for( e=0; e<twiddleSystem.eCount(); e++ ) {
		for( o=0; o<twiddleSystem[e]->observablesCount(); o++ ) {
			plots[plotCount++] = new TracePlot( &twiddleSystem[e]->traceOC, o, o,  KTP_STYLE_SLERP | KTP_STYLE_LINE_DASHED | KTP_STYLE_POINTS_BLACK_DOTS | KTP_STYLE_LINE_HALO, ZTmpStr("Twiddle Ex %d: Obs: %d",e,o) );
		}
	}

	if( lastClosestPlot >= 0 && lastClosestPlot < plotCount ) {
		// This is a bit of a hack because the plots could be changing and I'd be referencing 
		// the wrong one but for now it will do.
		plots[lastClosestPlot]->flags |= KTP_STYLE_LINE_BOLD;
	}

	assert( plotCount < sizeof(plots)/sizeof(plots[0]) );

	if( Aode_autoscale ) {
		glLoadIdentity();
		double minT=+DBL_MAX, maxT=-DBL_MAX, minY=+DBL_MAX, maxY=-DBL_MAX;
		for( i=0; i<plotCount; i++ ) {
			if( Aode_autoscale!=2 || plots[i]->flags & KTP_STYLE_LINE_SOLID ) {
				plots[i]->trace->getBounds( minT, maxT, minY, maxY, 0 );
			}
		}
		glTranslatef( -1.0, -1.0, 0.0 );
		if( maxY == 0.0 ) {
			maxY = 1.0;
		}
		glScaled( 2.0 / maxT, 2.0 / maxY, 1.0 );
		zviewpointSetupView();
	}

	plotUsingViewpointGridlinesFullscreen();

	kineticTracePlotArray( plots, plotCount );

	// MONITOR mouse movement
	static FVec2 lastMousePos;
	static ZTimer mouseMoveTimer;
	FVec2 mousePos = zviewpointProjMouseOnZ0Plane();
	FVec2 deltaMousePos = mousePos;
	deltaMousePos.sub( lastMousePos );
	FVec2 *checkMousePos = 0;
	if( deltaMousePos.mag2() == 0.f ) {
		// If mouse hasn't moved then check timers
		if( mouseMoveTimer.isRunning() ) {
			if( mouseMoveTimer.isDone() ) {
				// CHECK for plot collision
				lastClosestPlot = kineticTracePlotCollisionDetect( plots, plotCount, mousePos );
				mouseMoveTimer.stop();
			}
		}
	}
	else {
		// Mouse has moved, reset timers
		lastClosestPlot = -1;
		mouseMoveTimer.start( 0.1 );
	}
	lastMousePos = mousePos;

	if( lastClosestPlot >= 0 && lastClosestPlot < plotCount ) {
		// PRINT the name
		glPushMatrix();
			DMat4 projMat;
			DMat4 modelViewMat;
			int viewportI[4];
			glGetIntegerv( GL_VIEWPORT, viewportI );
			glGetDoublev( GL_PROJECTION_MATRIX, projMat );
			glGetDoublev( GL_MODELVIEW_MATRIX, modelViewMat );
			DVec3 win;
			gluProject( (double)mousePos.x, (double)mousePos.y, 0.f, modelViewMat, projMat, viewportI, &win.x, &win.y, &win.z );

			zglPixelMatrixFirstQuadrant();

			glColor3ub( 0, 0, 0 );
			zglFontPrint( plots[lastClosestPlot]->name, (float)win.x+1.f, (float)win.y-1.f, "controls" );
			glColor3ub( 255, 255, 255 );
			zglFontPrint( plots[lastClosestPlot]->name, (float)win.x, (float)win.y, "controls" );
		glPopMatrix();
	}
	
	for( i=0; i<plotCount; i++ ) {
		delete plots[i];
	}
}

// Experimental tests
//------------------------------------------------------------------------------------------

int (*setupCallback)(int) = 0;

int setupTest0( int modify ) {
	setupCallback = setupTest0;
	// A+B=C, 1 experiment, 1 observable, modify rate const, fit rate consts

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAdd(  0,  1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  0,  1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A * f1") );

	testSystem.allocParameterInfo();

	KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
	p->value = 2.0;
	testSystem.compile();

	testSystem.paramSetReactionRate(0,10);
	testSystem.paramSetReactionRate(1,1);
	testSystem.paramSetIC(0,0,3.0);
	testSystem.paramSetIC(0,1,1.0);
	testSystem.paramSetIC(0,2,0.0);

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}

int setupTest1( int modify ) {
	setupCallback = setupTest1;
	// A+B=I=C, 1 experiment, 1 observable, modify rate const, fit rate consts

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
//		testSystem.paramSetReactionRate( 0, 9.0 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "I" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay( "A", "B", "I", 0 );
	testSystem.reactionAddByNamesTwoWay( "I", 0, "C", 0 );


	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("C") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetReactionRate( 2, 30 );
	testSystem.paramSetReactionRate( 3, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );
	testSystem.paramSetIC( 0, 3, 0.0 );

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}

int setupTest2( int modify ) {
	setupCallback = setupTest2;
	// A+B=C, 1 experiment, 3 observables, modify rate const, fit all

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay(  "A", "B", "C", 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );
	e->observableInstructions.add( strdup("B") );
	e->observableInstructions.add( strdup("C") );

	testSystem.allocParameterInfo();
	testSystem.compile();
	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );

	testSystem.setFitFlags();
	return 0;
}


int setupTest3( int modify ) {
	setupCallback = setupTest3;
	// A+B=C, 1 experiment, 3 observables, modify init cond, fit all

	if( modify ) {
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay(  "A", "B", "C", 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );
	e->observableInstructions.add( strdup("B") );
	e->observableInstructions.add( strdup("C") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );

	testSystem.setFitFlags();
	return 0;
}


int setupTest4( int modify ) {
	setupCallback = setupTest4;
	// A+B=C, 1 experiment, 3 observables, modify rate consts and init cond, fit all

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		testSystem.paramSetReactionRate( 1, startSystem.paramGetReactionRate(1) * 0.5 );
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
			testSystem.paramSetIC( i, 1, startSystem.paramGetIC( i, 1 ) * 0.5 );
			testSystem.paramSetIC( i, 2, startSystem.paramGetIC( i, 2 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay(  "A", "B", "C", 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );
	e->observableInstructions.add( strdup("B") );
	e->observableInstructions.add( strdup("C") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );

	testSystem.setFitFlags();
	return 0;
}


int setupTest5( int modify ) {
	setupCallback = setupTest5;
	// A+B=C, 2 experiments, 6 observables, modify rate 1 consts, fit 1 reaction rate

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay( "A", "B", "C", 0 );

	KineticExperiment *e;

	// Experiment 0
	e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );
	e->observableInstructions.add( strdup("B") );
	e->observableInstructions.add( strdup("C") );

	// Experiment 1
	e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );
	e->observableInstructions.add( strdup("B") );
	e->observableInstructions.add( strdup("C") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 2.0 );
	testSystem.paramSetIC( 0, 1, 0.5 );
	testSystem.paramSetIC( 0, 2, 0.0 );
	testSystem.paramSetIC( 1, 0, 3.0 );
	testSystem.paramSetIC( 1, 1, 1.0 );
	testSystem.paramSetIC( 1, 2, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi->fitFlag = 1;
	return 0;
}

int setupTest6( int modify ) {
	setupCallback = setupTest6;
	// A+B=C, 1 experiment, 1 observable, modify 1 rate consts and 1 init cond, fit 1 rate const, 1 init cond

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay( "A", "B", "C", 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("C") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi[0].fitFlag = 1;
	pi = testSystem.paramGet( PI_INIT_COND );
	pi[0].fitFlag = 1;
	return 0;
}

int setupTest7( int modify ) {
	setupCallback = setupTest7;
	// A=B, 1 experiment, 1 observable, modify 1 rate consts and 1 init cond, fit 1 rate const, 1 init cond

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );
	testSystem.reactionAdd(  1, -1,  0, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi->fitFlag = 1;
	pi = testSystem.paramGet( PI_INIT_COND );
	pi->fitFlag = 1;
	return 0;
}


int setupTest8( int modify ) {
	setupCallback = setupTest8;
	// A=B, 2 experiments, 2 observables, modify 1 rate consts and 1 init cond, fit 1 rate const, 1 init cond

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );
	testSystem.reactionAdd(  1, -1,  0, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetReactionRate( 1, 1 );
	testSystem.paramSetIC( 0, 0, 3.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 1, 0, 10.0 );
	testSystem.paramSetIC( 1, 1,  9.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi->fitFlag = 1;
	pi = testSystem.paramGet( PI_INIT_COND );
	pi[0].fitFlag = 1;
	pi[1].fitFlag = 1;
	return 0;
}


int setupTest9( int modify ) {
	setupCallback = setupTest9;
	// A->B, 1 experiments, 1 observables, modify 1 rate consts, fit 1 rate const

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi->fitFlag = 1;
	return 0;
}


int setupTest10( int modify ) {
	setupCallback = setupTest10;
	// A->B, 1 experiments, 1 observables, modify 1 init cond, fit 1 init cond

	if( modify ) {
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_INIT_COND );
	pi->fitFlag = 1;
	return 0;
}


int setupTest11( int modify ) {
	setupCallback = setupTest11;
	// A->B, 2 experiments, 2 observables, modify 1 rate const, fit 1 rate const

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );
	testSystem.paramSetIC( 1, 0, 0.5 );
	testSystem.paramSetIC( 1, 1, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi->fitFlag = 1;
	return 0;
}


int setupTest12( int modify ) {
	setupCallback = setupTest12;
	// A->B, 2 experiments, 2 observables, modify 1 init cond, fit 1 init cond

	if( modify ) {
		testSystem.paramSetIC( 0, 0, startSystem.paramGetIC( 0, 0 ) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );
	testSystem.paramSetIC( 1, 0, 0.3 );
	testSystem.paramSetIC( 1, 1, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_INIT_COND );
	pi->fitFlag = 1;
	return 0;
}


int setupTest13( int modify ) {
	setupCallback = setupTest13;
	// A->B, 2 experiments, 2 observables, modify 2 init cond, fit 2 init conds

	if( modify ) {
		for( int i=0; i<testSystem.eCount(); i++ ) {
			testSystem.paramSetIC( i, 0, startSystem.paramGetIC( i, 0 ) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0, -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );
	testSystem.paramSetIC( 1, 0, 0.3 );
	testSystem.paramSetIC( 1, 1, 0.0 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_INIT_COND, 0, 0 );
	pi->fitFlag = 1;
	pi = testSystem.paramGet( PI_INIT_COND, 0, 1 );
	pi->fitFlag = 1;
	return 0;
}


int setupTest14( int modify ) {
	setupCallback = setupTest14;
	// EPSP, 1 experiment, modify 1 rate const, fit 1 rate const
	//	 0: E+A = EA
	//	 2: EA+B = EAB
	//	 4: EAB=EI
	//	 6: EI=EPQ
	//	 8: EPQ=EQ+P
	//	10: EQ=E+Q
	//	12: EA+P=EAP

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "E" );
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "EA" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "EAB" );
	testSystem.reagentAdd( "EI" );
	testSystem.reagentAdd( "EPQ" );
	testSystem.reagentAdd( "EQ" );
	testSystem.reagentAdd( "P" );
	testSystem.reagentAdd( "Q" );
	testSystem.reagentAdd( "EAP" );

	testSystem.reactionAdd(  0,  1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  0,  1 );
	testSystem.reactionAdd(  2,  3,  4, -1 );
	testSystem.reactionAdd(  4, -1,  2,  3 );
	testSystem.reactionAdd(  4, -1,  5, -1 );
	testSystem.reactionAdd(  5, -1,  4, -1 );
	testSystem.reactionAdd(  5, -1,  6, -1 );
	testSystem.reactionAdd(  6, -1,  5, -1 );
	testSystem.reactionAdd(  6, -1,  7,  8 );
	testSystem.reactionAdd(  7,  8,  6, -1 );
	testSystem.reactionAdd(  7, -1,  0,  9 );
	testSystem.reactionAdd(  0,  9,  7, -1 );
	testSystem.reactionAdd(  2,  8, 10, -1 );
	testSystem.reactionAdd( 10, -1,  2,  8 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("E") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 342 );
	testSystem.paramSetReactionRate(  1, 2370 );
	testSystem.paramSetReactionRate(  2, 12 );
	testSystem.paramSetReactionRate(  3, 197 );
	testSystem.paramSetReactionRate(  4, 1490 );
	testSystem.paramSetReactionRate(  5, 124 );
	testSystem.paramSetReactionRate(  6, 176 );
	testSystem.paramSetReactionRate(  7, 132 );
	testSystem.paramSetReactionRate(  8, 241 );
	testSystem.paramSetReactionRate(  9, 6.75 );
	testSystem.paramSetReactionRate( 10, 66.3 );
	testSystem.paramSetReactionRate( 11, 66.3 );
	testSystem.paramSetReactionRate( 12, 0.142 );
	testSystem.paramSetReactionRate( 13, 100 );
	testSystem.paramSetIC( 0, 0, 10 );
	testSystem.paramSetIC( 0, 8, 7500 );
	testSystem.paramSetIC( 0, 9, 2.1 );

	testSystem.clrFitFlags();
	KineticParameterInfo *pi = testSystem.paramGet( PI_REACTION_RATE );
	pi->fitFlag = 1;
	return 0;
}


int setupTest15( int modify ) {
	setupCallback = setupTest15;
	// EPSP, 1 experiment, modify all rate consts, fit rate consts
	//	 0: E+A = EA
	//	 2: EA+B = EAB
	//	 4: EAB=EI
	//	 6: EI=EPQ
	//	 8: EPQ=EQ+P
	//	10: EQ=E+Q
	//	12: EA+P=EAP

	if( modify ) {
		for( int j=0; j<startSystem.reactionCount(); j++ ) {
			testSystem.paramSetReactionRate( j, startSystem.paramGetReactionRate(j) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "E" );
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "EA" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "EAB" );
	testSystem.reagentAdd( "EI" );
	testSystem.reagentAdd( "EPQ" );
	testSystem.reagentAdd( "EQ" );
	testSystem.reagentAdd( "P" );
	testSystem.reagentAdd( "Q" );
	testSystem.reagentAdd( "EAP" );

	testSystem.reactionAdd(  0,  1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  0,  1 );
	testSystem.reactionAdd(  2,  3,  4, -1 );
	testSystem.reactionAdd(  4, -1,  2,  3 );
	testSystem.reactionAdd(  4, -1,  5, -1 );
	testSystem.reactionAdd(  5, -1,  4, -1 );
	testSystem.reactionAdd(  5, -1,  6, -1 );
	testSystem.reactionAdd(  6, -1,  5, -1 );
	testSystem.reactionAdd(  6, -1,  7,  8 );
	testSystem.reactionAdd(  7,  8,  6, -1 );
	testSystem.reactionAdd(  7, -1,  0,  9 );
	testSystem.reactionAdd(  0,  9,  7, -1 );
	testSystem.reactionAdd(  2,  8, 10, -1 );
	testSystem.reactionAdd( 10, -1,  2,  8 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("E") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 342 );
	testSystem.paramSetReactionRate(  1, 2370 );
	testSystem.paramSetReactionRate(  2, 12 );
	testSystem.paramSetReactionRate(  3, 197 );
	testSystem.paramSetReactionRate(  4, 1490 );
	testSystem.paramSetReactionRate(  5, 124 );
	testSystem.paramSetReactionRate(  6, 176 );
	testSystem.paramSetReactionRate(  7, 132 );
	testSystem.paramSetReactionRate(  8, 241 );
	testSystem.paramSetReactionRate(  9, 6.75 );
	testSystem.paramSetReactionRate( 10, 66.3 );
	testSystem.paramSetReactionRate( 11, 66.3 );
	testSystem.paramSetReactionRate( 12, 0.142 );
	testSystem.paramSetReactionRate( 13, 100 );
	testSystem.paramSetIC( 0, 0, 10 );
	testSystem.paramSetIC( 0, 8, 7500 );
	testSystem.paramSetIC( 0, 9, 2.1 );

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}


int setupTest16( int modify ) {
	// "irreversible"
	// E + S = ES
	// E = EX
	// 1 experiment, 1 observable, modify all rate consts, fit all rate consts
	setupCallback = setupTest16;

	if( modify ) {
		for( int j=0; j<startSystem.reactionCount(); j++ ) {
			testSystem.paramSetReactionRate( j, startSystem.paramGetReactionRate(j) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "ES" );
	testSystem.reagentAdd( "S" );
	testSystem.reagentAdd( "E" );
	testSystem.reagentAdd( "EX" );

	testSystem.reactionAdd(  2,  1,  0, -1 );
	testSystem.reactionAdd(  0, -1,  2,  1 );
	testSystem.reactionAdd(  0, -1,  3, -1 );
	testSystem.reactionAdd(  3, -1,  0, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("ES") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 20.0 );
	testSystem.paramSetReactionRate(  1, 0.0 );
	testSystem.paramSetReactionRate(  2, 50.0 );
	testSystem.paramSetReactionRate(  3, 0.0 );
	testSystem.paramSetIC( 0, 0, 0.0 );
	testSystem.paramSetIC( 0, 1, 2.0 );
	testSystem.paramSetIC( 0, 2, 1.0 );
	testSystem.paramSetIC( 0, 3, 0.0 );

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}


int setupTest17( int modify ) {
	setupCallback = setupTest17;
		// A=B=C, 1 experiment, 1 observables

	if( modify ) {
		return KE_DONE_TRAPPED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAdd(  0, -1,  1, -1 );
	testSystem.reactionAdd(  1, -1,  0, -1 );

	testSystem.reactionAdd(  1, -1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("A") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 10.0 );
	testSystem.paramSetReactionRate(  1,  5.0 );
	testSystem.paramSetReactionRate(  2,  2.0 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 2.0 );
	testSystem.paramSetIC( 0, 2, 3.0 );

	testSystem.clrFitFlags();
	return 0;
}


int setupTest18( int modify ) {
	setupCallback = setupTest18;
		// S+C=I1+SB
		// I1+F=I2+OB
		// I2=W+C
		// S+F=OB+W
		// S+C=X

	if( modify ) {
		for( int j=0; j<startSystem.reactionCount(); j++ ) {
			testSystem.paramSetReactionRate( j, startSystem.paramGetReactionRate(j) * 0.5 );
		}
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "S" ); // 0
	testSystem.reagentAdd( "C" ); // 1
	testSystem.reagentAdd( "I1" );// 2
	testSystem.reagentAdd( "SB" );// 3
	testSystem.reagentAdd( "F" ); // 4
	testSystem.reagentAdd( "I2" );// 5
	testSystem.reagentAdd( "OB" );// 6
	testSystem.reagentAdd( "W" ); // 7
	testSystem.reagentAdd( "X" ); // 8

	testSystem.reactionAddByNamesTwoWay( "S", "C", "I1", "SB" );
	testSystem.reactionAddByNamesTwoWay( "I1", "F", "I2", "OB" );
	testSystem.reactionAddByNamesTwoWay( "I2", 0, "W", "C" );
	testSystem.reactionAddByNamesTwoWay( "S", "F", "OB", "W" );
	testSystem.reactionAddByNamesTwoWay( "S", "C", "X", 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 100000.0;

	e->observableInstructions.add( strdup("OB * o1") );

//	testSystem.observableConstants[0] = 1e14;

//	e->measured[0]->init( 1 );
//	FILE *f = fopen( "/transfer/david/001m.txt", "rt" );
//	char line[256];
//	int lineNum = 0;
//	double t = 0.0;
//	while( fgets( line, sizeof(line), f ) ) {
//		if( lineNum > 0 ) {
//			int field = 0;
//			char *tok = strtok( line, " \t\n\r" );
//			while( tok ) {
//				if( field == 0 ) {
//					t = atof( tok );
//				}
//				else if( field == 4 ) {
//					double val = atof( tok );
//					e->measured[0]->add( t, &val );
//				}
//				tok = strtok( 0, " \t\n\r" );
//				field++;
//			}
//		}
//		lineNum++;
//	}
//	fclose( f );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 1e5 );
	testSystem.paramSetReactionRate(  1, 1e5 );
	testSystem.paramSetReactionRate(  2, 1e5 );
	testSystem.paramSetReactionRate(  3, 1 );
	testSystem.paramSetReactionRate(  4, 1e-2 );
	testSystem.paramSetReactionRate(  5, 1e5 );
	testSystem.paramSetReactionRate(  6, 1e1 );
	testSystem.paramSetReactionRate(  7, 1e-3 );
	testSystem.paramSetReactionRate(  8, 1e2 );
	testSystem.paramSetReactionRate(  9, 1e-3 );
	testSystem.paramSetIC( 0, 0, 1e-9 );
	testSystem.paramSetIC( 0, 1, 1e-9 );
	testSystem.paramSetIC( 0, 2, 0.0 );
	testSystem.paramSetIC( 0, 3, 0.0 );
	testSystem.paramSetIC( 0, 4, 2e-9 );
	testSystem.paramSetIC( 0, 5, 0.0 );
	testSystem.paramSetIC( 0, 6, 0.0 );
	testSystem.paramSetIC( 0, 7, 0.0 );

	testSystem.setFitFlags();
	return 0;
}

int setupTest19( int modify ) {
	setupCallback = setupTest19;
	// A+A=B, Test of dimerization

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0,  0,  1, -1 );
	testSystem.reactionAdd(  1,  -1,  0, 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 10.0;

	e->observableInstructions.add( strdup("B") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 10 );
	testSystem.paramSetReactionRate(  1, 1 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}


int setupTest20( int modify ) {
	setupCallback = setupTest20;
	// A+B=C, 

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAddByNamesTwoWay( "A", "B", "C", 0 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 10.0;

	e->observableInstructions.add( strdup("C * o1 + o0") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 10 );
	testSystem.paramSetReactionRate(  1, 0 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );
	KineticParameterInfo *pi = testSystem.paramGetByName( "o1" );
	pi->value = 2.0;
	pi = testSystem.paramGetByName( "o0" );
	pi->value = 1.0;

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}

int setupTest21( int modify ) {
	setupCallback = setupTest21;
	// Testing fitting an obersvable constant

	if( modify ) {
		KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
		p->value *= 0.5;
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAdd(  0,  1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  0,  1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("A * f1") );

	testSystem.allocParameterInfo();

	KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
	p->value = 2.0;
	testSystem.compile();

	testSystem.paramSetReactionRate(0,10);
	testSystem.paramSetReactionRate(1,1);
	testSystem.paramSetIC(0,0,3.0);
	testSystem.paramSetIC(0,1,1.0);
	testSystem.paramSetIC(0,2,0.0);

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_OBS_CONST );
	return 0;
}

int setupTest22( int modify ) {
	setupCallback = setupTest22;
	// Testing fitting an obersvable constant with a complex expression

	if( modify ) {
		KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
		p->value *= 0.5;
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAdd(  0,  1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  0,  1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("f1 * B") );
// @TODO: When 	f1 * B it converges slowly but is ok when f1 * (A+B)

	testSystem.allocParameterInfo();

	KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
	p->value = 2.0;
	testSystem.compile();

	testSystem.paramSetReactionRate(0,10);
	testSystem.paramSetReactionRate(1,1);
	testSystem.paramSetIC(0,0,3.0);
	testSystem.paramSetIC(0,1,1.0);
	testSystem.paramSetIC(0,2,0.0);

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_OBS_CONST );
	return 0;
}


int setupTest23( int modify ) {
	setupCallback = setupTest23;
	// Testing fitting an obersvable constant with a complex expression

	if( modify ) {
		KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
		p->value = 2.0;
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0,  -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;
	e->observableInstructions.add( strdup("f1 * B") );

	testSystem.allocParameterInfo();

	KineticParameterInfo *p = testSystem.paramGetByName( "f1" );
	p->value = 4.0;
	testSystem.compile();

	testSystem.paramSetReactionRate(0,1.0);
	testSystem.paramSetIC(0,0,3.0);
	testSystem.paramSetIC(0,1,2.0);

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_OBS_CONST );
	return 0;
}


int setupTest24( int modify ) {
	setupCallback = setupTest24;
	// Testing observables with time references

	if( modify ) {
		testSystem.paramSetReactionRate( 0, startSystem.paramGetReactionRate(0) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0,  0,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("B") );
	e->observableInstructions.add( strdup("B[end]") );
	e->observableInstructions.add( strdup("B[0.5]") );

//	e->observableInstructions.add( strdup("B[0] + 0.1") );
// @TODO: This constant should either work or error?

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 10 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );

	testSystem.clrFitFlags();
	testSystem.setFitFlags( PI_REACTION_RATE );
	return 0;
}

int setupTest25( int modify ) {
	setupCallback = setupTest25;
		// Testing observables identity expressions

	KineticParameterInfo *p;
	if( modify ) {
		p = testSystem.paramGetByName( "f0" );
		p->value = 0.25;
		p = testSystem.paramGetByName( "f1" );
		p->value = 1.0;
		p = testSystem.paramGetByName( "f2" );
		p->value = 1.0;
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAdd(  0,  -1,  1, -1 );
	testSystem.reactionAdd(  1,  -1,  2, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("f0+f1*f2*A*B") );
	e->observableInstructions.add( strdup("f0+f1*(A+B)*(A+B)-f1*A*A-f1*B*B") );

	testSystem.allocParameterInfo();
	p = testSystem.paramGetByName( "f0" );
	p->value = 0.5;
	p = testSystem.paramGetByName( "f1" );
	p->value = 2.0;
	p = testSystem.paramGetByName( "f2" );
	p->value = 2.0;

	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 10.0 );
	testSystem.paramSetReactionRate( 1, 3.0 );
	testSystem.paramSetIC( 0, 0, 5.0 );
	testSystem.paramSetIC( 0, 1, 0.0 );
	testSystem.paramSetIC( 0, 2, 0.0 );

	testSystem.clrFitFlags();
	p = testSystem.paramGetByName( "f0" );
	p->fitFlag = 1;
	p = testSystem.paramGetByName( "f1" );
	p->fitFlag = 1;
	p = testSystem.paramGetByName( "f2" );
	p->fitFlag = 1;
	return 0;
}

int setupTest26( int modify ) {
	setupCallback = setupTest26;

	KineticParameterInfo *p;
	if( modify ) {
		p = testSystem.paramGetByName( "B" );
		p->value = 2.0;
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );

	testSystem.reactionAdd(  0,  -1,  1, -1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->mixsteps[0].duration = 1.0;

	e->observableInstructions.add( strdup("B") );

	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate( 0, 0.0 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 5.0 );

	testSystem.clrFitFlags();
	p = testSystem.paramGetByName( "B" );
	p->fitFlag = 1;
	return 0;
}

int setupTest27( int modify ) {

	// @TODO make the regression and this thing set up the scalar val of the measurement basedon the last val of the integration

	setupCallback = setupTest27;
	// A+B=C, 1 EQUILIBRIUM experiment, 1 observables

	if( modify ) {
		testSystem.paramSetIC( 0, 0, startSystem.paramGetIC( 0, 0 ) * 0.5 );
		testSystem.paramSetIC( 0, 1, startSystem.paramGetIC( 0, 1 ) * 0.5 );
		testSystem.paramSetIC( 0, 2, startSystem.paramGetIC( 0, 2 ) * 0.5 );
		return KE_DONE_CONVERGED;
	}

	testSystem.clear();
	testSystem.reagentAdd( "A" );
	testSystem.reagentAdd( "B" );
	testSystem.reagentAdd( "C" );

	testSystem.reactionAdd(  0, 1,  2, -1 );
	testSystem.reactionAdd(  2, -1,  0, 1 );

	KineticExperiment *e = testSystem.newExperiment();
	e->observableInstructions.add( strdup("C/A") );
	int obsCount = testSystem.reagentCount() + e->observableInstructions.count;
	assert( obsCount == 4 );
	e->equilibriumExperiment = 1;
	e->equilibriumVariance.alloc( obsCount, 1, zmatF64 );
	e->equilibriumVariance.putD( 0, 0, 0.05 );	
	e->equilibriumVariance.putD( 1, 0, 0.0 );	
	e->equilibriumVariance.putD( 2, 0, 0.0 );	
	e->equilibriumVariance.putD( 3, 0, 0.0 );	

	for( int i=0; i<obsCount; i++ ) {
		KineticTrace *kt = new KineticTrace( 1, 1 );
		double equil = i==0 ? 2.0 : 0.0;
		kt->add( 0.0, &equil );
		e->measured.add( kt );
	}
	
	testSystem.allocParameterInfo();
	testSystem.compile();

	testSystem.paramSetReactionRate(  0, 2.0 );
	testSystem.paramSetReactionRate(  1, 1.0 );
	testSystem.paramSetIC( 0, 0, 1.0 );
	testSystem.paramSetIC( 0, 1, 1.0 );
	testSystem.paramSetIC( 0, 2, 2.0 );

	testSystem.setFitFlags();
	return 0;
}


//-----------------------------------------------------------------------------------------------


void generateErrorSurface() {
	setupTest1(0);

	testSystem.simulate();
	for( int i=0; i<testSystem.eCount(); i++ ) {
		testSystem[i]->measuredCreateFake( 1000, Aode_noise );
	}

	startSystem.copy( testSystem );

	int samples = 50;

//	double r0_0 = startExpList[0]->reactionRates[0] * 0.0;
//	double r0_1 = startExpList[0]->reactionRates[0] * 8.0;
//	double r0_s = ( r0_1 - r0_0 ) / (double)samples;

//	double r1_0 = startExpList[0]->initCond[0] * 0.0;
//	double r1_1 = startExpList[0]->initCond[0] * 8.0;
//	double r1_s = ( r1_1 - r1_0 ) / (double)samples;

	double r0_0 = startSystem.paramGetReactionRate( 0 ) * 0.0;
	double r0_1 = startSystem.paramGetReactionRate( 0 ) * 5.0;
	double r0_s = ( r0_1 - r0_0 ) / (double)samples;

	double r1_0 = startSystem.paramGetReactionRate( 1 ) * 0.0;
	double r1_1 = startSystem.paramGetReactionRate( 1 ) * 5.0;
	double r1_s = ( r1_1 - r1_0 ) / (double)samples;

	// This pass system permits getting both the trajectory and the values at the first point
	// This is propbably not optimal but I only need to run this a few tims for illustration
	for( int pass=1; pass<2; pass++ ) {
		double r0 = r0_0;
		for( int r0i=0; r0i<samples; r0 += r0_s, r0i++ ) {
			double r1 = r1_0;
			for( int r1i=0; r1i<samples; r1 += r1_s, r1i++ ) {
				printf( "%d %d\n", r0i, r1i );

				// COPY of the original experiment
				testSystem.copy( startSystem );

				testSystem.paramSetReactionRate( 0, r0 );
				testSystem.paramSetReactionRate( 1, r1 );
//				for( int e=0; e<startSystem.eCount(); e++ ) {
//					testSystem[e]->initCond[0] = r1;
//				}

				// RUN sampleErrorSurface on this dup experiment
//				testFitter.eList.copy( testExpList );
//
//				testFitter.fit( pass );
//
//				if( pass == 1 ) {
//					FILE *f = fopen( ZTmpStr("/transfer/fitsurf/fitsurf-point-%02d-%02d.txt", r0i, r1i), "wt" );
//					fprintf( f, "%d %d %lf %lf %lf\n", r0i, r1i, r0, r1, testFitter.fitError );
//					fclose( f );
//
//					testFitter.fitGrad.saveCSV( ZTmpStr("/transfer/fitsurf/fitsurf-grad-%02d-%02d.txt", r0i, r1i) );
//					testFitter.fitHess.saveCSV( ZTmpStr("/transfer/fitsurf/fitsurf-hess-%02d-%02d.txt", r0i, r1i) );
//					testFitter.fitU.saveCSV( ZTmpStr("/transfer/fitsurf/fitsurf-u-%02d-%02d.txt", r0i, r1i) );
//					testFitter.fitS.saveCSV( ZTmpStr("/transfer/fitsurf/fitsurf-s-%02d-%02d.txt", r0i, r1i) );
//				}
//				if( pass == 0 ) {
//					testFitter.fitTrajectory.saveCSV( ZTmpStr("/transfer/fitsurf/fitsurf-trajectory-%02d-%02d.txt", r0i, r1i) );
//				}
//
//				ZMat vt;
//				zmatTranspose( testFitter.fitVtt, vt );
//				vt.saveCSV( ZTmpStr("/transfer/fitsurf/fitsurf-vt-%02d-%02d.txt", r0i, r1i) );
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------


typedef int (*RegressionSetupFn)(int modify);

RegressionSetupFn regressionTestFns[] = {
	setupTest0,
	setupTest1,
	setupTest2,
	setupTest3,
	setupTest4,
	setupTest5,
	setupTest6,
	setupTest7,
	setupTest8,
	setupTest9,
	setupTest10,
	setupTest11,
	setupTest12,
	setupTest13,
	setupTest14,
	setupTest15,
	setupTest16,
	setupTest17,
	setupTest18,
	setupTest19,
	setupTest20,
	setupTest21,
	setupTest22,
	setupTest23,
	setupTest24,
	setupTest25,
	setupTest26,
	//setupTest100, // equilibirum
};

int regressionTestI = -1;
//#define REGESSION

void regressionTestStateCheck() {
#ifndef REGESSION
return;
#endif

	int i;
	int stateChange = 0;
	
	if( regressionTestI == -1 ) {
		stateChange = 1;
	}

	// check for the fitter to complete
	if( testFitter && (testFitter->fitThreadStatus == KE_DONE_CONVERGED || testFitter->fitThreadStatus == KE_DONE_TRAPPED || testFitter->fitThreadStatus == KE_DONE_CANCELLED) ) {
		// ASK the test what the correct result should be
		printf(
			"Test %d stop. propCount=%d, origCount=%d, Result status=%d, %s\n",
			regressionTestI,
			testFitter->propCount,
			testFitter->origCount,
			testFitter->fitThreadStatus,
			(*regressionTestFns[regressionTestI])(1) == testFitter->fitThreadStatus ? "PASS" : "FAIL"
		);
		printf( "Press any key...\n" );
		zconsoleGetch();

		stateChange = 1;
	}
			
	if( stateChange ) {
		if( testFitter ) {
			delete testFitter;
			testFitter = 0;
			ZUI *z = ZUI::zuiFindByName( "aodeFitterPanel" );
			z->putP( "fitter", testFitter );
			z->putP( "*fitter", testFitter );
		}

		testSystem.clear();

		regressionTestI++;
		if( regressionTestI >= sizeof(regressionTestFns) / sizeof(regressionTestFns[0]) ) {
			return;
		}
		
		printf( "Test %d start.\n", regressionTestI );

		(*regressionTestFns[regressionTestI])(0);

		testSystem.compile();
		testSystem.simulate();

		// CREATE the fake data
		for( i=0; i<testSystem.eCount(); i++ ) {
			testSystem[i]->measuredCreateFake( 1000, Aode_noise );
		}
		startSystem.copy( testSystem );

		// PUT the system into a perturbed state
		(*setupCallback)(1);
		testSystem.simulate();

		// LAUNCH the fitter	
		testFitter = new KineticFitter( testSystem );
		testFitter->fitThreadMonitorStep = 0;
		testFitter->fitThreadSimulateEachStep = 1;
		testFitter->threadStart();

		ZUI *z = ZUI::zuiFindByName( "aodeFitterPanel" );
		z->putP( "fitter", testFitter );
		z->putP( "*fitter", testFitter );
	}
}


// regression setup
// each test knows if it should fail
// ability to jump to a spec test if it gerts wrtong ansewr

void startup() {
	zrandSeedFromTime();

#ifndef REGESSION
	// SETUP a test case (model and experiments)
//	setupTest0(0);
//	setupTest1(0);
//	setupTest2(0);
//	setupTest3(0);
//	setupTest4(0);
//	setupTest5(0);
//	setupTest6(0);
//	setupTest7(0);
//	setupTest8(0);  // This is correctly not going to fit well. The test moves an ICs that it doesn't allow it to fit and so of course you can't get a good fit
//	setupTest9(0);	// Simpelset case A->B
//	setupTest10(0);
//	setupTest11(0);
//	setupTest12(0);
//	setupTest13(0);
//	setupTest14(0);    // "epsp"
//	setupTest15(0);    // "epsp"
//	setupTest16(0);    // "irreversible"
//	setupTest17(0);    // No fit
//	setupTest18(0);    // Daivd Zhang's work FAILING
//	setupTest19(0);    // Dimerization
//	setupTest20(0);    // test of complex expressions
//	setupTest21(0);    // fitting observable constant
//	setupTest22(0);    // broken, using 24 to test
//	setupTest23(0);    // testing
//	setupTest24(0);    // testing time references in observable expressions
//	setupTest25(0);    // testing the identity observable expressions
//	setupTest26(0);    // noise averaging test
	setupTest27(0);    // equilibiurm experiment (@TODO)

	startSystem.copy( testSystem );
	testSystem.simulate();
#endif

	ZUI::zuiExecuteFile( "_aode/aode.zui" );
}

void shutdown() {
	testSystem.clear();
	startSystem.clear();
	twiddleSystem.clear();
	if( testFitter ) {
		delete testFitter;
		testFitter = 0;
	}
}

void handleMsg( ZMsg *msg ) {
	int i;

	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,a) ) {
		zMsgQueue( "type=ZUISet key=hidden toggle=1 toZUI=aodeFitterPanel" );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		testSystem.simulate();
	}
	
	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		for( i=0; i<testSystem.eCount(); i++ ) {
			testSystem[i]->measuredCreateFake( 1000, Aode_noise );
		}
		startSystem.copy( testSystem );
		ZUI::dirtyAll();
	}

	if( zmsgIs(type,Key) && zmsgIs(which,e) ) {
		testSystem.copy( startSystem );
		testSystem.compile();
		(*setupCallback)(1);
		testSystem.simulate();
		ZUI::dirtyAll();
	}

	if( zmsgIs(type,Key) && zmsgIs(which,r) ) {
		if( testFitter ) {
			delete testFitter;
		}
		testFitter = new KineticFitter( testSystem );
		testFitter->fitThreadMonitorStep = 1;
		testFitter->fitThreadStepFlag = 1;
		testFitter->fitThreadSimulateEachStep = 1;
		testFitter->threadStart();

		zMsgQueue( "type=ZUISet key=fitter val=0x%x toZUI=aodeFitterPanel", testFitter );
		zMsgQueue( "type=ZUISet key=*fitter val=0x%x toZUI=aodeFitterPanel", testFitter );
	}
	
	if( zmsgIs(type,Key) && zmsgIs(which,t) ) {
		testFitter->fitThreadStepFlag = 1;
	}

	if( zmsgIs(type,Key) && zmsgIs(which,T) ) {
		testFitter->fitThreadStepFlag = 1;
		testFitter->fitThreadMonitorStep = 0;
	}

	if( zmsgIs(type,Key) && zmsgIs(which,y) ) {
		testFitter->qHistory.saveCSV( "qhistory.csv" );
		testFitter->eHistory.saveCSV( "ehistory.csv" );
		testFitter->stateHistory.saveCSV( "statehistory.csv" );
	}

	if( zmsgIs(type,FitterStep) ) {
		ZUI *z = ZUI::zuiFindByName( zmsgS(fromZUI) );
		if( z ) {
			KineticFitter *fitter = (KineticFitter *)z->getI( "fitter" );
			fitter->fitThreadStepFlag = 1;
		}
	}

	if( zmsgIs(type,FitterRun) ) {
		ZUI *z = ZUI::zuiFindByName( zmsgS(fromZUI) );
		if( z ) {
			KineticFitter *fitter = (KineticFitter *)z->getI( "fitter" );
			fitter->fitThreadStepFlag = 1;
			fitter->fitThreadMonitorStep = 0;
		}
	}

	if( zmsgIs(type,FitterEigenTwiddle) ) {
		KineticFitter *fitter = (KineticFitter *)zmsgI(fitter);
		twiddleSystem.copy( *fitter );
		twiddleSystem.compile();

		ZMat tweakedSignal( fitter->origSignal );
		tweakedSignal.putD( zmsgI(param), 0, (double)zmsgF(value) + (double)zmsgF(deltaValue) );

		// CONVERT from eigen space to parameter space
		ZMat qVec;
		zmatMul( fitter->origVtt, tweakedSignal, qVec );
		twiddleSystem.putQ( qVec );

		twiddleSystem.simulate();
	}

	if( zmsgIs(type,FitterEigenTwiddleStop) ) {
		twiddleSystem.clear();
	}

}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;



