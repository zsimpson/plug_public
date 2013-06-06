// @ZBS {
//		*SDK_DEPENDS opencv2
// }

// OPERATING SYSTEM specific includes:
#include "wingl.h"
#include "GL/gl.h"
#include "GL/glu.h"
// SDK includes:
#include "cv.h"
#include "highgui.h"
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
#include "zbittexcache.h"
#include "zrand.h"
#include "zmousemsg.h"
#include "ztime.h"
#include "zgltools.h"
#include "ztl.h"
#include "zmathtools.h"
#include "zglfont.h"

ZPLUGIN_BEGIN( cellmove );

using namespace cv;

ZVAR( int, Cellmove_viewFrame, 0 );
ZVAR( int, Cellmove_erase, 90 );
ZVAR( int, Cellmove_x, 0 );
ZVAR( int, Cellmove_y, 0 );
ZVAR( int, Cellmove_viewCellId, 0x415 );
ZVAR( float, Cellmove_lineThickness, 4.4e-2 );
ZVAR( float, Cellmove_nodeSize, 2.67e-2 );
ZVAR( float, Cellmove_singleLikely, 500 );
ZVAR( float, Cellmove_aboveMed, 10.f );
ZVAR( int, Cellmove_showStart, 1 );
ZVAR( int, Cellmove_drawIds, 1 );
ZVAR( int, Cellmove_viewGraphSpacing, 22 );


ZHashTable options;

#define FRAME_COUNT (25)
#define MAX_CELLS_PER_FRAME (4000)

Mat origMats[FRAME_COUNT];
Mat labeledMats[FRAME_COUNT];
Mat flowXMats[FRAME_COUNT];
Mat flowYMats[FRAME_COUNT];

int w, h;


// After a merge / split, I'll need to be able to re-run the cellsAlign for cells in a given area
// And will also need to run cellsCluster on all cells that previously started the neighborhood
// All the graph manipulation stuff should be separated.

#define matGet( m, y, x ) ((double *)m.ptr(y))[x]
#define matSet( m, y, x, v ) ((double *)m.ptr(y))[x] = v

struct FrameId {
	unsigned short id;
	unsigned short frame;

	FrameId() {
	}

	FrameId( unsigned short _frame, unsigned short _id ) {
		frame = _frame;
		id = _id;
	}
	
	bool equals( FrameId f ) {
		return frame == f.frame && id == f.id;
	}
};

struct Point {
	unsigned short x;
	unsigned short y;

	Point( unsigned short _x, unsigned short _y ) {
		x = _x;
		y = _y;
	}
};

struct Vote {
	FrameId frameId;
	int strength;

	Vote( int f, int id, int str ) {
		frameId = FrameId( f, id );
		strength = str;
	}
};

struct Cell {
	FrameId frameId;
	int startCell;
	IVec2 mean;
	FrameId mergedWith;
	ZTLVec<Point> pixels;	
	
	void init( int f, int id ) {
		frameId.frame = f;
		frameId.id = id;
		startCell = 0;
		mean = IVec2( 0, 0 );
		mergedWith = FrameId( 0, 0 );
	}
	
	bool valid() {
		return mergedWith.id == 0 && mergedWith.frame == 0;
	}
	
	int area() {
		return pixels.count;
	}

	void addPixel( int x, int y ) {
		pixels.add( Point( x, y ) );
	}
	
	void computeMean() {
		mean = IVec2( 0, 0 );
		for( int i=0; i<pixels.count; i++ ) {
			mean.x += pixels[i].x;
			mean.y += pixels[i].y;
		}
		if( pixels.count > 0 ) {
			mean.x /= pixels.count;
			mean.y /= pixels.count;
		}
	}

	void mergePixels( Cell *other ) {
		for( int i=0; i<other->pixels.count; i++ ) {
			pixels.add( other->pixels[i] );
		}
		computeMean();
		other->pixels.clear();
		other->mergedWith = frameId;
	}
	
	void save( FILE *file ) {
		fwrite( &frameId, sizeof(frameId), 1, file );
		fwrite( &startCell, sizeof(startCell), 1, file );
		fwrite( &mean, sizeof(mean), 1, file );
		fwrite( &pixels.count, sizeof(pixels.count), 1, file );
		fwrite( pixels.vec, pixels.count * sizeof(Point), 1, file );
	}
	
	void load( FILE *file ) {
		fread( &frameId, sizeof(frameId), 1, file );
		fread( &startCell, sizeof(startCell), 1, file );
		fread( &mean, sizeof(mean), 1, file );
		int count;
		fread( &count, sizeof(count), 1, file );
		pixels.setCount( count );
		fread( pixels.vec, pixels.count * sizeof(Point), 1, file );
	}
	
};

struct Edge {
	FrameId from;
	FrameId to;
	
	Edge( FrameId from, FrameId to ) {
		this->from = from;
		this->to = to;
	}
};

struct Graph {
	ZHashTable edges;
	
	Graph() {
		edges.init( 100000 );
	}
	
	void addEdge( Edge edge, int weight ) {
		edges.bputI( &edge, sizeof(Edge), weight );
	}
	
	void removeEdge( Edge edge ) {
		edges.del( (char *)&edge, sizeof(Edge) );
	}
	
	void clear( int greaterThanOrEqualToFrame=0 ) {
		ZTLVec<Edge> toRemove;
		for( ZHashWalk e( edges ); e.next(); ) {
			if( ((Edge *)e.key)->from.frame >= greaterThanOrEqualToFrame ) {
				toRemove.add( *(Edge *)e.key );
			}
		}
		for( int i=0; i<toRemove.count; i++ ) {
			removeEdge( toRemove[i] );
		}
	}
	
	void parentsOf( FrameId fid, ZTLVec<FrameId> &list ) {
		for( ZHashWalk e( edges ); e.next(); ) {
			if( ((Edge *)e.key)->to.equals(fid) ) {
				list.add( ((Edge *)e.key)->from );
			}
		}
	}

	void childrenOf( FrameId fid, ZTLVec<FrameId> &list ) {
		for( ZHashWalk e( edges ); e.next(); ) {
			if( ((Edge *)e.key)->from.equals(fid) ) {
				list.add( ((Edge *)e.key)->to );
			}
		}
	}
	
	void hit( FrameId from, FrameId to, int amt=1 ) {
		Edge e( from, to );
		int curr = edges.bgeti( &e, sizeof(FrameId)*2 );
		edges.bputI( &e, sizeof(Edge), curr+amt );
	}
	
	void nodes( ZTLVec<FrameId> &list, int minFrame, int maxFrame ) {
		ZHashTable found;
		for( ZHashWalk e( edges ); e.next(); ) {
			found.bputI( &((Edge *)e.key)->from, sizeof(FrameId), 1 );
			found.bputI( &((Edge *)e.key)->to, sizeof(FrameId), 1 );
		}
		for( ZHashWalk e( found ); e.next(); ) {
			FrameId fid = *(FrameId *)e.key;
			if( fid.frame >= minFrame && fid.frame <= maxFrame ) {
				list.add( fid );
			}
		}
	}

	void merge( FrameId into, FrameId from ) {
		// FIND any edges to "from" and and give to into
		// FIND any edges from "from" and restate as hits from into
		ZTLVec<Edge> toRemove;
		for( ZHashWalk e( edges ); e.next(); ) {
			if( ((Edge *)e.key)->to.equals(from) ) {
				hit( ((Edge *)e.key)->from, into, get(*(Edge *)e.key) );
				toRemove.add( *(Edge *)e.key );
			}
			if( ((Edge *)e.key)->from.equals(from) ) {
				hit( into, ((Edge *)e.key)->to, get(*(Edge *)e.key) );
				toRemove.add( *(Edge *)e.key );
			}
		}

		for( int i=0; i<toRemove.count; i++ ) {
			removeEdge( toRemove[i] );
		}
	}
	
	int get( Edge edge ) {
		return edges.bgeti( (char *)&edge, sizeof(Edge) );
	}
	
	void extractSubgraph( FrameId node, Graph &subgraph, int minFrame, int maxFrame ) {
		// FLOODFILL to find the subgraph downstream of this node
		for( ZHashWalk e( edges ); e.next(); ) {
			if( ((Edge *)e.key)->from.equals(node) ) {
				Edge edge( node, ((Edge *)e.key)->to );
				int w = subgraph.get( edge );
				if( !w ) {
					subgraph.addEdge( edge, *(int *)e.val );
					if( ((Edge *)e.key)->to.frame >= minFrame && ((Edge *)e.key)->to.frame < maxFrame ) {
						extractSubgraph( ((Edge *)e.key)->to, subgraph, minFrame, maxFrame );
					}
				}
			}
		}
	}
	
	void save( FILE *file ) {
		zHashTableSave( edges, file );
	}
	
	void load( FILE *file ) {
		zHashTableLoad( file, edges );
	}
};

int cellCounts[FRAME_COUNT];
Cell cells[FRAME_COUNT][MAX_CELLS_PER_FRAME];

Cell *cell( FrameId frameId ) {
	return &cells[frameId.frame][frameId.id];
}

Cell *cell( int frame, int id ) {
	return &cells[frame][id];
}

Graph graph;


// Image analysis
//------------------------------------------------------------------------------------------------

void cellsConstruct() {
	for( int f=0; f<FRAME_COUNT; f++ ) {
		for( int i=0; i<MAX_CELLS_PER_FRAME; i++ ) {
			cell(f,i)->init( f, i );
		}
	}

	for( int f=0; f<FRAME_COUNT; f++ ) {
		cellCounts[f] = 0;
		
		// FOREACH pixel in this frame.  If it is non-zero then add to the pixelList
		for( int y=0; y<h; y++ ) {
			double *src = (double *)labeledMats[f].ptr( y );
			for( int x=0; x<w; x++ ) {
				int id = (int)*src;
				if( id != 0 ) {
					cell(f,id)->addPixel( x, y );
					cellCounts[f] = max( cellCounts[f], id+1 );
				}
				src++;
			}
		}
		
		for( int i=0; i<cellCounts[f]; i++ ) {
			cell(f,i)->computeMean();
		}
	}
}

IVec2 cellsAvgFlow( int f, Cell *cell ) {
	// AVERAGE the flow over the pixels in this cell group
	DVec2 d;
	for( int j=0; j<cell->pixels.count; j++ ) {
		int y = cell->pixels[j].y;
		int x = cell->pixels[j].x;
		if( x >= 0 && x < w && y >= 0 && y < h ) {
			d.x += matGet( flowXMats[f], y, x );
			d.y += matGet( flowYMats[f], y, x );
		}
	}
	if( cell->pixels.count > 0 ) {
		d.div( (double)cell->pixels.count );
	}
	return IVec2( int(d.x+0.5), int(d.y+0.5) );
}

void cellsAlign( int minFrame, int maxFrame ) {
	maxFrame = min( FRAME_COUNT, maxFrame );
	for( int f=minFrame; f<maxFrame-1; f++ ) {
		printf( "align: %d\n", f );
		
		for( int id=0; id<cellCounts[f]; id++ ) {
			Cell *c = cell(f,id);
		
			IVec2 avgFlow = cellsAvgFlow( f, c );
		
			for( int pix=0; pix<c->pixels.count; pix++ ) {
				int x = c->pixels[pix].x;
				int y = c->pixels[pix].y;
				
				int xx = x + avgFlow.x;
				int yy = y + avgFlow.y;

				if( yy >= 0 && yy < h && xx >= 0 && xx < w ) {
					int hit = (int)matGet( labeledMats[f+1], yy, xx );
					if( hit ) {
						graph.hit( FrameId(f,id), FrameId(f+1,hit) );
					}
				}
			}
		}
	}

	// REMOVE edges with less than 15% of the size of the hitter
	ZTLVec<Edge> toRemove;
	for( ZHashWalk e( graph.edges ); e.next(); ) {
		Edge *edge = (Edge *)e.key;
		int hits = *(int *)e.val;
		Cell *hitter = cell( edge->from );
		if( hits < 15 * hitter->area() / 100 ) {
			toRemove.add( Edge( edge->from, edge->to ) );
		}
	}
	
	for( int i=0; i<toRemove.count; i++ ) {
		graph.removeEdge( toRemove[i] );
	}
}


void cellsMerge( FrameId f0, FrameId f1 ) {
	assert( f0.frame == f1.frame );
	
	Cell *cell0 = cell(f0);
	Cell *cell1 = cell(f1);
	
	// MERGE the pixels of one into the other
	cell0->mergePixels( cell1 );
	
	// UPDATE the labeled bits
	for( int i=0; i<cell0->pixels.count; i++ ) {
		matSet( labeledMats[f0.frame], cell0->pixels[i].y, cell0->pixels[i].x, cell0->frameId.id );
	}
	
	// MERGE the graph
	graph.merge( f0, f1 );
}

Cell *cellsAdd( int frame ) {
	cells[frame][ cellCounts[frame] ].init( frame, cellCounts[frame] );
	cellCounts[frame]++;
	return &cells[frame][ cellCounts[frame]-1 ];
}

bool cellsSplit( FrameId toSplit ) {
	ZTLVec<FrameId> parents;
	graph.parentsOf( toSplit, parents );
	if( parents.count > 1 ) {
		for( int i=0; i<parents.count; i++ ) {
			Cell *c0 = cellsAdd( toSplit.frame );
			Cell *p0 = cell( parents[i] );

			assert( p0->frameId.frame == toSplit.frame-1 );

			// ASSIGN pixels from p0 to c0
			IVec2 avgFlow = cellsAvgFlow( p0->frameId.frame, p0 );
			for( int pix=0; pix<p0->pixels.count; pix++ ) {
				int x = p0->pixels[pix].x + avgFlow.x;
				int y = p0->pixels[pix].y + avgFlow.y;
				if( x >= 0 && x < w && y >= 0 && y < h ) {
					int hit = (int)matGet( labeledMats[toSplit.frame], y, x );
					if( hit == toSplit.id ) {
						c0->addPixel( x, y );
						matSet( labeledMats[toSplit.frame], y, x, c0->frameId.id );
					}
				}
			}
			
			// COMPUTE the hits from the new cells in this frame to the next
			if( c0->frameId.frame < FRAME_COUNT-1 ) {
				avgFlow = cellsAvgFlow( c0->frameId.frame, c0 );
				for( int pix=0; pix<c0->pixels.count; pix++ ) {
					int x = c0->pixels[pix].x + avgFlow.x;
					int y = c0->pixels[pix].y + avgFlow.y;
					if( x >= 0 && x < w && y >= 0 && y < h ) {
						int hit = (int)matGet( labeledMats[toSplit.frame+1], y, x );
						if( hit ) {
							graph.hit( c0->frameId, FrameId(toSplit.frame+1,hit) );
						}
					}
				}
			}		
			
			// REMOVE the edges from the parents to the toSplit
			graph.removeEdge( Edge( p0->frameId, toSplit ) );

			// ADD edges from p0, p1 to c0 and c1 respectively
			graph.addEdge( Edge( p0->frameId, c0->frameId ), p0->area() );
		}

		// REMOVE the edges toSplit to subsequent nodes
		ZTLVec<FrameId> children;
		graph.childrenOf( toSplit, children );
		for( int i=0; i<children.count; i++ ) {
			assert( cell( children[i] )->frameId.frame == cell(toSplit)->frameId.frame+1 );
			graph.removeEdge( Edge(toSplit,children[i]) );
		}

		// CLEAR the original
		cell( toSplit )->mergedWith = FrameId( 0xFFFF, 0xFFFF );
		cell( toSplit )->pixels.clear();
		
		return true;
	}
	return false;
}



// Graph processing
//---------------------------------------------------------------------------------------------------------

int frameCmp( const void *a, const void *b ) {
	return ((FrameId *)a)->frame - ((FrameId *)b)->frame;
	//return cell(*(FrameId *)b)->area() - cell(*(FrameId *)a)->area();
}

void graphProcess( FrameId fid, int minFrame, int maxFrame ) {
	maxFrame = min( maxFrame, FRAME_COUNT );

	int action;
	Graph subgraph;
	do {
		subgraph.clear();
		graph.extractSubgraph( fid, subgraph, minFrame, maxFrame );
		
		// COMPUTE the median cell size of the neighborhood
		float areas[4096];
		int areasCount = 0;

		ZTLVec<FrameId> nodes;
		subgraph.nodes( nodes, minFrame, maxFrame );
		
		qsort( nodes, nodes.count, sizeof(FrameId), frameCmp );
		
		for( int i=0; i<nodes.count; i++ ) {
			areas[areasCount++] = (float)cell( nodes[i] )->area();
		}

		assert( areasCount < 4096 );
		float mean, median, stddev;
		zmathStatsF( areas, 0, areasCount, &mean, &median, &stddev );

		action = false;

		for( int i=0; i<nodes.count; i++ ) {
			int area = cell( nodes[i] )->area();
			//printf( "examine: %08x (area=%d)\n", nodes[i], area );
			if( nodes[i].id == 0x606 ) {
				int a = 1;
			}
			if( area < 5000 ) {
				if( cell( nodes[i] )->valid() && area > 250 && (float)area > 1.4f * median ) {
					// Possible split
					//printf( "split: %08x (area=%d)\n", nodes[i], area );
					action = cellsSplit( nodes[i] );
				}
				else if( cell( nodes[i] )->valid() && (float)cell( nodes[i] )->area() < 0.8f * median ) {
					// Possible merge
					ZTLVec<FrameId> parents;
					subgraph.parentsOf( nodes[i], parents );
					if( parents.count == 1 ) {
						ZTLVec<FrameId> sibs;
						subgraph.childrenOf( parents[0], sibs );
						for( int k=0; k<sibs.count; k++ ) {
							int combinedArea = cell(sibs[k])->area() + cell(nodes[i])->area();
							if( cell(sibs[k])->valid() && !sibs[k].equals(nodes[i]) && combinedArea > 0.5f * median && combinedArea < 1.5f * median ) {
								// Found a sibling to merge with! Merge into the larger one
								if( cell(nodes[i])->area() > cell(sibs[k])->area() ) {
									//printf( "merge: %08x %08x\n", nodes[i], sibs[k] );
									cellsMerge( nodes[i], sibs[k] );
								}
								else {
									//printf( "merge: %08x %08x\n", sibs[k], nodes[i] );
									cellsMerge( sibs[k], nodes[i] );
								}
								action = true;
								break;
							}
						}
					}
				}
				if( action ) {
					break;
				}
			}
		}
		
		// BREAK links that account for less than 15% of the hitter
		ZTLVec<Edge> toRemove;
		for( ZHashWalk e( subgraph.edges ); e.next(); ) {
			Edge *edge = (Edge *)e.key;
			int hitterArea = cell(edge->from)->area();
			int hit = *(int*)e.val;
			if( hit < 25 * hitterArea / 100 ) {
				toRemove.add( *edge );
			}
		}
		for( int i=0; i<toRemove.count; i++ ) {
			action = true;
			subgraph.removeEdge( toRemove[i] );
			graph.removeEdge( toRemove[i] );
		}
	} while( action );

	subgraph.clear();
	graph.extractSubgraph( fid, subgraph, minFrame, maxFrame );

	// LOOK for a contiguous line in the graph
	FrameId foundLine[FRAME_COUNT];

	ZTLVec<FrameId> allNodes;
	subgraph.nodes( allNodes, 0, FRAME_COUNT );

	for( int i=0; i<allNodes.count; i++ ) {
		if( allNodes[i].frame == minFrame ) {

			FrameId curr = allNodes[i];
			foundLine[minFrame] = curr;

			int f;
			bool found = false;
			for( f=minFrame+1; f<maxFrame; f++ ) {
				foundLine[f] = curr;

				found = false;
				ZTLVec<FrameId> children;
				subgraph.childrenOf( curr, children );
				
				if( f==maxFrame-1 ) {
					found = true;
				}
				else if( children.count == 1 ) {
					ZTLVec<FrameId> parents;
					subgraph.parentsOf( children[0], parents );
					
					if( parents.count == 1 ) {
						found = true;
						curr = children[0];
					}
				}
				
				if( !found ) {
					break;
				}
			}
			
			if( f==maxFrame && found ) {
				// A contiguous line is found, mark all these
				int label = minFrame == 0 ? allNodes[i].id : cell(allNodes[i])->startCell;
				for( f=minFrame; f<maxFrame; f++ ) {
					cell(foundLine[f])->startCell = label;
				}
				break;
			}
		}
	}
}

void graphProcessAllCells() {
	int window = 10;
	for( int range=0; range<FRAME_COUNT; range+=window-4 ) {
		graph.clear();

		printf( "align: %d-%d\n", range, min( FRAME_COUNT-1, range+window ) );
		cellsAlign( range, range+window );

		for( int i=0; i<cellCounts[range]; i++ ) {
			printf( "process: %f\n", (float)i / (float)cellCounts[range] );
			graphProcess( FrameId(range,i), range, range+window );
		}
	}
}

/*
void graphProcessAllCells() {
	for( int i=0; i<cellCounts[0]; i++ ) {
		printf( "process: %f\n", (float)i / (float)cellCounts[0] );
		graphProcess( FrameId(0,i), 0, FRAME_COUNT );
	}
}
*/

void graphProcessOneCell() {
	graphProcess( FrameId(0,Cellmove_viewCellId), 0, FRAME_COUNT );
}


FrameId viewGraph[FRAME_COUNT][MAX_CELLS_PER_FRAME];
int viewGraphCount[FRAME_COUNT];

void createViewGraph( FrameId fid ) {
	Graph subgraph;
	graph.extractSubgraph( fid, subgraph, 0, FRAME_COUNT );
	
	// LAYOUT the nodes in frame order
	for(int f=0; f<FRAME_COUNT; f++ ) {
		viewGraphCount[f] = 0;

		for( ZHashWalk e( subgraph.edges ); e.next(); ) {
			Edge *edge = (Edge *)e.key;
			if( edge->from.frame == f ) {
				// If it isn't already on the row
				bool found = false;
				for( int i=0; i<viewGraphCount[f]; i++ ) {
					found = viewGraph[f][i].equals( edge->from );
					if( found ) {
						break;
					}
				}
				if( ! found ) {
					viewGraph[f][ viewGraphCount[f]++ ] = edge->from;
				}
			}
		}
	}
}

void renderViewGraph() {
	int SPREAD = Cellmove_viewGraphSpacing;

	glColor3ub( 255, 255, 255 );

	for( int f=0; f<FRAME_COUNT-1; f++ ) {	

		for( int i=0; i<viewGraphCount[f]; i++ ) {
			for( int j=0; j<viewGraphCount[f+1]; j++ ) {

				int strength = graph.get( Edge( viewGraph[f][i], viewGraph[f+1][j] ) );
				
				if( strength ) {
					
					float ii = (float)i * SPREAD;
					float jj = (float)j * SPREAD;
					float ff = (float)f * SPREAD;
					float fo = (float)(f+1) * SPREAD;
					
					glBegin( GL_QUADS );
						glVertex2f( ii, ff );
						glVertex2f( jj, fo );
						glVertex2f( jj+(float)strength*Cellmove_lineThickness, fo );
						glVertex2f( ii+(float)strength*Cellmove_lineThickness, ff );
					glEnd();
				}
			}
		}
	}
	
	for( int f=0; f<FRAME_COUNT; f++ ) {
		for( int i=0; i<viewGraphCount[f]; i++ ) {
			float size = max( 0.01f, Cellmove_nodeSize * cell( viewGraph[f][i] )->area() );
			if( f == Cellmove_viewFrame ) {
				glColor3ub( 255, 255, 0 );
			}
			else {
				glColor3ub( 255, 0, 0 );
			}
			if( !cell(viewGraph[f][i])->valid() ) {
				glColor3ub( 255, 0, 255 );
			}
			
			glPointSize( size );
			glBegin( GL_POINTS );
			glVertex2i( i*(int)SPREAD, f*(int)SPREAD );
			glEnd();
			glColor3ub( 0, 0, 0 );
			zglFontPrintInverted( ZTmpStr("%x",viewGraph[f][i].id), (float)i*SPREAD+1, (float)f*SPREAD+1, "controls" );
			glColor3ub( 0, 255, 0 );
			zglFontPrintInverted( ZTmpStr("%x",viewGraph[f][i].id), (float)i*SPREAD, (float)f*SPREAD, "controls" );
		}
	}
}


// Startup, shotdown, render
//-----------------------------------------------------------------------------------------------------


int *labeledTex[FRAME_COUNT] = { 0, };
int *chainTex[FRAME_COUNT] = { 0, };

Mat readRGBImageFile( char *filename ) {
	Mat img = cv::imread( filename );
	Mat dst( img.rows, img.cols, CV_8UC1 );
	for( int y=0; y<img.rows; y++ ) {
		unsigned char *s = img.ptr(y);
		unsigned char *d = dst.ptr(y);
		for( int x=0; x<img.cols; x++ ) {
			*d = *(s+1);
			s += 3;
			d ++;
		}
	}
	return dst;
}

Mat readZMatFile( char *filename ) {
	FILE *f = fopen( filename, "rb" );
	int w, h, d;
	fread( &h, sizeof(h), 1, f );
	fread( &w, sizeof(w), 1, f );
	fread( &d, sizeof(d), 1, f );
	unsigned char *buf = (unsigned char *)malloc( w * h * d );
	fread( buf, w * h * d, 1, f );
	fclose( f );
	Mat dst( h, w, CV_64F );
	memcpy( dst.ptr(), buf, w * h * d );
	free( buf );
	return dst;
}

void drawCellPixels( ZBits &bits, Cell *cell, int red, int blu ) {
	for( int i=0; i<cell->pixels.count; i++ ) {
		int y = cell->pixels[i].y;
		int x = cell->pixels[i].x;
		bits.ptrU1(x,y)[0] = red;
		bits.ptrU1(x,y)[2] = blu;
	}
}

static unsigned char *palette = 0;

void renderCells() {
	for( int i=0; i<FRAME_COUNT; i++ ) {
		if( chainTex[i] ) {
			zbitTexCacheFree( chainTex[i] );
		}
	}

	Cell *cell = &cells[Cellmove_viewFrame][Cellmove_viewCellId % cellCounts[Cellmove_viewFrame] ];
	for( int f=0; f<FRAME_COUNT; f++ ) {
		//printf( "%d\n", f );
		
		ZBits bits( w, h, 3 );
		bits.fill( 0 );

		// DRAW the original
		for( int y=0; y<h; y++ ) {
			unsigned char *dst = bits.lineU1(y);
			double *orig = (double *)origMats[f].ptr(y);
			for( int x=0; x<w; x++ ) {
				dst[0] = 0;
				dst[1] = (int)(*orig * 0.5);
				dst[2] = 0;
				orig++;
				dst += 3;
			}
		}
		
		// DRAW all the cells
		for( int i=0; i<cellCounts[f]; i++ ) {
			int palIdx = 0;
			Cell *cell = &cells[f][i];

			if( Cellmove_showStart) {
				palIdx = cell->startCell;
			}
			else {
				palIdx = zrandI( 1, 255 );
			}

			drawCellPixels( bits, &cells[f][i], palette[palIdx*3+0], palette[palIdx*3+2] );
		}

		// DRAW the marked cell
		if( f==Cellmove_viewFrame && cell ) {
			drawCellPixels( bits, cell, 255, 0 );
		}
		
		chainTex[f] = zbitTexCacheLoad( &bits, 0 );
	}
}

int *labledImage( Mat labeled, Mat orig, int frame ) {
	int w = labeled.cols;
	int h = labeled.rows;
	
	// @TODO: Scan to count actual number of groups in the labeled matrix
	ZBits bits( w, h, 3 );
	bits.fill( 0 );

	for( int y=0; y<h; y++ ) {
		unsigned char *dst = bits.lineU1(y);
		for( int x=0; x<w; x++ ) {
			int label = (int)matGet( labeled, y, x );
			int o = (int)matGet( orig, y, x );
			
			// LOOK in the alignment matrix to figure out what group this in in terms of frame 1
			//alignmentMat.ptr(label)[f];
			
			dst[0] = palette[ label*3 + 0 ];
			dst[1] = o;
			dst[2] = palette[ label*3 + 2 ];
			o++;
			dst += 3;
		}
	}

	return zbitTexCacheLoad( &bits, 1 );
}

void load() {
	FILE *file = fopen( "cells.bin", "rb" );
	int fc;
	fread( &fc, sizeof(fc), 1, file );
	assert( fc == FRAME_COUNT );
	fread( cellCounts, sizeof(cellCounts), 1, file );
	for( int f=0; f<FRAME_COUNT; f++ ) {
		for( int i=0; i<cellCounts[f]; i++ ) {
			cells[f][i].load( file );
		}
	}
	graph.load( file );
	fclose( file );
}

void save() {
	FILE *file = fopen( "cells.bin", "wb" );
	int fc = FRAME_COUNT;
	fwrite( &fc, sizeof(fc), 1, file );
	fwrite( cellCounts, sizeof(cellCounts), 1, file );
	for( int f=0; f<FRAME_COUNT; f++ ) {
		for( int i=0; i<cellCounts[f]; i++ ) {
			cells[f][i].save( file );
		}
	}
	graph.save( file );
	fclose( file );
}

int mouseX, mouseY;

void render() {
	int f = Cellmove_viewFrame = max( 0, min( FRAME_COUNT-1, Cellmove_viewFrame) );

	glScalef( 1.f / 1000.f, 1.f / 1000.f, 1.f );
	zviewpointSetupView();

	FVec3 mouse = zviewpointProjMouseOnZ0Plane();
	mouseX = max( 0, min( w-1, (int)(mouse.x) ) );
	mouseY = max( 0, min( h-1, (h-1) - (int)(mouse.y) ) );

	static int lastViewCellId = 0;
	if( Cellmove_viewCellId != lastViewCellId ) {
		lastViewCellId = Cellmove_viewCellId;
		renderCells();
	}

	glColor3ub( 255, 255, 255 );
	zbitTexCacheRender( chainTex[f] );

	int viewporti[4];
	DMat4 modelMat, projMat;
	glGetIntegerv( GL_VIEWPORT, viewporti );
	glGetDoublev( GL_MODELVIEW_MATRIX, modelMat );
	glGetDoublev( GL_PROJECTION_MATRIX, projMat );

	FVec2 projected[MAX_CELLS_PER_FRAME];
	bool valid[MAX_CELLS_PER_FRAME];
	
	for( int i=0; i<cellCounts[Cellmove_viewFrame]; i++ ) {
		double x, y, z;
		Cell *cell = &cells[ Cellmove_viewFrame ][ i ];
		valid[i] = cell->valid();
		gluProject( cell->mean.x, (h-1)-cell->mean.y, 0.0, modelMat, projMat, viewporti, &x, &y, &z );
		projected[i] = FVec2( x, y );
	}

	if( Cellmove_drawIds ) {	
		zglPixelMatrixFirstQuadrant();

		for( int i=0; i<cellCounts[Cellmove_viewFrame]; i++ ) {
			if( _finite( projected[i].x ) && _finite( projected[i].y ) ) {
				if( valid[i] ) {
					glColor3ub( 255, 255, 255 );
				}
				else {
					glColor3ub( 255, 255, 0 );
				}
				zglFontPrint( ZTmpStr("%x",i), projected[i].x, projected[i].y, "controls" );
			}
		}
	}
	
	zglPixelMatrixInvertedFirstQuadrant();

	glTranslatef( 20.f, 20.f, 0.f );
	renderViewGraph();
}

void startup() {
	palette = (unsigned char *)malloc( 3 * 10000 );
	memset( palette, 0, 3 * 10000 );
	for( int i=0; i<10000; i++ ) {
		palette[i*3+0] = zrandI( 50, 100 );
		palette[i*3+2] = zrandI( 100, 255 );
	}
	palette[0*3+0] = 0;
	palette[0*3+1] = 0;
	palette[0*3+2] = 0;
	
	char *prefix = options.getS( "Cellmove_prefix" );

 	for( int f=0; f<FRAME_COUNT; f++ ) {
		printf( "load: %d\n", f );
	
		Mat origMat = readZMatFile( ZTmpStr("%s%02d-orig.zmat",prefix,f+1) );
		origMats[f] = origMat;
		w = origMat.cols;
		h = origMat.rows;

		Mat labeldMat = readZMatFile( ZTmpStr("%s%02d-labeled.zmat",prefix,f+1) );
		labeledMats[f] = labeldMat;
		labeledTex[f] = labledImage( labeldMat, origMat, f );

		Mat flowXMat = readZMatFile( ZTmpStr("%s%02d-flow_x.zmat",prefix,f+1) );
		flowXMats[f] = flowXMat;

		Mat flowYMat = readZMatFile( ZTmpStr("%s%02d-flow_y.zmat",prefix,f+1) );
		flowYMats[f] = flowYMat;
	}

	cellsConstruct();

	if( 1 ) {
		load();
	}
	else {
		cellsAlign( 0, FRAME_COUNT );
		save();
	}
}

void shutdown() {
}

void handleMsg( ZMsg *msg ) {
	zviewpointHandleMsg( msg );
	if( zMsgIsUsed() ) return;

	if( zmsgIs(type,ZUIMouseClickOn) && zmsgIs(dir,D) && zmsgIs(which,L) ) {
		Cellmove_viewCellId = (int)matGet( labeledMats[Cellmove_viewFrame], mouseY, mouseX );
		createViewGraph( FrameId( Cellmove_viewFrame, Cellmove_viewCellId ) );
		printf( "%08x area = %d\n", FrameId( Cellmove_viewFrame, Cellmove_viewCellId ), cell(FrameId( Cellmove_viewFrame, Cellmove_viewCellId ))->area() );
	}
	
	if( zmsgIs(type,Key) && zmsgIs(which,q) ) {
		graphProcess( FrameId(0,Cellmove_viewCellId), 0, FRAME_COUNT );
	}

	if( zmsgIs(type,Key) && zmsgIs(which,w) ) {
		graphProcessAllCells();
		save();
	}

	if( zmsgIs(type,Key) && zmsgIs(which,e) ) {
		renderCells();
	}

	if( zmsgIs(type,Key) && zmsgIs(which,r) ) {
		graphProcessOneCell();
	}

	if( zmsgIs(type,Key) && zmsgIs(which,a) ) {
		Cellmove_viewFrame--;
	}
	if( zmsgIs(type,Key) && zmsgIs(which,s) ) {
		Cellmove_viewFrame++;
	}
}

ZPLUGIN_EXPORT_PROPERTY( shadowGardenInterface, "1" );
ZPLUGIN_EXPORT_SYMBOL( startup );
ZPLUGIN_EXPORT_SYMBOL( shutdown );
ZPLUGIN_EXPORT_SYMBOL( render );
ZPLUGIN_EXPORT_SYMBOL( handleMsg );

ZPLUGIN_END;


