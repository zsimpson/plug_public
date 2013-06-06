// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "string.h"
#include "math.h"
#include "stdlib.h"
#ifdef WIN32
#include "malloc.h"
#endif
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zmat.h"
#include "zrand.h"
#include "zbittexcache.h"
#include "zbits.h"
#include "zviewpoint.h"
#include "zgltools.h"
#include "ztl.h"
#include "zmathtools.h"
#include "zintegrator_gsl.h"
#include "zwildcard.h"
#include "ztmpstr.h"

ZPLUGIN_BEGIN( lnet );

ZVAR( int, Lnet_gateCount, 32 );
ZVAR( int, Lnet_xDim, 31 );
ZVAR( float, Lnet_alpha, 1.33e+000 );
ZVARB( double, Lnet_diff, 1.00e-001, 0.00e+000, 1.00e+000 );
ZVARB( double, Lnet_diffSD, 1.00e+000, 0.00e+000, 1.00e+000 );
ZVAR( double, Lnet_duration, 6.34e+000 );
ZVAR( int, Lnet_icWhich, 3 );
ZVAR( int, Lnet_diffWhich, 3 );
ZVAR( int, Lnet_cols, 11 );
ZVAR( double, Lnet_wThresh, 1.00e-001 );

ZVAR( double, Lnet_loThresh, 0.3 );
ZVAR( double, Lnet_hiThresh, 2.7 );
ZVAR( double, Lnet_slope, 15 );
ZVAR( double, Lnet_pullDown, 0.1 );
ZVAR( double, Lnet_ic, 3.0 );



double nand( double a, double b ) {
	a = 1.0 / ( 1.0 + exp( -15.0 * (a-0.5) ) );
	b = 1.0 / ( 1.0 + exp( -15.0 * (b-0.5) ) );
	return 1.0 - a*b;
}

double bandPass( double a ) {
	double lo = 1.0 / ( 1.0 + exp( -Lnet_slope * (a-Lnet_loThresh) ) );
	double hi = 1.0 / ( 1.0 + exp( -Lnet_slope * (a-Lnet_hiThresh) ) );
	
	return 3.0*(hi - lo)+3.0;
}

// I need a more geneirc system here so I can use this either for
// the interconnected model or for any hand built model such as the
// attempts at serpinski. I want to be able to overload for each kind
// the basic integration and storage and plotting should all stay the same
// as well as the IC storage, IC maker, and diffusion.
// Maybe IC and diffusion makers should be free funcitons? But they need to know
// number of variables, of state and space. No ok not free

// I really need a better matrix framework. Need it to work seemlessly with BLAS etc
// If not seemlessly at least without a lot of work
//
// Need it to be able to be growable by either row or col
// probably no way around that something growable is also going to have
// to be converted into one of the forms that BLAS will accept. Need to be able
// to convert these forms into the packed forms of BLAS
//
// Question, does everything get stuffed into one kind of class? Do I overload
// a virtualized base class with the different forms? Templatize? Problem with
// templatizing is it becomes hard to do make a simple base class that get pushed around
//
// Crap, maybe this is all just stupid. With plain old ZMat that is col
// major it is already the case that I can easily add cols because I'm col major
// and thus if I need things row growable I just transpose at the end
// There is a bit of an advantage to this union way of doing things wrt the ZMatElem constructors?
// No, the ZMatElem unoin doesn't really end up helping in the slightest
//
// No, wait, it is not the case that the old ZMat was efficient in adding columns
// because it was true that adding columns caused a complete realloc. If it happened
// to be that there was spaec in the heap after the add then it would be efficient but
// otherwise it isn't so there's still room for this. But still, don't think that ZMatElem adds anything

union ZMatElem {
	double d;
	int i;
	float f;
	struct {
		double reD;
		double imD;
	};
	struct {
		float reF;
		float imF;
	};
};

enum ZMatElemType { zmatD=0, zmatF, zmatI, zmatC, zmatZ };

struct ZMat2 {
	ZMatElemType type;
	int rows;
	int cols;
	char *m;
	static int elemSize[5];
	
	virtual void *get( int row, int col ) {
		assert( row >= 0 && row < rows && col >= 0 && col < cols );
		return (void*)&m[ (col*rows+row)*elemSize[type] ];
	}
	
	virtual void set( int row, int col, void *e ) {
		assert( row >= 0 && row < rows && col >= 0 && col < cols );
		int size = elemSize[type];
		char *src = (char *)e;
		char *dst = &m[ (col*rows+row)*size ];
		memcpy( dst, src, size );
	}
	
	double getD( int row, int col ) { assert( type == zmatD ); void *d = get( row, col ); return *(double *)d; }
	int getI( int row, int col ) { assert( type == zmatI ); void *i = get( row, col ); return *(int *)i; }
	void setD( int row, int col, double d ) { assert( type == zmatD ); set( row, col, &d ); }
	void setI( int row, int col, int i ) { assert( type == zmatI ); set( row, col, &i ); }
	// etc...s

	virtual void makeDense() { }

	void reverseOrdering() {
		// Reverses the col/row ordering. Be careful, caller is responsible for
		// tracking what order the values are in. All set and gets assume col major ordering
		int size = elemSize[ type ];
		char *newM = (char*)malloc( size * rows * cols );
		for( int r=0; r<rows; r++ ) {
			for( int c=0; c<cols; c++ ) {
				memcpy( &newM[ size*(r*cols+c) ], &m[ size*(c*rows+r) ], size );
			}
		}
		free( m );
		m = newM;
	}

	void allocDense( int rows, int cols, ZMatElemType type ) {
		clear();
		this->rows = rows;
		this->cols = cols;
		m = (char *)malloc( rows * cols * elemSize[type] );
		memset( m, 0, rows * cols * elemSize[type] );
	}

	virtual void clear() {
		if( m ) {
			free( m );
		}
		rows = 0;
		cols = 0;
		type = zmatD;
	}

	ZMat2() {
		rows = 0;
		cols = 0;
		m = 0;
		type = zmatD;
	}

	ZMat2( int rows, int cols, ZMatElemType type ) {
		this->rows = 0;
		this->cols = 0;
		this->m = 0;
		this->type = zmatD;
		allocDense( rows, cols, type );
	}

	virtual ~ZMat2() {
		clear();
	}
};

int ZMat2::elemSize[5] = { sizeof(double), sizeof(float), sizeof(int), sizeof(float)*2, sizeof(double)*2 };

struct ZMatGrowableRows : ZMat2 {
	// This matrix stores an array of pointers to rows instead of densely packing.
	// It can grow rows efficiently but not columns.
	int allocedRowPtrs;
		// The allocated size of the rowPtrs array 
	char **rowPtrs;
	
	void growIfNec( int row ) {
		// Grows the rowPtrs to include row if needed
		if( row > allocedRowPtrs ) {
			rowPtrs = (char**)realloc( rowPtrs, sizeof(char*)*(row+1) );
			for( int i=allocedRowPtrs; i<row; i++ ) {
				rowPtrs[i] = (char *)malloc( elemSize[type] * cols );
				memset( rowPtrs[i], 0, elemSize[type] * cols );
			}
			allocedRowPtrs = row+1;
		}
	}

	virtual void *get( int row, int col ) {
		assert( row >= 0 && row < rows && col >= 0 && col < cols );
		return (void *)&rowPtrs[row][ (col*rows+row)*elemSize[type] ];
	}
	
	virtual void set( int row, int col, void *e ) {
		assert( row >= 0 && row < rows && col >= 0 && col < cols );
		int size = elemSize[type];
		char *dst = &rowPtrs[row][ size*(col*rows+row) ];
		memcpy( dst, e, size );
	}
	
	void addRow( int row, ZMatElem *e ) {
		growIfNec( row );
		memcpy( rowPtrs[row], e, elemSize[type] * cols );
	}

	virtual void makeDense() {
		ZMat2::allocDense( rows, cols, type ); 
	}

	void init( int rows, int cols, ZMatElemType type ) {
		clear();
		growIfNec( rows * 2 );
		this->rows = rows;
		this->cols = cols;
	}

	virtual void clear() {
		for( int r=0; r<allocedRowPtrs; r++ ) {
			free( rowPtrs[r] );
		}
		free( rowPtrs );
		rowPtrs = 0;
		allocedRowPtrs = 0;
		ZMat2::clear();
	}

	ZMatGrowableRows() {
		allocedRowPtrs = 0;
		rowPtrs = 0;
		rows = 0;
		cols = 0;
		m = 0;
		type = zmatD;
	}

	ZMatGrowableRows( int rows, int cols, ZMatElemType type ) {
		this->allocedRowPtrs = 0;
		this->rowPtrs = 0;
		this->rows = 0;
		this->cols = 0;
		this->m = 0;
		this->type = zmatD;
		init( rows, cols, type );
	}

	virtual ~ZMatGrowableRows() {
		clear();
	}
};

struct ZMatSparse : ZMat2 {
};

struct ZMatTri : ZMat2 {
};


but for my tests i want something a little more matlabby?
i need threaded work and render so that I can think

void test() {
	ZMatGrowableRows md( 2, 2, zmatD );
	md.setD( 0, 0, 1.0 );
	md.setD( 1, 0, 2.0 );
	
	double d = md.getD( 1, 0 );

	ztexGroupFromZMat( md, scale );
		// Makes a nice displayable group of textures with scaling from the zmat

}

void update() {
	// do some work on md
	ztexGroupFromMat( md, scale );
}

void render() {
	ztexGroupRender( md.user );
}

ZMSG_HANDLER( Lnet_vizScale ) {
	ztexGroupFromMat( md, scale );
}




struct LNet1D {
	int gateCount;
	int xDim;
	ZMat w;
		// Because each gate has 2 inputs, this is a gateCount*2 x gateCount matrix
	ZMat ic;
		// A gateCount*2 x spaceDim vector
	ZMat diff;
		// A vector of diffusion rates (gateCount*2 by 1 vector)
	ZTLPVec<double> trace;

	typedef int (*CallbackDeriv)( double x, double *y, double *dYdx, void *params );

	static int adapator_compute_D0( double t, double *y, double *dYdx, void *params ) {
		LNet1D *_this = (LNet1D*)params;
		int xDim = _this->xDim;
		double *d = _this->diff.getPtrD();
		for( int x=0; x<xDim; x++ ) {
			//dYdx[x] = 0.0;
			dYdx[x] = bandPass( y[x] ) - y[x];
			//dYdx[x] = bandPass( y[x] ) - Lnet_pullDown * y[x];
			

			// DIFFUSION
			int xOffset0  = x;
			int xOffsetm1 = ((x+xDim-1)%xDim);
			int xOffsetp1 = ((x+xDim+1)%xDim);
			dYdx[x] += d[0] * ( y[xOffsetp1] - y[xOffset0] );
			dYdx[x] += d[0] * ( y[xOffsetm1] - y[xOffset0] );
		}
		return 0;
	}

	static int adapator_compute_D1( double t, double *y, double *dYdx, void *params ) {
		int i, j;

		LNet1D *_this = (LNet1D*)params;
		int gateCount = _this->gateCount;
		int xDim = _this->xDim;
		ZMat &w = _this->w;
		double *d = _this->diff.getPtrD();

		// COMPUTE sigmoid of the inputs to get the outputs
		double *outputs = (double *)alloca( sizeof(double) * gateCount );
		for( int x=0; x<xDim; x++ ) {
			int xOffset0  = x * gateCount * 2;
			int xOffsetm1 = ((x+xDim-1)%xDim) * gateCount * 2;
			int xOffsetp1 = ((x+xDim+1)%xDim) * gateCount * 2;

			for( i=0; i<gateCount; i++ ) {
				outputs[i] = nand( y[xOffset0+i*2+0], y[xOffset0+i*2+1] );
			}

			for( i=0; i<gateCount*2; i++ ) {
				double sum = 0.0;
				for( j=0; j<gateCount; j++ ) {
					sum += w.getD(j,i) * ( outputs[j] - y[xOffset0+i] );
				}

				// DIFFUSION
				sum += d[i] * ( y[xOffsetp1+i] - y[xOffset0+i] );
				sum += d[i] * ( y[xOffsetm1+i] - y[xOffset0+i] );
					// @TODO: Change from constant diffusion to per-reagent diffusion

				dYdx[xOffset0+i] = sum;
			}
		}
		return 0;
	}

	int systemDim() {
		return xDim;
	}

	void integrate( double duration ) {
		double largestIC = zmathMaxVecD( ic.getPtrD(), ic.rows * ic.cols );
		double endTime = duration;
		double errAbs = 1e-7*largestIC;
		double errRel = 1e-7;
		double startTime = 0.0;
		double initStep = ( endTime - startTime ) / 1000.0;
		double minStep  = ( endTime - startTime ) * 1e-10;
		int sysDim = systemDim();

		assert( ic.rows * ic.cols == sysDim );

		double *ics = ic.getPtrD();
		ZIntegratorGSLRK45 *zIntegrator = new ZIntegratorGSLRK45( 
			sysDim, ics, startTime, endTime, errAbs, errRel, initStep, minStep, 0,
			adapator_compute_D0, this
		);

		trace.clear();
		while( zIntegrator->evolve() == ZI_WORKING ) {
			double *v = new double[ sysDim + 1 ];
			v[0] = zIntegrator->x;
			memcpy( &v[1], zIntegrator->y, sizeof(double)*sysDim );
			trace.add( v );
		}
	}

	void init( int _gateCount ) {
		// ALLOCATE w and ic
		gateCount = _gateCount;
		w.alloc( gateCount, gateCount*2, zmatF64 );
	}

	void bistableNet() {
		init( 2 );
		w.putD( 0, 2, 1.0 );
		w.putD( 0, 3, 1.0 );
		w.putD( 1, 0, 1.0 );
		w.putD( 1, 1, 1.0 );
	}

	void ringOscillatorNet() {
		init( 3 );
		w.putD( 0, 4, 1.0 );
		w.putD( 0, 5, 1.0 );
		w.putD( 1, 0, 1.0 );
		w.putD( 1, 1, 1.0 );
		w.putD( 2, 2, 1.0 );
		w.putD( 2, 3, 1.0 );
	}
	
	void randomNet( int size, double alpha ) {
		init( size );
		for( int r=0; r<w.rows; r++ ) {
			for( int c=0; c<w.cols; c++ ) {
				double con = (double)zrandZipf(alpha) / 1000.0;
				con = max( 0.0, min( 1.0, con ) );
				w.putD( r, c, con );
			}
		}
	}

	void createICs( int _xDim, int which ) {
		xDim = _xDim;
		int i, x;
		ic.alloc( 1, xDim, zmatF64 );
		switch( which % 4 ) {
			case 0:
				// RANDOM, large values
				for( i=0; i<1; i++ ) {
					for( x=0; x<xDim; x++ ) {
						ic.putD( i, x, zrandF(0.f,1.f) );
					}
				}
				break;

			case 1:
				// RANDOM, small neutral values
				for( i=0; i<1; i++ ) {
					for( x=0; x<xDim; x++ ) {
						ic.putD( i, x, zrandF(0.49f,0.51f) );
					}
				}
				break;

			case 2:
				// ZERO except center has large random
				for( i=0; i<1; i++ ) {
					ic.putD( i, xDim/2, zrandF(0.f,1.f) );
				}
				break;

			case 3:
				// ZERO except center has large random
				for( x=0; x<xDim; x++ ) {
					ic.putD( 0, x, Lnet_ic );
				}
				ic.putD( 0, xDim/2+0, 0.f );
				break;
		}
	}
	
	void createDiff( int which ) {
		double d = 0.0;
		diff.alloc( 1, 1, zmatF64 );
		switch( which % 5 ) {
			case 0:
				// All reagents diffuse the same random diffusion
				d = zrandF( 0.f, 1.f );				
				for( int i=0; i<diff.rows; i++ ) {
					diff.setD( i, 0, d );
				}
				break;
			case 1:
				// All reagents have random diffusion
				for( int i=0; i<diff.rows; i++ ) {
					diff.setD( i, 0, zrandF(0.f,1.f) );
				}
				break;
			case 2:
				// A few reagents diffuse, others do not
				for( int i=0; i<diff.rows / 6; i++ ) {
					diff.setD( i, 0, 0.0 );
					if( i < diff.rows / 6 ) {
						diff.setD( i, 0, 1.0 );
					}
				}
				break;
			case 3:
				// All 1.0
				for( int i=0; i<diff.rows; i++ ) {
					diff.setD( i, 0, Lnet_diff );
				}
				break;
			case 4:
				// All 0.0 except 2
				for( int i=0; i<diff.rows; i++ ) {
					diff.setD( i, 0, 0.0 );
					if( i < 2 ) {
						diff.setD( i, 0, 1.0 );
					}
				}
				break;
		}
	}

	void save( char *filename ) {
		FILE *f = fopen( filename, "wb" );
		assert( f );
		fwrite( &gateCount, sizeof(gateCount), 1, f );
		fwrite( &xDim, sizeof(xDim), 1, f );
		
		int *wBuf = w.saveBin();
		fwrite( wBuf, *wBuf, 1, f );
		free( wBuf );
		int *icBuf = ic.saveBin();
		fwrite( icBuf, *icBuf, 1, f );
		free( icBuf );
		
		fclose( f );
	}

	void load( char *filename ) {
		FILE *f = fopen( filename, "rb" );
		assert( f );
		fread( &gateCount, sizeof(gateCount), 1, f );
		fread( &xDim, sizeof(xDim), 1, f );
		
		int wBufSize;
		fread( &wBufSize, sizeof(wBufSize), 1, f );
		int *wBuf = (int *)malloc( wBufSize );
		wBuf[0] = wBufSize;
		fread( &wBuf[1], wBufSize-sizeof(int), 1, f );
		w.loadBin( wBuf );
		free( wBuf );

		int icBufSize;
		fread( &icBufSize, sizeof(icBufSize), 1, f );
		int *icBuf = (int *)malloc( icBufSize );
		icBuf[0] = icBufSize;
		fread( &icBuf[1], icBufSize-sizeof(int), 1, f );
		ic.loadBin( icBuf );
		free( icBuf );

		fclose( f );
	}
};


struct LNetViz {
	// Time on the y axis, each gate on the x, tick
	// some number of times and accumulate into a graph

	ZTLVec<int*> tex;
	ZTLPVec<ZBits> bits;
	ZTLVec<double> times;

	LNetViz() {
	}

	~LNetViz() {
		bits.clear();
		freeTex();
	}

	void freeTex() {
		for( int i=0; i<tex.count; i++ ) {
			zbitTexCacheFree( tex[i] );
		}
		tex.clear();
	}

	void run( LNet1D *net, double duration ) {
		int i, x, k;

		net->integrate( duration );

		int gateCount = net->gateCount;
		int xDim = net->xDim;
		int ticks = net->trace.count;

		times.clear();

		bits.clear();
		for( i=0; i<1; i++ ) {
			ZBits *b = new ZBits();
			b->createCommon( xDim, ticks, ZBITS_COMMON_LUM_8 );
			bits.add( b );
		}

		for( i=0; i<ticks; i++ ) {

			double *v = net->trace[i];
			double t = v[0];
			times.add( t );
			v++;

			// COPY into the output viz
			for( x=0; x<xDim; x++ ) {
				int xOffset2 = x;
				for( k=0; k<1; k++ ) {
					//double in0 = v[xOffset2+k*2+0];
					//double in1 = v[xOffset2+k*2+1];
					double out = v[xOffset2] / 3.0;
					int view = max( 0, min( 255, (int)(out*255) ) );
					bits[k]->setU1( x, i, view );
				}
			}
		}

		// MAKE textures
		freeTex();
		for( k=0; k<1; k++ ) {
			int *t = zbitTexCacheLoad( bits[k], 1 );
			tex.add( t );
		}
	}

	void render() {
		glColor3ub( 0, 0, 255 );
		glBegin( GL_LINE_STRIP );
		for( int i=0; i<times.count; i++ ) {
			glVertex2f( (float)-times[i], (float)i );
		}
		glEnd();

		glColor3ub( 255, 255, 255 );
		for( int k=0; k<tex.count; k++ ) {
			int c = Lnet_cols;
			glPushMatrix();
				float x = (k%c) * (bits[0]->w() + 3.f);
				float y = (k/c) * (bits[0]->h() + 3.f);
				glTranslatef( x, y, 0.f );
				zbitTexCacheRender( tex[k] );
			glPopMatrix();
		}
	}

};

ZMat masterW;
LNet1D lNet1d;
LNetViz lNetViz;

void render() {
	glClearColor( 0.5f, 0.4f, 0.3f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT );
	glTranslatef( -1.f, -1.f, 0.0f );
	glScalef( 0.02f, 0.02f, 1.f );
	zviewpointSetupView();

	glBegin( GL_LINES );
		glVertex2f( 0.f, 0.f );
		glVertex2f( 3.f, 0.f );
	glEnd();
	glBegin( GL_LINE_STRIP );
	for( double a=0.0; a<=3.0; a+=0.01 ) {
		glVertex2d( a, bandPass(a) );
	}
	glEnd();


	glTranslatef( 0.f, 5.f, 0.f );
	lNet1d.createICs( Lnet_xDim, Lnet_icWhich );
	lNet1d.createDiff( Lnet_diffWhich );
	
	lNetViz.run( &lNet1d, Lnet_duration );

	lNetViz.render();

	

return;
/*
	lNetViz.render();

	// DRAW a histogram of the weights
	#define BUCKETS (100)
	int buckets[BUCKETS] = {0,};
	ZMat &w = lNet1d.w;
	for( int r=0; r<w.rows; r++ ) {
		for( int c=0; c<w.cols; c++ ) {
			int bucket = int( w.getD( r, c ) * (double)(BUCKETS-1) );
			assert( bucket >= 0 && bucket < BUCKETS );
			buckets[ bucket ]++;
		}
	}

	glColor3ub( 0, 0, 255 );
	glBegin( GL_LINES );
	for( int i=0; i<BUCKETS; i++ ) {
		glVertex2i( i, 0 );
		glVertex2i( i, -buckets[i] );
	}
	glEnd();
*/
}

void startup() {
test();
	zrandSeed( 1 );
	//lNet1d.ringOscillatorNet();
	//lNet1d.bistableNet();
	
//	lNet1d.createICs( Lnet_xDim, Lnet_icWhich );
//	lNet1d.createDiff( Lnet_diffWhich );

//	lNet1d.load( "_lnet/save000.lnet" );
//	lNetViz.run( &lNet1d, Lnet_duration );
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		lNetViz.run( &lNet1d, Lnet_duration );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		lNet1d.randomNet( Lnet_gateCount, Lnet_alpha );
		masterW.copy( lNet1d.w );
		if( lNet1d.ic.rows * lNet1d.ic.cols != lNet1d.systemDim() ) {
			lNet1d.createICs( Lnet_xDim, Lnet_icWhich );
		}
		if( lNet1d.diff.rows != lNet1d.gateCount*2 ) {
			lNet1d.createDiff( Lnet_diffWhich );
		}
		lNetViz.run( &lNet1d, Lnet_duration );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,e) ) {
		lNet1d.createICs( Lnet_xDim, Lnet_icWhich );
		lNetViz.run( &lNet1d, Lnet_duration );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,r) ) {
		lNet1d.createDiff( Lnet_diffWhich );
		lNetViz.run( &lNet1d, Lnet_duration );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,t) ) {
		ZMat w( masterW );
		for( int r=0; r<w.rows; r++ ) {
			for( int c=0; c<w.cols; c++ ) {
				double v = w.getD( r, c );
				if( v < Lnet_wThresh ) {
					w.putD( r, c, 0.0 );
				}
			}
		}
		lNet1d.w.copy( w );
		lNetViz.run( &lNet1d, Lnet_duration );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,y) ) {
		zrandSeed( 1 );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,s) ) {
		int i = zWildcardNextSerialNumber( "_lnet/save*.lnet" );
		ZTmpStr filename( "_lnet/save%03d.lnet", i );
		lNet1d.save( filename );
	}

//	if( zmsgIs(type,Key) && zmsgIs(which,t) ) {
//		tweakWeights( Lnet_weight );
//	}

}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;


