// @ZBS {
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
// STDLIB includes:
#include "assert.h"
// MODULE includes:
// ZBSLIB includes:
#include "zvars.h"
#include "zplugin.h"
#include "zmsg.h"
#include "zbits.h"
#include "zgraphfile.h"
#include "ztmpstr.h"
#include "zviewpoint.h"
#include "zmousemsg.h"
#include "ztime.h"
#include "zgltools.h"
#include "ztl.h"
#include "zmathtools.h"
#include "zglfont.h"
#include "zmat.h"
#include "nrrand.h"
#include "zhash2d.h"
#include "zwildcard.h"


extern ZHashTable options;

ZPLUGIN_BEGIN( cellgraph );

ZVAR( int, Cellgraph_viewFrame, 0 );
ZVAR( int, Cellgraph_alignSteps, 10000 );
ZVAR( float, Cellgraph_threshold, 0.999 );
ZVAR( float, Cellgraph_viewLineThickness, 1 );



#define FRAME_COUNT (76)
#define MAX_NODES (10000)
ZMat nodesMat[FRAME_COUNT];
ZMat edgesMat[FRAME_COUNT];
ZMat flowXMat[FRAME_COUNT];
ZMat flowYMat[FRAME_COUNT];
NRRandom random;

char *indexFilename = "nodeindex.dat";

int randi( int n ) {
	return int( random.randFastestInt64() % (S64)n );
}

// INDEX... For every frame I need a 2d hash of all the nodes and I need a list of all the outbound links from that node
struct Node : ZHash2DElement {
	double x, y;
	int id;
	int outEdgeCount;
	int outEdges[16];
	
	void addEdge( int e ) {
		if( outEdgeCount < 16 ) {
			outEdges[ outEdgeCount ] = e;
			outEdgeCount++;
		}
	}
};

struct Score {
	int node2;
	float score;
};

int nodeCount[FRAME_COUNT];
Node nodes[FRAME_COUNT][MAX_NODES];
ZHash2D nodesHash[FRAME_COUNT];
Score scores[FRAME_COUNT][MAX_NODES];

void addScore( int frame1, int node1, int node2, double score ) {
//	static int hack = 0;
//	if( hack ) {
//		return;
//	}
//	hack = 1;

	scores[frame1][node1].node2 = node2;
	scores[frame1][node1].score = (float)score;
}

void createHashes() {
	// FIND the boundaries
	double xMax = 0.0;
	double yMax = 0.0;
	for( int f=0; f<FRAME_COUNT; f++ ) {
		for( int i=0; i<nodesMat[f].rows; i++ ) {
			xMax = max( xMax, nodesMat[f].getD(i,0) );
			yMax = max( yMax, nodesMat[f].getD(i,1) );
		}
	}

	// CREATE the hashes
	for( int f=0; f<FRAME_COUNT; f++ ) {
		nodesHash[f].init( 256, 256, 0.f, (float)xMax, 0.f, (float)yMax );
	}
}

void buildIndex() {
	createHashes();

	// LOAD the nodes and hashes
	for( int f=0; f<FRAME_COUNT; f++ ) {
		printf( "%d\n", f );
		nodeCount[f] = nodesMat[f].rows;
		assert( nodeCount[f] < MAX_NODES );
		for( int i=0; i<nodeCount[f]; i++ ) {
			for( int e=0; e<edgesMat[f].rows; e++ ) {
				if( (int)edgesMat[f].getD(e,0) == i ) {
					nodes[f][i].addEdge( (int)edgesMat[f].getD(e,1) );
				}
				else if( (int)edgesMat[f].getD(e,1) == i ) {
					nodes[f][i].addEdge( (int)edgesMat[f].getD(e,0) );
				}
			}
		}
	}
	
	FILE *f = fopen( indexFilename, "wb" );
	fwrite( nodeCount, sizeof(int) * FRAME_COUNT, 1, f );
	fwrite( nodes, sizeof(Node) * FRAME_COUNT * MAX_NODES, 1, f );
	fclose( f );
}

void loadIndex() {
	FILE *f = fopen( indexFilename, "rb" );
	fread( nodeCount, sizeof(int) * FRAME_COUNT, 1, f );
	fread( nodes, sizeof(Node) * FRAME_COUNT * MAX_NODES, 1, f );
	fclose( f );

	createHashes();

	for( int f=0; f<FRAME_COUNT; f++ ) {
		for( int i=0; i<nodeCount[f]; i++ ) {
			nodes[f][i].x = nodesMat[f].getD(i,0);
			nodes[f][i].y = nodesMat[f].getD(i,1);
			nodes[f][i].id = i;
			nodesHash[f].insert( &nodes[f][i], (float)nodes[f][i].x, (float)nodes[f][i].y );
		}
	}
}

void alignNode( double threshold ) {
	// Each run of this function simply picks a pair nodes between two adjacent frames
	// and scores them based on all available evidence.
	// Evidence includes: are they where you'd expect given the flow? (Easy)
	// Do they have similar arms? (Easy)
	// Are they connected to resolved partners? (Harder)
	
	int frame1 = Cellgraph_viewFrame;//randi( FRAME_COUNT );
	int frame2 = frame1 + 1;
	if( frame1 < FRAME_COUNT-1 ) {
		int node1 = randi( nodesMat[frame1].rows );
		int node2 = -1;

		// FIND the candidate nodes nearby node1 in frame2
		double x = nodesMat[frame1].getD(node1,0);
		double y = nodesMat[frame1].getD(node1,1);
		int candidateCount = 0;
		int candidates[16];
		for( ZHash2DEnum b( &nodesHash[frame2], (float)x, (float)y, 3, 3 ); b.next(); ) {
			ZHash2DElement *be = b.get();
			if( candidateCount < 16 ) {
				candidates[candidateCount++] = ((Node *)be)->id;
			}
		}
		if( candidateCount > 0 ) {
			node2 = candidates[ randi(candidateCount) ];
		}
		
		if( node2 != -1 ) {
			// COMPARE frame1, node1 with frame2, node2
			// find every link in frame1 from node1 and then find the closest match in angle and length from frame2, node2
			
			DVec2 distFromPredicted( nodes[frame1][node1].x, nodes[frame1][node1].y );
			double flowX = flowXMat[frame1].getD( (int)nodes[frame1][node1].y, (int)nodes[frame1][node1].x );
			double flowY = flowYMat[frame1].getD( (int)nodes[frame1][node1].y, (int)nodes[frame1][node1].x );
			distFromPredicted.x += flowX;
			distFromPredicted.y += flowY;
			distFromPredicted.sub( DVec2( nodes[frame2][node2].x, nodes[frame2][node2].y ) );
			double score = 10.f * exp( - distFromPredicted.mag2() );

			DVec2 f1n1( nodes[frame1][node1].x, nodes[frame1][node1].y );
			DVec2 f2n2( nodes[frame2][node2].x, nodes[frame2][node2].y );
			
			// FOR EACH edge from frame1, node1
			for( int i=0; i<nodes[frame1][node1].outEdgeCount; i++ ) {
				double best = 0.0;
				int link1 = nodes[frame1][node1].outEdges[i];
				DVec2 f1n1Link( nodes[frame1][link1].x, nodes[frame1][link1].y );
				f1n1Link.sub( f1n1 );
				
				// FOR EACH link from frame2, node2
				for( int j=0; j<nodes[frame2][node2].outEdgeCount; j++ ) {
					int link2 = nodes[frame2][node2].outEdges[j];
					DVec2 f2n2Link( nodes[frame2][link2].x, nodes[frame2][link2].y );
					f2n2Link.sub( f2n2 );
					f2n2Link.sub( f1n1Link );
					double _score = exp( - f2n2Link.mag2() );
					best = max( _score, best );
				}
				score += best;
			}

			if( score > threshold ) {			
				addScore( frame1, node1, node2, score );
			}
		}
	}
}

void removeNodes() {
	// Remove nodes that have very low scores
}

void createLinks() {
	// CREATE a link if 
}

void readZMatFile( char *filename, ZMat &dst ) {
	FILE *f = fopen( filename, "rb" );
	int w, h, d;
	fread( &h, sizeof(h), 1, f );
	fread( &w, sizeof(w), 1, f );
	fread( &d, sizeof(d), 1, f );
	unsigned char *buf = (unsigned char *)malloc( w * h * d );
	fread( buf, w * h * d, 1, f );
	fclose( f );
	dst.alloc( h, w, zmatF64 );
	memcpy( dst.getPtrD(), buf, w * h * d );
	free( buf );
}

void render() {
//	for( int i=0; i<Cellgraph_alignSteps; i++ ) {
//		alignNode( Cellgraph_threshold );
//	}


	glScalef( 0.01f, -0.01f, 1.f );
	zviewpointSetupView();

	glPointSize( 2.f );
	glColor3ub( 255, 255, 0 );
	glBegin( GL_POINTS );
	for( int i=0; i<nodesMat[Cellgraph_viewFrame+1].rows; i++ ) {
		double x = nodesMat[Cellgraph_viewFrame+1].getD(i,0);
		double y = nodesMat[Cellgraph_viewFrame+1].getD(i,1);
		glVertex2d( x, y );
	}
	glEnd();
	
	glColor3ub( 0, 255, 255 );
	glBegin( GL_LINES );
	for( int i=0; i<edgesMat[Cellgraph_viewFrame+1].rows; i++ ) {
		int n1 = (int)edgesMat[Cellgraph_viewFrame+1].getD(i,0);
		int n2 = (int)edgesMat[Cellgraph_viewFrame+1].getD(i,1);
		glVertex2d( nodesMat[Cellgraph_viewFrame+1].getD(n1,0), nodesMat[Cellgraph_viewFrame+1].getD(n1,1) );
		glVertex2d( nodesMat[Cellgraph_viewFrame+1].getD(n2,0), nodesMat[Cellgraph_viewFrame+1].getD(n2,1) );
	}
	glEnd();


	glPointSize( 2.f );
	glColor3ub( 255, 0, 0 );
	glBegin( GL_POINTS );
	for( int i=0; i<nodesMat[Cellgraph_viewFrame].rows; i++ ) {
		double x = nodesMat[Cellgraph_viewFrame].getD(i,0);
		double y = nodesMat[Cellgraph_viewFrame].getD(i,1);
		glVertex2d( x, y );
	}
	glEnd();
	
	glColor3ub( 0, 0, 255 );
	glBegin( GL_LINES );
	for( int i=0; i<edgesMat[Cellgraph_viewFrame].rows; i++ ) {
		int n1 = (int)edgesMat[Cellgraph_viewFrame].getD(i,0);
		int n2 = (int)edgesMat[Cellgraph_viewFrame].getD(i,1);
		glVertex2d( nodesMat[Cellgraph_viewFrame].getD(n1,0), nodesMat[Cellgraph_viewFrame].getD(n1,1) );
		glVertex2d( nodesMat[Cellgraph_viewFrame].getD(n2,0), nodesMat[Cellgraph_viewFrame].getD(n2,1) );
	}
	glEnd();


	glLineWidth( 2.f );
	glColor3ub( 0, 0, 0 );
	glBegin( GL_LINES );
	for( int n1=1; n1<nodeCount[Cellgraph_viewFrame]; n1++ ) {
		int n2 = scores[Cellgraph_viewFrame][n1].node2;
		if( n2 > 0 && scores[Cellgraph_viewFrame][n1].score > 0.0f ) {
			glVertex2d( nodesMat[Cellgraph_viewFrame+0].getD(n1,0), nodesMat[Cellgraph_viewFrame+0].getD(n1,1) );
			glVertex2d( nodesMat[Cellgraph_viewFrame+1].getD(n2,0), nodesMat[Cellgraph_viewFrame+1].getD(n2,1) );
		}
	}
	glEnd();


/*
	glTranslatef( 0.f, 1000.f, 0.f );	

	glScalef( 50.f, 10.f, 1.f );	
	for( int f=0; f<FRAME_COUNT; f++ ) {
		glColor3ub( 255, 0, 0 );
		glBegin( GL_POINTS );
		for( int n=0; n<nodeCount[f]; n++ ) {
			glVertex2d( f, n );
		}
		glEnd();

		glColor3ub( 0, 0, 255 );
		for( int n=1; n<nodeCount[f]; n++ ) {
			if( scores[f][n].node2 > 0 && scores[f][n].score > 0.0f ) {
				glLineWidth( Cellgraph_viewLineThickness * scores[f][n].score );
				glBegin( GL_LINES );
				glVertex2d( f, n );
				glVertex2d( f+1, scores[f][n].node2 );
				glEnd();
			}
		}
	}
*/
}

void startup() {
	char *prefix = options.getS( "Cellgraph_prefix" );
	memset( scores, 0, sizeof(Score)*FRAME_COUNT*MAX_NODES );
	random.setSeed( 10938349048 );
 	for( int f=0; f<FRAME_COUNT; f++ ) {
		readZMatFile( ZTmpStr("%s%02d-nodes.zmat",prefix,f+1), nodesMat[f] );
		readZMatFile( ZTmpStr("%s%02d-edges.zmat",prefix,f+1), edgesMat[f] );
	}
	if( ! zWildcardFileExists( indexFilename ) ) {
		buildIndex();
	}
	loadIndex();
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
	}
	
	if( zmsgIs(type,Key) && zmsgIs(which,a) ) {
		Cellgraph_viewFrame = max( 0, Cellgraph_viewFrame-1 );
	}
	if( zmsgIs(type,Key) && zmsgIs(which,s) ) {
		Cellgraph_viewFrame = min( FRAME_COUNT-1, Cellgraph_viewFrame+1 );
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;


